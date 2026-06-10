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


def write_compact_state(
    deck_dir: Path,
    deck_id: str,
    reviewed_count: int = 1,
    progress_day: int = 20000,
    introduced_indices: tuple[int, ...] = (0,),
    completed_today_indices: tuple[int, ...] | None = None,
    suspended_indices: tuple[int, ...] = (),
    schedules: tuple[tuple[int, int, int], ...] = (),
    version: int = 2,
) -> None:
    if completed_today_indices is None:
        completed_today_indices = introduced_indices

    rows = [
        f"version\t{version}",
        f"card_count\t{len(card_ids(deck_id))}",
        "current_index\t0",
        f"progress_day\t{progress_day}",
        f"reviewed_count\t{reviewed_count}",
        f"reviewed_today_count\t{reviewed_count}",
        f"introduced_count\t{len(introduced_indices)}",
        f"introduced_today_count\t{len(introduced_indices)}",
        f"completed_today_count\t{len(completed_today_indices)}",
        "again_count\t0",
        "hard_count\t0",
        f"good_count\t{reviewed_count}",
        "easy_count\t0",
        f"suspended_count\t{len(suspended_indices)}",
    ]
    rows.extend(f"introduced_index\t{index}" for index in introduced_indices)
    for card_index, due_day, interval_days in schedules:
        rows.append(f"schedule_index\t{card_index}")
        rows.append(f"schedule_due_day\t{due_day}")
        rows.append(f"schedule_interval_days\t{interval_days}")
    rows.extend(
        f"completed_today_index\t{index}" for index in completed_today_indices
    )
    rows.extend(f"suspended_index\t{index}" for index in suspended_indices)
    (deck_dir / "state.tsv").write_text(
        "\n".join(rows) + "\n",
        encoding="utf-8",
    )


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


def compact_review_log_row(
    event: str,
    rating: str = "-",
    card_index: int = 0,
    reviewed_count: int = 1,
    suspended_count: int = 0,
) -> str:
    return (
        f"1800000000\t{event}\t{rating}\t{card_index}\t"
        f"{reviewed_count}\t{suspended_count}"
    )


def write_compact_review_log(
    deck_dir: Path,
    events: list[str],
    reviewed_count: int = 1,
    suspended_count: int = 0,
) -> None:
    rows = []
    for index, event in enumerate(events):
        rating = "good" if event == "rating" else "-"
        rows.append(
            compact_review_log_row(
                event,
                rating,
                index % 2,
                reviewed_count,
                suspended_count,
            )
        )
    (deck_dir / "review-log.tsv").write_text(
        "\n".join(rows) + "\n",
        encoding="utf-8",
    )


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
    learning_mode: int | None = None,
) -> Path:
    deck_dir = sdmc / verify_m7_artifacts.APP_SD_DIR / "decks" / deck_id
    deck_dir.mkdir(parents=True)
    write_cards(deck_dir, deck_id)
    settings_rows = [
        f"new_limit\t{new_limit}",
        f"review_limit\t{review_limit}",
    ]
    if learning_mode is not None:
        settings_rows.append(f"learning_mode\t{learning_mode}")
    (deck_dir / "settings.tsv").write_text(
        "\n".join(settings_rows) + "\n",
        encoding="utf-8",
    )
    write_state(deck_dir, deck_id)
    write_review_log(deck_dir, deck_id, events)
    return deck_dir


def write_compact_deck(
    sdmc: Path,
    deck_id: str,
    events: list[str],
    new_limit: int = 7,
    review_limit: int = 9,
    reviewed_count: int = 1,
    progress_day: int = 20000,
    introduced_indices: tuple[int, ...] = (0,),
    suspended_indices: tuple[int, ...] = (),
    schedules: tuple[tuple[int, int, int], ...] = (),
    learning_mode: int | None = None,
) -> Path:
    deck_dir = sdmc / verify_m7_artifacts.APP_SD_DIR / "decks" / deck_id
    deck_dir.mkdir(parents=True)
    write_cards(deck_dir, deck_id)
    settings_rows = [
        f"new_limit\t{new_limit}",
        f"review_limit\t{review_limit}",
    ]
    if learning_mode is not None:
        settings_rows.append(f"learning_mode\t{learning_mode}")
    (deck_dir / "settings.tsv").write_text(
        "\n".join(settings_rows) + "\n",
        encoding="utf-8",
    )
    write_compact_state(
        deck_dir,
        deck_id,
        reviewed_count=reviewed_count,
        progress_day=progress_day,
        introduced_indices=introduced_indices,
        suspended_indices=suspended_indices,
        schedules=schedules,
    )
    write_compact_review_log(
        deck_dir,
        events,
        reviewed_count,
        len(suspended_indices),
    )
    return deck_dir


