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
RESET_PROGRESS_FILES = (
    "state.tsv",
    "state.tsv.tmp",
    "state.tsv.bak",
    "review-log.tsv",
    "review-log.tsv.tmp",
    "review-log.tsv.bak",
)


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


def write_session(
    sdmc: Path,
    launch_count: int = 2,
    deck_count: int = 2,
    deck_open_count: int = 2,
    rating_saved_count: int = 1,
    undo_saved_count: int = 1,
    suspend_saved_count: int = 1,
    restore_saved_count: int = 1,
    settings_saved_count: int = 1,
    reset_progress_count: int = 1,
    answer_shown_count: int = 1,
    scan_completed: int = 1,
    exit_confirmed: int = 1,
) -> Path:
    app_dir = sdmc / verify_m7_artifacts.APP_SD_DIR
    app_dir.mkdir(parents=True, exist_ok=True)
    path = app_dir / "session.tsv"
    rows = [
        "#anki3ds-session-v1",
        "started_at\t1800000000",
        "updated_at\t1800000001",
        f"launch_count\t{launch_count}",
        "started_day\t20000",
        "current_day\t20000",
        f"scan_completed\t{scan_completed}",
        f"deck_count\t{deck_count}",
        "ignored_count\t0",
        f"deck_open_count\t{deck_open_count}",
        "review_screen_count\t1",
        "summary_screen_count\t1",
        "load_error_count\t0",
        f"answer_shown_count\t{answer_shown_count}",
        f"rating_saved_count\t{rating_saved_count}",
        f"undo_saved_count\t{undo_saved_count}",
        f"suspend_saved_count\t{suspend_saved_count}",
        f"restore_saved_count\t{restore_saved_count}",
        f"settings_saved_count\t{settings_saved_count}",
        f"reset_progress_count\t{reset_progress_count}",
        f"exit_confirmed\t{exit_confirmed}",
        "last_deck_id\tsample",
        "last_event\texit_confirmed",
        "#anki3ds-session-complete",
    ]
    path.write_text("\n".join(rows) + "\n", encoding="utf-8")
    return path


def reset_deck_progress_files(deck_dir: Path) -> None:
    for name in RESET_PROGRESS_FILES:
        path = deck_dir / name
        if path.exists():
            path.unlink()


class VerifyM7ArtifactsTests(unittest.TestCase):
    def write_valid_sdmc(self, root: Path) -> Path:
        sdmc = root / "sdmc"
        write_deck(sdmc, "sample", ["rating", "undo"])
        write_deck(sdmc, "limits-demo", ["suspend", "restore"])
        write_session(sdmc)
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
            write_session(sdmc)

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

    def test_rejects_missing_session(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            (sdmc / verify_m7_artifacts.APP_SD_DIR / "session.tsv").unlink()

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertTrue(any("session.tsv: missing" in error for error in errors), errors)

    def test_rejects_incomplete_session_evidence(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(
                sdmc,
                launch_count=1,
                deck_open_count=1,
                rating_saved_count=0,
                exit_confirmed=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                ["rating"],
                [],
            )

            self.assertIn("session.tsv: exit_confirmed must be 1", errors)
            self.assertIn("session.tsv: launch_count must prove relaunch", errors)
            self.assertIn(
                "session.tsv: deck_open_count below checked deck count",
                errors,
            )
            self.assertIn("session.tsv: missing rating_saved_count", errors)

    def test_rejects_rating_without_answer_reveal_evidence(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, answer_shown_count=0)

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                ["rating"],
                [],
            )

            self.assertIn("session.tsv: missing answer_shown_count", errors)
            self.assertIn(
                "session.tsv: answer_shown_count below rating_saved_count",
                errors,
            )

    def test_rejects_missing_settings_save_evidence(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, settings_saved_count=0)

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertIn("session.tsv: missing settings_saved_count", errors)

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

    def test_rejects_expected_settings_for_unselected_deck(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("limits-demo", 7, 9)],
            )

            self.assertTrue(
                any(
                    "limits-demo/settings.tsv: expected settings deck not selected"
                    in error
                    for error in errors
                ),
                errors,
            )

    def test_rejects_empty_deck_selection(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                [],
                [],
                [],
                reset_deck_ids=[],
            )

            self.assertIn("no decks selected for verification", errors)

    def test_cli_can_skip_required_events_for_log_skipped_case(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            for deck_id in ("sample", "limits-demo"):
                (
                    sdmc
                    / verify_m7_artifacts.APP_SD_DIR
                    / "decks"
                    / deck_id
                    / "review-log.tsv"
                ).unlink()

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
                    "--allow-missing-review-log",
                    "--no-required-events",
                    "--quiet",
                ],
            ):
                exit_code = verify_m7_artifacts.main()

            self.assertEqual(exit_code, 0)

    def test_cli_rejects_no_required_events_with_required_event(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            stderr = io.StringIO()

            with mock.patch(
                "sys.argv",
                [
                    "verify_m7_artifacts.py",
                    "--sdmc",
                    str(sdmc),
                    "--require-event",
                    "rating",
                    "--no-required-events",
                ],
            ), redirect_stderr(stderr):
                with self.assertRaises(SystemExit) as raised:
                    verify_m7_artifacts.main()

            self.assertEqual(raised.exception.code, 2)
            self.assertIn("cannot be combined", stderr.getvalue())

    def test_accepts_reset_deck_artifacts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            reset_dir = (
                sdmc
                / verify_m7_artifacts.APP_SD_DIR
                / "decks"
                / "sample"
            )
            reset_deck_progress_files(reset_dir)

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["limits-demo"],
                ["suspend", "restore"],
                [("sample", 7, 9), ("limits-demo", 7, 9)],
                reset_deck_ids=["sample"],
            )

            self.assertEqual(errors, [])

    def test_reset_deck_rejects_leftover_progress_files(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            reset_dir = (
                sdmc
                / verify_m7_artifacts.APP_SD_DIR
                / "decks"
                / "sample"
            )
            reset_deck_progress_files(reset_dir)
            (reset_dir / "state.tsv.bak").write_text("stale\n", encoding="utf-8")

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["limits-demo"],
                ["suspend", "restore"],
                [],
                reset_deck_ids=["sample"],
            )

            self.assertTrue(
                any("state.tsv.bak: must be absent after reset" in error for error in errors),
                errors,
            )

    def test_cli_rejects_study_and_reset_conflict(self):
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
                    "--expect-reset-deck",
                    "sample",
                    "--quiet",
                ],
            ), redirect_stderr(stderr):
                exit_code = verify_m7_artifacts.main()

            self.assertEqual(exit_code, 1)
            self.assertIn("sample: cannot be both study and reset deck", stderr.getvalue())

    def test_cli_can_verify_only_reset_decks(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, reset_progress_count=2)
            for deck_id in ("sample", "limits-demo"):
                reset_dir = (
                    sdmc
                    / verify_m7_artifacts.APP_SD_DIR
                    / "decks"
                    / deck_id
                )
                reset_deck_progress_files(reset_dir)

            with mock.patch(
                "sys.argv",
                [
                    "verify_m7_artifacts.py",
                    "--sdmc",
                    str(sdmc),
                    "--no-study-decks",
                    "--expect-reset-deck",
                    "sample",
                    "--expect-reset-deck",
                    "limits-demo",
                    "--no-required-events",
                    "--quiet",
                ],
            ):
                exit_code = verify_m7_artifacts.main()

            self.assertEqual(exit_code, 0)


if __name__ == "__main__":
    unittest.main()
