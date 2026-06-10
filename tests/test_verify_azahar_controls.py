import importlib.util
import io
import tempfile
import unittest
from contextlib import redirect_stderr
from pathlib import Path
from unittest import mock


ROOT = Path(__file__).resolve().parents[1]
VERIFY_AZAHAR_CONTROLS_PATH = ROOT / "tools" / "verify_azahar_controls.py"
VERIFY_AZAHAR_CONTROLS_SPEC = importlib.util.spec_from_file_location(
    "verify_azahar_controls",
    VERIFY_AZAHAR_CONTROLS_PATH,
)
verify_azahar_controls = importlib.util.module_from_spec(VERIFY_AZAHAR_CONTROLS_SPEC)
assert VERIFY_AZAHAR_CONTROLS_SPEC.loader is not None
VERIFY_AZAHAR_CONTROLS_SPEC.loader.exec_module(verify_azahar_controls)


EXPECTED_CIRCLE_PAD = (
    'profiles\\1\\circle_pad="'
    'down:code$083$1engine$0keyboard,'
    'engine:analog_from_button,'
    'left:code$081$1engine$0keyboard,'
    'right:code$069$1engine$0keyboard,'
    'up:code$087$1engine$0keyboard'
    '"'
)
ARROW_CIRCLE_PAD = (
    'profiles\\1\\circle_pad="'
    'down:code$016777237$1engine$0keyboard,'
    'engine:analog_from_button,'
    'left:code$016777234$1engine$0keyboard,'
    'right:code$016777236$1engine$0keyboard,'
    'up:code$016777235$1engine$0keyboard'
    '"'
)

VALID_CONTROLS = """\
[Controls]
profile=0
profiles\\1\\button_a="code:65,engine:keyboard"
profiles\\1\\button_b="code:66,engine:keyboard"
profiles\\1\\button_x="code:88,engine:keyboard"
profiles\\1\\button_y="code:89,engine:keyboard"
profiles\\1\\button_l="code:76,engine:keyboard"
profiles\\1\\button_r="code:82,engine:keyboard"
profiles\\1\\button_select="code:16777219,engine:keyboard"
profiles\\1\\button_start="code:16777220,engine:keyboard"
profiles\\1\\button_up="code:16777235,engine:keyboard"
profiles\\1\\button_down="code:16777237,engine:keyboard"
profiles\\1\\button_left="code:16777234,engine:keyboard"
profiles\\1\\button_right="code:16777236,engine:keyboard"
profiles\\1\\circle_pad="down:code$083$1engine$0keyboard,engine:analog_from_button,left:code$081$1engine$0keyboard,right:code$069$1engine$0keyboard,up:code$087$1engine$0keyboard"
profiles\\size=1
"""


class VerifyAzaharControlsTests(unittest.TestCase):
    def write_config(self, root: Path, content: str = VALID_CONTROLS) -> Path:
        path = root / "qt-config.ini"
        path.write_text(content, encoding="utf-8")
        return path

    def test_accepts_expected_keyboard_profile(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_config(Path(temp_dir))

            self.assertEqual(verify_azahar_controls.verify_azahar_controls(path), [])

    def test_rejects_wrong_face_button_binding(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_config(
                Path(temp_dir),
                VALID_CONTROLS.replace(
                    'profiles\\1\\button_a="code:65,engine:keyboard"',
                    'profiles\\1\\button_a="code:90,engine:keyboard"',
                ),
            )

            errors = verify_azahar_controls.verify_azahar_controls(path)

            self.assertTrue(
                any("button_a should map to keyboard A" in error for error in errors),
                errors,
            )

    def test_rejects_missing_circle_pad(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_config(
                Path(temp_dir),
                VALID_CONTROLS.replace(
                    EXPECTED_CIRCLE_PAD + "\n",
                    "",
                ),
            )

            errors = verify_azahar_controls.verify_azahar_controls(path)

            self.assertTrue(any("missing profiles\\1\\circle_pad" in error for error in errors))

    def test_rejects_arrow_key_circle_pad_binding(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_config(
                Path(temp_dir),
                VALID_CONTROLS.replace(EXPECTED_CIRCLE_PAD, ARROW_CIRCLE_PAD),
            )

            errors = verify_azahar_controls.verify_azahar_controls(path)

            self.assertTrue(
                any("circle pad up should map to W" in error for error in errors),
                errors,
            )

    def test_fix_rewrites_circle_pad_without_touching_dpad(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_config(
                Path(temp_dir),
                VALID_CONTROLS.replace(EXPECTED_CIRCLE_PAD, ARROW_CIRCLE_PAD),
            )

            self.assertTrue(verify_azahar_controls.fix_azahar_controls(path))
            content = path.read_text(encoding="utf-8")

            self.assertIn(EXPECTED_CIRCLE_PAD, content)
            self.assertIn(
                'profiles\\1\\button_up="code:16777235,engine:keyboard"',
                content,
            )
            self.assertEqual(verify_azahar_controls.verify_azahar_controls(path), [])

    def test_cli_reports_errors_without_traceback(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            path = self.write_config(
                Path(temp_dir),
                VALID_CONTROLS.replace(
                    'profiles\\1\\button_start="code:16777220,engine:keyboard"',
                    'profiles\\1\\button_start="code:77,engine:keyboard"',
                ),
            )
            stderr = io.StringIO()

            with mock.patch(
                "sys.argv",
                ["verify_azahar_controls.py", str(path)],
            ), redirect_stderr(stderr):
                exit_code = verify_azahar_controls.main()

            self.assertEqual(exit_code, 1)
            self.assertIn("button_start should map to keyboard Enter", stderr.getvalue())
            self.assertNotIn("Traceback", stderr.getvalue())


if __name__ == "__main__":
    unittest.main()
