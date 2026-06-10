#!/usr/bin/env python3
"""Verify Azahar keyboard bindings used by the anki3ds feedback loop."""

from __future__ import annotations

import argparse
import os
import sys
from pathlib import Path


DEFAULT_CONFIG = Path(
    os.environ.get(
        "AZAHAR_CONFIG",
        "~/Library/Application Support/Azahar/config/qt-config.ini",
    )
).expanduser()

EXPECTED_BUTTONS = {
    "button_a": ("A", 65),
    "button_b": ("B", 66),
    "button_x": ("X", 88),
    "button_y": ("Y", 89),
    "button_l": ("L", 76),
    "button_r": ("R", 82),
    "button_select": ("Backspace", 16777219),
    "button_start": ("Enter", 16777220),
    "button_up": ("Up Arrow", 16777235),
    "button_down": ("Down Arrow", 16777237),
    "button_left": ("Left Arrow", 16777234),
    "button_right": ("Right Arrow", 16777236),
}

EXPECTED_CIRCLE_PAD = {
    "up": ("W", 87),
    "down": ("S", 83),
    "left": ("Q", 81),
    "right": ("E", 69),
}


def unquote_qsettings_value(value: str) -> str:
    value = value.strip()
    if len(value) >= 2 and value[0] == '"' and value[-1] == '"':
        return value[1:-1]

    return value


def load_controls_section(config_path: Path) -> dict[str, str]:
    controls: dict[str, str] = {}
    in_controls = False

    for line in config_path.read_text(encoding="utf-8").splitlines():
        stripped = line.strip()
        if stripped == "" or stripped.startswith("#"):
            continue
        if stripped.startswith("[") and stripped.endswith("]"):
            in_controls = stripped == "[Controls]"
            continue
        if not in_controls or "=" not in stripped:
            continue

        key, value = stripped.split("=", 1)
        controls[key] = unquote_qsettings_value(value)

    return controls


def active_profile_prefix(controls: dict[str, str]) -> str:
    profile_value = controls.get("profile", "0")

    try:
        profile_number = int(profile_value)
    except ValueError:
        profile_number = 0

    preferred_prefix = f"profiles\\{profile_number + 1}"
    if any(key.startswith(preferred_prefix + "\\") for key in controls):
        return preferred_prefix

    return "profiles\\1"


def keyboard_binding_matches(value: str, expected_code: int) -> bool:
    fields: dict[str, str] = {}

    for field in value.split(","):
        if ":" not in field:
            continue
        key, field_value = field.split(":", 1)
        fields[key] = field_value

    return fields.get("engine") == "keyboard" and fields.get("code") == str(expected_code)


def keyboard_binding_value(expected_code: int) -> str:
    return f"code:{expected_code},engine:keyboard"


def expected_circle_pad_value() -> str:
    return (
        f"down:code$0{EXPECTED_CIRCLE_PAD['down'][1]}$1engine$0keyboard,"
        "engine:analog_from_button,"
        f"left:code$0{EXPECTED_CIRCLE_PAD['left'][1]}$1engine$0keyboard,"
        f"right:code$0{EXPECTED_CIRCLE_PAD['right'][1]}$1engine$0keyboard,"
        f"up:code$0{EXPECTED_CIRCLE_PAD['up'][1]}$1engine$0keyboard"
    )


def analog_direction_binding_matches(
    value: str,
    direction: str,
    expected_code: int,
) -> bool:
    direction_prefix = f"{direction}:"
    encoded_code = f"code$0{expected_code}"

    for field in value.split(","):
        if not field.startswith(direction_prefix):
            continue

        payload = field[len(direction_prefix):]
        return (
            (
                encoded_code in payload or
                f"code:{expected_code}" in payload
            ) and (
                "engine$0keyboard" in payload or
                "engine:keyboard" in payload
            )
        )

    return False


