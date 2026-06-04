import importlib.util
import io
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
VERIFY_M7_ARTIFACTS_PATH = ROOT / "tools" / "verify_m7_artifacts.py"
VERIFY_M7_ARTIFACTS_SPEC = importlib.util.spec_from_file_location(
    "verify_m7_artifacts",
    VERIFY_M7_ARTIFACTS_PATH,
)
verify_m7_artifacts = importlib.util.module_from_spec(VERIFY_M7_ARTIFACTS_SPEC)
assert VERIFY_M7_ARTIFACTS_SPEC.loader is not None
VERIFY_M7_ARTIFACTS_SPEC.loader.exec_module(verify_m7_artifacts)


REQUIRED_EVENTS = ["rating", "undo", "suspend", "restore"]


def card_ids(deck_id: str) -> list[str]:
    return [f"{deck_id}-card-1", f"{deck_id}-card-2"]


def write_cards(deck_dir: Path, deck_id: str) -> None:
    rows = []
    for index, card_id in enumerate(card_ids(deck_id), start=1):
        rows.append(
            f"{card_id}\t{deck_id}-note-{index}\tfront {index}\tback {index}\ttag"
        )
    (deck_dir / "cards.tsv").write_text("\n".join(rows) + "\n", encoding="utf-8")


def state_row(
    card_id: str,
    review_count: int,
    last_rating: int,
    due_day: int,
    interval_days: int,
    suspended: int,
    first_review_day: int,
    last_review_day: int,
) -> str:
    return (
        f"{card_id}\t{review_count}\t{last_rating}\t{due_day}\t"
        f"{interval_days}\t2500\t0\t{suspended}\t"
        f"{first_review_day}\t{last_review_day}"
    )


def write_state(deck_dir: Path, deck_id: str) -> None:
    ids = card_ids(deck_id)
    rows = [
        state_row(ids[0], 1, 2, 20001, 1, 0, 20000, 20000),
        state_row(ids[1], 0, 0, 20000, 0, 0, 0, 0),
    ]
    content = (
        "#anki3ds-state-v1\t2\n"
        + "\n".join(rows)
        + "\n#anki3ds-state-complete\t2\n"
    )
    (deck_dir / "state.tsv").write_text(content, encoding="utf-8")


def scheduler_snapshot(
    review_count: int,
    first_review_day: int,
    last_review_day: int,
    due_day: int,
    interval_days: int,
    suspended: int,
) -> list[str]:
    return [
        str(review_count),
        str(first_review_day),
        str(last_review_day),
        str(due_day),
        str(interval_days),
        "2500",
        "0",
        str(suspended),
    ]


def review_log_row(event: str, card_id: str, rating: str = "-") -> str:
    old = scheduler_snapshot(0, 0, 0, 20000, 0, 0)
    if event == "suspend":
        new = scheduler_snapshot(0, 0, 0, 20000, 0, 1)
    elif event == "restore":
        old = scheduler_snapshot(0, 0, 0, 20000, 0, 1)
        new = scheduler_snapshot(0, 0, 0, 20000, 0, 0)
    elif event == "undo":
        old = scheduler_snapshot(1, 20000, 20000, 20001, 1, 0)
        new = scheduler_snapshot(0, 0, 0, 20000, 0, 0)
    else:
        new = scheduler_snapshot(1, 20000, 20000, 20001, 1, 0)

    fields = [
        "1800000000",
        "20000",
        event,
        card_id,
        rating,
        *old,
        *new,
    ]
    return "\t".join(fields)


def write_review_log(deck_dir: Path, deck_id: str, events: list[str]) -> None:
    ids = card_ids(deck_id)
    rows = []
    for index, event in enumerate(events):
        rating = "good" if event == "rating" else "-"
        rows.append(review_log_row(event, ids[index % len(ids)], rating))
    (deck_dir / "review-log.tsv").write_text(
        "\n".join(rows) + "\n",
        encoding="utf-8",
    )