def write_session(
    sdmc: Path,
    started_at: int = 1800000000,
    updated_at: int = 1800000001,
    launch_count: int = 2,
    started_day: int = 20000,
    current_day: int = 20000,
    deck_count: int = 2,
    deck_open_count: int = 2,
    rating_saved_count: int = 1,
    undo_saved_count: int = 1,
    suspend_saved_count: int = 1,
    restore_saved_count: int = 1,
    settings_saved_count: int = 1,
    reset_progress_count: int = 1,
    answer_shown_count: int = 1,
    review_screen_count: int = 1,
    summary_screen_count: int = 1,
    load_error_count: int = 0,
    scan_completed: int = 1,
    exit_confirmed: int = 1,
    last_deck_id: str = "sample",
    last_event: str = "exit_confirmed",
) -> Path:
    app_dir = sdmc / verify_m7_artifacts.APP_SD_DIR
    app_dir.mkdir(parents=True, exist_ok=True)
    path = app_dir / "session.tsv"
    rows = [
        "#anki3ds-session-v1",
        f"started_at\t{started_at}",
        f"updated_at\t{updated_at}",
        f"launch_count\t{launch_count}",
        f"started_day\t{started_day}",
        f"current_day\t{current_day}",
        f"scan_completed\t{scan_completed}",
        f"deck_count\t{deck_count}",
        "ignored_count\t0",
        f"deck_open_count\t{deck_open_count}",
        f"review_screen_count\t{review_screen_count}",
        f"summary_screen_count\t{summary_screen_count}",
        f"load_error_count\t{load_error_count}",
        f"answer_shown_count\t{answer_shown_count}",
        f"rating_saved_count\t{rating_saved_count}",
        f"undo_saved_count\t{undo_saved_count}",
        f"suspend_saved_count\t{suspend_saved_count}",
        f"restore_saved_count\t{restore_saved_count}",
        f"settings_saved_count\t{settings_saved_count}",
        f"reset_progress_count\t{reset_progress_count}",
        f"exit_confirmed\t{exit_confirmed}",
        f"last_deck_id\t{last_deck_id}",
        f"last_event\t{last_event}",
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

    def test_accepts_compact_clean_shell_artifacts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_compact_deck(sdmc, "sample", ["rating", "undo"])
            write_compact_deck(
                sdmc,
                "limits-demo",
                ["suspend", "restore"],
                reviewed_count=0,
                introduced_indices=(),
                suspended_indices=(0,),
            )
            write_session(sdmc)

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertEqual(errors, [])

    def test_accepts_compact_clean_shell_schedule(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_compact_deck(
                sdmc,
                "sample",
                ["rating"],
                schedules=((0, 20004, 4),),
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertEqual(errors, [])

    def test_accepts_direct_control_smoke_artifact_shape(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_compact_deck(
                sdmc,
                "limits-demo",
                ["rating", "undo", "suspend", "restore"],
                new_limit=5,
                review_limit=10,
            )
            reset_dir = write_compact_deck(
                sdmc,
                "sample",
                ["rating"],
                new_limit=20,
                review_limit=200,
            )
            reset_deck_progress_files(reset_dir)
            write_session(
                sdmc,
                deck_count=2,
                deck_open_count=3,
                review_screen_count=3,
                summary_screen_count=0,
                rating_saved_count=2,
                reset_progress_count=1,
                answer_shown_count=3,
                last_deck_id="sample",
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["limits-demo"],
                REQUIRED_EVENTS,
                [("limits-demo", 5, 10), ("sample", 20, 200)],
                reset_deck_ids=["sample"],
            )

            self.assertEqual(errors, [])

    def test_accepts_expected_learning_mode(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_compact_deck(
                sdmc,
                "sample",
                ["rating"],
                learning_mode=1,
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9, 1)],
            )

            self.assertEqual(errors, [])

    def test_rejects_expected_learning_mode_mismatch(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_compact_deck(sdmc, "sample", ["rating"])
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9, 1)],
            )

            self.assertIn("sample/settings.tsv: expected learning_mode 1", errors)

    def test_rejects_bad_compact_state_counts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            deck_dir = write_compact_deck(sdmc, "sample", ["rating"])
            (deck_dir / "state.tsv").write_text(
                "version\t1\n"
                "card_count\t2\n"
                "current_index\t0\n"
                "reviewed_count\t2\n"
                "again_count\t1\n"
                "hard_count\t0\n"
                "good_count\t0\n"
                "easy_count\t0\n"
                "suspended_count\t0\n",
                encoding="utf-8",
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("compact state rating counts mismatch" in error for error in errors),
                errors,
            )

    def test_rejects_bad_compact_daily_state_counts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            deck_dir = write_compact_deck(sdmc, "sample", ["rating"])
            (deck_dir / "state.tsv").write_text(
                "version\t1\n"
                "card_count\t2\n"
                "current_index\t0\n"
                "progress_day\t20000\n"
                "reviewed_count\t1\n"
                "reviewed_today_count\t2\n"
                "introduced_count\t1\n"
                "introduced_today_count\t2\n"
                "introduced_index\t0\n"
                "again_count\t0\n"
                "hard_count\t0\n"
                "good_count\t1\n"
                "easy_count\t0\n"
                "suspended_count\t0\n",
                encoding="utf-8",
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("reviewed_today_count above total" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("introduced_today_count above total" in error for error in errors),
                errors,
            )

    def test_rejects_stale_compact_progress_day(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_compact_deck(sdmc, "sample", ["rating"])
            write_session(
                sdmc,
                current_day=20001,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("progress_day does not match session current_day" in error
                    for error in errors),
                errors,
            )

    def test_rejects_compact_progress_day_too_high(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            deck_dir = write_compact_deck(sdmc, "sample", ["rating"])
            write_compact_state(deck_dir, "sample", progress_day=1000001)
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("compact state progress_day too high" in error for error in errors),
                errors,
            )

    def test_rejects_bad_compact_review_log_row(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            deck_dir = write_compact_deck(sdmc, "sample", ["rating"])
            (deck_dir / "review-log.tsv").write_text(
                "1800000000\trating\t-\t9\t1\t0\n",
                encoding="utf-8",
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("rating event needs rating" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("bad card_index" in error for error in errors),
                errors,
            )

    def test_rejects_bad_compact_introduced_state(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            deck_dir = write_compact_deck(sdmc, "sample", ["rating"])
            (deck_dir / "state.tsv").write_text(
                "version\t1\n"
                "card_count\t2\n"
                "current_index\t0\n"
                "reviewed_count\t1\n"
                "introduced_count\t1\n"
                "introduced_index\t0\n"
                "introduced_index\t0\n"
                "again_count\t0\n"
                "hard_count\t0\n"
                "good_count\t1\n"
                "easy_count\t0\n"
                "suspended_count\t0\n",
                encoding="utf-8",
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("introduced_count mismatch" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("duplicate introduced_index" in error for error in errors),
                errors,
            )

    def test_rejects_bad_compact_completed_today_state(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            deck_dir = write_compact_deck(sdmc, "sample", ["rating"])
            (deck_dir / "state.tsv").write_text(
                "version\t1\n"
                "card_count\t2\n"
                "current_index\t0\n"
                "progress_day\t20000\n"
                "reviewed_count\t1\n"
                "reviewed_today_count\t1\n"
                "introduced_count\t1\n"
                "introduced_today_count\t1\n"
                "introduced_index\t0\n"
                "completed_today_count\t1\n"
                "completed_today_index\t1\n"
                "again_count\t0\n"
                "hard_count\t0\n"
                "good_count\t1\n"
                "easy_count\t0\n"
                "suspended_count\t0\n",
                encoding="utf-8",
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("completed_today_index not introduced" in error for error in errors),
                errors,
            )

    def test_rejects_bad_compact_schedule_state(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            deck_dir = write_compact_deck(sdmc, "sample", ["rating"])
            (deck_dir / "state.tsv").write_text(
                "version\t1\n"
                "card_count\t2\n"
                "current_index\t0\n"
                "progress_day\t20000\n"
                "reviewed_count\t1\n"
                "reviewed_today_count\t1\n"
                "introduced_count\t1\n"
                "introduced_today_count\t1\n"
                "introduced_index\t0\n"
                "schedule_index\t1\n"
                "schedule_due_day\t1000001\n"
                "schedule_index\t1\n"
                "schedule_due_day\t20004\n"
                "schedule_interval_days\t36501\n"
                "again_count\t0\n"
                "hard_count\t0\n"
                "good_count\t1\n"
                "easy_count\t0\n"
                "suspended_count\t0\n",
                encoding="utf-8",
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample"],
                ["rating"],
                [("sample", 7, 9)],
            )

            self.assertTrue(
                any("schedule requires version 2" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("schedule count mismatch" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("schedule_index not introduced" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("duplicate schedule_index" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("schedule_due_day too high" in error for error in errors),
                errors,
            )
            self.assertTrue(
                any("schedule_interval_days too high" in error for error in errors),
                errors,
            )

    def test_rejects_missing_expected_settings(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertIn(
                "settings expectations required; pass --expect-settings "
                "deck_id:new_limit:review_limit[:learning_mode] for the saved "
                "daily-limit edits",
                errors,
            )

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

    def test_rejects_session_counter_without_review_log_event(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            sample_dir = (
                sdmc
                / verify_m7_artifacts.APP_SD_DIR
                / "decks"
                / "sample"
            )
            write_review_log(sample_dir, "sample", ["undo"])

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                ["undo", "suspend", "restore"],
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertIn(
                "session.tsv: rating_saved_count exceeds review-log rating events",
                errors,
            )

    def test_rejects_session_counter_above_review_log_event_count(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, rating_saved_count=2, answer_shown_count=2)

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertIn(
                "session.tsv: rating_saved_count exceeds review-log rating events",
                errors,
            )

    def test_allow_missing_log_accepts_partial_log_skipped_event_counts(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, rating_saved_count=2, answer_shown_count=2)

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [("sample", 7, 9), ("limits-demo", 7, 9)],
                allow_missing_log=True,
            )

            self.assertNotIn(
                "session.tsv: rating_saved_count exceeds review-log rating events",
                errors,
            )
            self.assertEqual(errors, [])

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

    def test_rejects_backwards_session_time_and_day(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(
                sdmc,
                started_at=1800000100,
                updated_at=1800000000,
                started_day=20001,
                current_day=20000,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertIn("session.tsv: updated_at before started_at", errors)
            self.assertIn("session.tsv: current_day before started_day", errors)

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

    def test_rejects_session_from_unchecked_deck(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, last_deck_id="other-deck")

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [],
            )

            self.assertIn("session.tsv: last_deck_id outside checked decks", errors)

    def test_rejects_impossible_deck_open_outcome_counters(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(
                sdmc,
                deck_open_count=2,
                review_screen_count=2,
                summary_screen_count=1,
                load_error_count=0,
            )

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertIn(
                "session.tsv: deck-open outcome counters exceed deck_open_count",
                errors,
            )

    def test_rejects_session_not_ending_with_confirmed_exit(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, last_event="rating_saved")

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                REQUIRED_EVENTS,
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertIn(
                "session.tsv: last_event must prove confirmed exit",
                errors,
            )

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

    def test_rejects_rating_without_review_screen_evidence(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_valid_sdmc(Path(temp_dir))
            write_session(sdmc, review_screen_count=0)

            errors = verify_m7_artifacts.verify_m7_artifacts(
                sdmc,
                ["sample", "limits-demo"],
                ["rating"],
                [("sample", 7, 9), ("limits-demo", 7, 9)],
            )

            self.assertIn("session.tsv: missing review_screen_count", errors)

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

    def test_cli_accepts_expected_learning_mode(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = Path(temp_dir) / "sdmc"
            write_compact_deck(
                sdmc,
                "sample",
                ["rating"],
                learning_mode=1,
            )
            write_session(
                sdmc,
                deck_count=1,
                deck_open_count=1,
                undo_saved_count=0,
                suspend_saved_count=0,
                restore_saved_count=0,
                reset_progress_count=0,
                summary_screen_count=0,
            )

            with mock.patch(
                "sys.argv",
                [
                    "verify_m7_artifacts.py",
                    "--sdmc",
                    str(sdmc),
                    "--deck",
                    "sample",
                    "--require-event",
                    "rating",
                    "--expect-settings",
                    "sample:7:9:1",
                    "--quiet",
                ],
            ):
                exit_code = verify_m7_artifacts.main()

            self.assertEqual(exit_code, 0)

    def test_cli_rejects_bad_expected_learning_mode(self):
        stderr = io.StringIO()

        with mock.patch(
            "sys.argv",
            [
                "verify_m7_artifacts.py",
                "--expect-settings",
                "sample:7:9:2",
            ],
        ), redirect_stderr(stderr):
            with self.assertRaises(SystemExit) as raised:
                verify_m7_artifacts.main()

        self.assertEqual(raised.exception.code, 2)
        self.assertIn("learning_mode must be 0 or 1", stderr.getvalue())

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
                    "--expect-settings",
                    "sample:7:9",
                    "--expect-settings",
                    "limits-demo:7:9",
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
                    "--expect-settings",
                    "sample:7:9",
                    "--expect-settings",
                    "limits-demo:7:9",
                    "--quiet",
                ],
            ):
                exit_code = verify_m7_artifacts.main()

            self.assertEqual(exit_code, 0)


if __name__ == "__main__":
    unittest.main()
