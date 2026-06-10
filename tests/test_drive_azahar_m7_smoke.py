import contextlib
import importlib.util
import io
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
DRIVER_PATH = ROOT / "tools" / "drive_azahar_m7_smoke.py"
DRIVER_SPEC = importlib.util.spec_from_file_location(
    "drive_azahar_m7_smoke",
    DRIVER_PATH,
)
drive_azahar_m7_smoke = importlib.util.module_from_spec(DRIVER_SPEC)
assert DRIVER_SPEC.loader is not None
sys.modules[DRIVER_SPEC.name] = drive_azahar_m7_smoke
DRIVER_SPEC.loader.exec_module(drive_azahar_m7_smoke)


class DriveAzaharM7SmokeTests(unittest.TestCase):
    def write_deck_root(self, root: Path, deck_ids: list[str]) -> Path:
        sdmc = root / "sdmc"
        deck_root = sdmc / drive_azahar_m7_smoke.DECK_ROOT
        deck_root.mkdir(parents=True)
        for deck_id in deck_ids:
            deck_dir = deck_root / deck_id
            deck_dir.mkdir()
            (deck_dir / "cards.tsv").write_text(
                f"{deck_id}-card\t{deck_id}-note\tfront\tback\ttag\n",
                encoding="utf-8",
            )
        return sdmc

    def test_discovers_valid_decks_in_selector_order(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            sdmc = self.write_deck_root(
                Path(temp_dir),
                ["sample", "limits-demo", "leetcode"],
            )
            invalid_dir = sdmc / drive_azahar_m7_smoke.DECK_ROOT / "bad deck"
            invalid_dir.mkdir()
            (invalid_dir / "cards.tsv").write_text(
                "card\tnote\tfront\tback\ttag\n",
                encoding="utf-8",
            )
            no_cards_dir = sdmc / drive_azahar_m7_smoke.DECK_ROOT / "empty"
            no_cards_dir.mkdir()

            self.assertEqual(
                drive_azahar_m7_smoke.discover_deck_ids(sdmc),
                ["leetcode", "limits-demo", "sample"],
            )

    def test_smoke_steps_navigate_to_samples_when_personal_decks_exist(self):
        deck_ids = [
            "leetcode",
            "limits-demo",
            "recsys",
            "sample",
            "sat-vocabulary",
            "vim",
        ]
        steps = drive_azahar_m7_smoke.smoke_steps(deck_ids)
        labels = [step.label for step in steps]
        buttons_by_label = {step.label: step.button for step in steps}

        self.assertLess(
            labels.index("select limits-demo deck"),
            labels.index("open limits-demo"),
        )
        self.assertEqual(buttons_by_label["select limits-demo deck"], "DOWN")
        self.assertLess(
            labels.index("select limits-demo deck after relaunch"),
            labels.index("reopen limits-demo after relaunch"),
        )
        self.assertEqual(
            buttons_by_label["select limits-demo deck after relaunch"],
            "DOWN",
        )
        self.assertLess(
            labels.index("select sample reset deck 1/2"),
            labels.index("open sample reset deck"),
        )
        self.assertEqual(buttons_by_label["select sample reset deck 1/2"], "DOWN")
        self.assertEqual(buttons_by_label["select sample reset deck 2/2"], "DOWN")

    def test_smoke_steps_cover_daily_use_artifact_flow(self):
        steps = drive_azahar_m7_smoke.smoke_steps()
        labels = [step.label for step in steps]
        buttons = [step.button for step in steps if step.button is not None]
        buttons_by_label = {step.label: step.button for step in steps}

        self.assertIn("open study settings", labels)
        self.assertIn("save study settings", labels)
        self.assertIn("rate first card Good", labels)
        self.assertIn("undo saved rating", labels)
        self.assertEqual(buttons_by_label["undo saved rating"], "B")
        self.assertIn("confirm suspend", labels)
        self.assertIn("open restore confirmation", labels)
        self.assertIn("confirm restore", labels)
        self.assertEqual(
            buttons_by_label["return to deck selector before exit"],
            "SELECT",
        )
        self.assertEqual(buttons_by_label["open exit confirmation"], "Y")
        self.assertTrue(any(step.relaunch for step in steps))
        self.assertIn("reopen limits-demo after relaunch", labels)
        self.assertIn("return to deck selector after persistence check", labels)
        self.assertIn("open sample reset deck", labels)
        self.assertIn("reveal sample reset card", labels)
        self.assertIn("rate sample reset card Good", labels)
        self.assertIn("open reset confirmation", labels)
        self.assertIn("confirm sample reset", labels)
        self.assertEqual(
            labels[-3:],
            [
                "return to deck selector before final exit",
                "open final exit confirmation",
                "confirm final exit",
            ],
        )
        self.assertEqual(buttons[-3:], ["SELECT", "Y", "A"])
        self.assertEqual(
            steps[labels.index("save study settings")].button,
            "X",
        )
        self.assertNotIn("open actions", labels)
        self.assertNotIn("choose restore suspended", labels)
        self.assertEqual(
            buttons_by_label["return to deck selector after persistence check"],
            "SELECT",
        )
        self.assertEqual(
            buttons_by_label["confirm sample reset"],
            "X",
        )

    def test_smoke_steps_save_and_reset_expected_decks(self):
        self.assertEqual(
            drive_azahar_m7_smoke.EXPECTED_SETTINGS,
            "limits-demo:5:10 sample:20:200",
        )
        self.assertEqual(drive_azahar_m7_smoke.EXPECTED_RESET_DECK, "sample")
        self.assertIn(
            "verify-azahar-m7-smoke-artifacts",
            drive_azahar_m7_smoke.VERIFY_COMMAND,
        )
        self.assertIn(
            "M7_DECKS=limits-demo",
            drive_azahar_m7_smoke.VERIFY_COMMAND_DETAILS,
        )
        self.assertIn(
            "M7_RESET_DECKS=sample",
            drive_azahar_m7_smoke.VERIFY_COMMAND_DETAILS,
        )
        self.assertIn(
            "M7_EXPECT_SETTINGS=limits-demo:5:10 sample:20:200",
            drive_azahar_m7_smoke.VERIFY_COMMAND_DETAILS,
        )

    def test_button_script_uses_verified_azahar_key_bindings(self):
        self.assertIn('keystroke "a"', drive_azahar_m7_smoke.button_script("A"))
        self.assertIn("key code 51", drive_azahar_m7_smoke.button_script("SELECT"))
        self.assertIn("key code 36", drive_azahar_m7_smoke.button_script("START"))
        self.assertIn("key code 125", drive_azahar_m7_smoke.button_script("DOWN"))
        self.assertIn("key code 124", drive_azahar_m7_smoke.button_script("RIGHT"))

    def test_button_script_rejects_unknown_buttons(self):
        with self.assertRaisesRegex(ValueError, "unknown driver button"):
            drive_azahar_m7_smoke.button_script("TOUCH")

    def test_dry_run_prints_sequence_and_verifier_command(self):
        output = io.StringIO()
        with contextlib.redirect_stdout(output):
            exit_code = drive_azahar_m7_smoke.main(["--dry-run"])

        text = output.getvalue()
        self.assertEqual(exit_code, 0)
        self.assertIn("open limits-demo", text)
        self.assertIn("relaunch app to prove persistence", text)
        self.assertIn("open sample reset deck", text)
        self.assertIn("confirm sample reset", text)
        self.assertIn("make verify-azahar-m7-smoke-artifacts", text)
        self.assertIn("M7_RESET_DECKS=sample", text)
        self.assertIn("M7_EXPECT_SETTINGS=limits-demo:5:10 sample:20:200", text)

    def test_check_automation_mode_does_not_require_azahar_paths(self):
        output = io.StringIO()
        with mock.patch.object(
            drive_azahar_m7_smoke,
            "check_keyboard_automation",
        ) as check:
            with contextlib.redirect_stdout(output):
                exit_code = drive_azahar_m7_smoke.main(
                    [
                        "--check-automation",
                        "--azahar-app",
                        "/missing/Azahar.app",
                        "--app-3dsx",
                        "/missing/anki3ds.3dsx",
                    ]
                )

        self.assertEqual(exit_code, 0)
        check.assert_called_once_with()
        self.assertIn("automation check ok", output.getvalue())

    def test_check_automation_mode_reports_accessibility_denial(self):
        output = io.StringIO()
        with mock.patch.object(
            drive_azahar_m7_smoke,
            "check_keyboard_automation",
            side_effect=drive_azahar_m7_smoke.DriverError(
                "Grant Accessibility permission"
            ),
        ):
            with contextlib.redirect_stderr(output):
                exit_code = drive_azahar_m7_smoke.main(["--check-automation"])

        self.assertEqual(exit_code, 1)
        self.assertIn("Grant Accessibility permission", output.getvalue())

    def test_osascript_accessibility_denial_has_actionable_error(self):
        denied = subprocess.CalledProcessError(
            1,
            ["osascript", "-e", "..."],
            stderr="System Events got an error: osascript is not allowed to send keystrokes.",
        )
        with mock.patch.object(
            drive_azahar_m7_smoke.subprocess,
            "run",
            side_effect=denied,
        ):
            with self.assertRaisesRegex(
                drive_azahar_m7_smoke.DriverError,
                "Grant Accessibility permission",
            ):
                drive_azahar_m7_smoke.run_osascript("script")


if __name__ == "__main__":
    unittest.main()