def write_deck(
    sdmc: Path,
    deck_id: str,
    events: list[str],
    new_limit: int = 7,
    review_limit: int = 9,
) -> Path:
    deck_dir = sdmc / verify_m7_artifacts.APP_SD_DIR / "decks" / deck_id
    deck_dir.mkdir(parents=True)
    write_cards(deck_dir, deck_id)
    (deck_dir / "settings.tsv").write_text(
        f"new_limit\t{new_limit}\nreview_limit\t{review_limit}\n",
        encoding="utf-8",
    )
    write_state(deck_dir, deck_id)
    write_review_log(deck_dir, deck_id, events)
    return deck_dir


class VerifyM7ArtifactsTests(unittest.TestCase):
    def write_valid_sdmc(self, root: Path) -> Path:
        sdmc = root / "sdmc"
        write_deck(sdmc, "sample", ["rating", "undo"])
        write_deck(sdmc, "limits-demo", ["suspend", "restore"])
        return sdmc

    def test_accepts_two_deck_m7_artifacts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertEqual(errors, [])

    def test_rejects_missing_state(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            state_path = (
                sdmc
                / verify_m7_artifacts.APP_SD_DIR
                / "decks"
                / "sample"
                / "state.tsv"
            )
            state_path.unlink()

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertTrue(any("state.tsv: missing" in error for error in errors), errors)

    def test_rejects_state_row_count_mismatch(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            state_path = (
                sdmc
                / verify_m7_artifacts.APP_SD_DIR
                / "decks"
                / "sample"
                / "state.tsv"
            )
            state_path.write_text(
                state_path.read_text(encoding="utf-8").replace(
                    "#anki3ds-state-v1\t2",
                    "#anki3ds-state-v1\t3",
                ),
                encoding="utf-8",
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertTrue(
                any("state row count mismatch" in error for error in errors),
                errors,
            )

    def test_rejects_missing_required_event(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_deck(sdmc, "sample", ["rating"])
            write_deck(sdmc, "limits-demo", ["suspend"])

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertTrue(
                any("missing required undo event" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("missing required restore event" in error for error in errors),
                errors,
            )

    def test_rejects_bad_settings_row(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            settings_path = (
                sdmc
                / verify_m7_artifacts.APP_SD_DIR
                / "decks"
                / "sample"
                / "settings.tsv"
            )
            settings_path.write_text(
                "new_limit\t7\textra\nreview_limit\t9\n",
                encoding="utf-8",
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertTrue(any("expected 2 fields" in error for error in errors), errors)
            self.assertTrue(any("missing new_limit" in error for error in errors), errors)

    def test_cli_reports_errors_without_traceback(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            state_path = (
                sdmc
                / verify_m7_artifacts.APP_SD_DIR
                / "decks"
                / "sample"
                / "state.tsv"
            )
            state_path.unlink()
            stderr = io.StringIO()

            with mock.patch(
                "sys.argv",
                [
                    "verify_m7_artifacts.py",
                    "--sdmc",
                    str(sdmc),
                    "--deck",
                    "sample",
                    "--deck",
                    "limits-demo",
                ],
            ), redirect_stderr(stderr):
                exit_code = verify_m7_artifacts.main()

            self.assertEqual(exit_code, 1)
            self.assertIn("state.tsv: missing", stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())

    def test_cli_rejects_unexpected_saved_settings(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            stderr = io.StringIO()

            with mock.patch(
                "sys.argv",
                [
                    "verify_m7_artifacts.py",
                    "--sdmc",
                    str(sdmc),
                    "--deck",
                    "sample",
                    "--deck",
                    "limits-demo",
                    "--expect-settings",
                    "sample:5:9",
                ],
            ), redirect_stderr(stderr):
                exit_code = verify_m7_artifacts.main()

            self.assertEqual(exit_code, 1)
            self.assertIn("sample/settings.tsv: expected new_limit 5", stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