def verify_azahar_controls(config_path: Path) -> list[str]:
    errors: list[str] = []

    if not config_path.is_file():
        return [f"{config_path}: missing Azahar config"]

    controls = load_controls_section(config_path)
    if not controls:
        return [f"{config_path}: missing [Controls] section"]

    profile_prefix = active_profile_prefix(controls)
    for button_key, (label, expected_code) in EXPECTED_BUTTONS.items():
        full_key = f"{profile_prefix}\\{button_key}"
        value = controls.get(full_key)
        if value is None:
            errors.append(f"{config_path}: missing {full_key}")
        elif not keyboard_binding_matches(value, expected_code):
            errors.append(
                f"{config_path}: {full_key} should map to keyboard {label}"
            )

    circle_pad_key = f"{profile_prefix}\\circle_pad"
    circle_pad = controls.get(circle_pad_key)
    if circle_pad is None:
        errors.append(f"{config_path}: missing {circle_pad_key}")
    elif "engine:analog_from_button" not in circle_pad:
        errors.append(f"{config_path}: {circle_pad_key} should use arrow-key buttons")
    else:
        for direction, (label, expected_code) in EXPECTED_CIRCLE_PAD.items():
            if not analog_direction_binding_matches(
                circle_pad,
                direction,
                expected_code,
            ):
                errors.append(
                    f"{config_path}: circle pad {direction} should map to {label}"
                )

    return errors


def expected_control_values(profile_prefix: str) -> dict[str, str]:
    values = {
        f"{profile_prefix}\\{button_key}": keyboard_binding_value(expected_code)
        for button_key, (_, expected_code) in EXPECTED_BUTTONS.items()
    }
    values[f"{profile_prefix}\\circle_pad"] = expected_circle_pad_value()
    return values


def quote_qsettings_value(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def fix_azahar_controls(config_path: Path) -> bool:
    if config_path.is_file():
        lines = config_path.read_text(encoding="utf-8").splitlines()
    else:
        lines = []

    controls = load_controls_section(config_path) if config_path.is_file() else {}
    profile_prefix = active_profile_prefix(controls)
    updates = expected_control_values(profile_prefix)
    default_updates = {
        f"{key}\\default": "false"
        for key in updates
    }
    changed = False
    in_controls = False
    controls_seen = False
    seen: set[str] = set()
    output: list[str] = []

    def append_missing_controls() -> None:
        nonlocal changed

        for key, value in updates.items():
            if key not in seen:
                output.append(f"{key}={quote_qsettings_value(value)}")
                changed = True
            default_key = f"{key}\\default"
            if default_key not in seen:
                output.append(f"{default_key}=false")
                changed = True

    for line in lines:
        stripped = line.strip()
        if stripped.startswith("[") and stripped.endswith("]"):
            if in_controls:
                append_missing_controls()
            in_controls = stripped == "[Controls]"
            controls_seen = controls_seen or in_controls
            output.append(line)
            continue

        if in_controls and "=" in stripped and not stripped.startswith("#"):
            key, _ = stripped.split("=", 1)
            if key in updates:
                replacement = f"{key}={quote_qsettings_value(updates[key])}"
                output.append(replacement)
                seen.add(key)
                changed = changed or replacement != line
                continue
            if key in default_updates:
                replacement = f"{key}={default_updates[key]}"
                output.append(replacement)
                seen.add(key)
                changed = changed or replacement != line
                continue

        output.append(line)

    if in_controls:
        append_missing_controls()
    elif not controls_seen:
        if output and output[-1] != "":
            output.append("")
        output.append("[Controls]")
        output.append("profile=0")
        output.append("profile\\default=false")
        append_missing_controls()

    if changed:
        config_path.parent.mkdir(parents=True, exist_ok=True)
        config_path.write_text("\n".join(output) + "\n", encoding="utf-8")

    return changed


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "config",
        nargs="?",
        type=Path,
        default=DEFAULT_CONFIG,
        help="Path to Azahar's qt-config.ini.",
    )
    parser.add_argument(
        "--quiet",
        action="store_true",
        help="Print only validation errors.",
    )
    parser.add_argument(
        "--fix",
        action="store_true",
        help="Rewrite the active Azahar profile with the expected keyboard bindings.",
    )
    args = parser.parse_args()

    if args.fix:
        changed = fix_azahar_controls(args.config.expanduser())
        if changed and not args.quiet:
            print(f"{args.config.expanduser()}: updated Azahar controls")

    errors = verify_azahar_controls(args.config.expanduser())
    if errors:
        for error in errors:
            print(f"error: {error}", file=sys.stderr)
        return 1

    if not args.quiet:
        print(f"{args.config.expanduser()}: ok - Azahar controls match anki3ds")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
