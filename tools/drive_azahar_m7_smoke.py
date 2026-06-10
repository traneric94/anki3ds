#!/usr/bin/env python3
"""Drive a small Azahar M7 smoke pass through the configured keyboard map."""

from __future__ import annotations

import argparse
import os
import subprocess
import sys
import time
from dataclasses import dataclass
from pathlib import Path

from verify_azahar_controls import DEFAULT_CONFIG, verify_azahar_controls


DEFAULT_AZAHAR_APP = Path(
    os.environ.get(
        "AZAHAR_APP",
        "~/Applications/azahar-macos-arm64-2125.1.2/Azahar.app",
    )
).expanduser()
DEFAULT_APP_3DSX = Path("app-3ds/anki3ds.3dsx")
DEFAULT_SDMC = Path(
    os.environ.get(
        "AZAHAR_SDMC",
        "~/Library/Application Support/Azahar/sdmc",
    )
).expanduser()
APP_SD_DIR = Path("3ds/anki3ds")
DECK_ROOT = APP_SD_DIR / "decks"
EXPECTED_SETTINGS = "limits-demo:5:10 sample:20:200"
EXPECTED_STUDY_DECK = "limits-demo"
EXPECTED_RESET_DECK = "sample"
VERIFY_COMMAND = "make verify-azahar-m7-smoke-artifacts"
VERIFY_COMMAND_DETAILS = (
    "make verify-m7-artifacts "
    "M7_DECKS=limits-demo "
    f"M7_RESET_DECKS={EXPECTED_RESET_DECK} "
    f"M7_EXPECT_SETTINGS={EXPECTED_SETTINGS}"
)


class DriverError(RuntimeError):
    pass


@dataclass(frozen=True)
class DriverStep:
    label: str
    button: str | None = None
    wait: float = 0.18
    relaunch: bool = False


BUTTON_APPLESCRIPT = {
    "A": 'keystroke "a"',
    "B": 'keystroke "b"',
    "X": 'keystroke "x"',
    "Y": 'keystroke "y"',
    "L": 'keystroke "l"',
    "R": 'keystroke "r"',
    "SELECT": "key code 51",
    "START": "key code 36",
    "UP": "key code 126",
    "DOWN": "key code 125",
    "LEFT": "key code 123",
    "RIGHT": "key code 124",
}

AUTOMATION_CHECK_APPLESCRIPT = "\n".join(
    (
        'tell application "System Events"',
        "  key code 63",
        "end tell",
    )
)


def valid_deck_id(deck_id: str) -> bool:
    return (
        0 < len(deck_id) <= 63
        and all(character.isalnum() or character in "_-" for character in deck_id)
    )


def discover_deck_ids(sdmc: Path) -> list[str]:
    deck_root = sdmc.expanduser() / DECK_ROOT
    if not deck_root.is_dir():
        raise DriverError(f"missing deck root: {deck_root}")

    deck_ids = [
        child.name
        for child in deck_root.iterdir()
        if (
            child.is_dir()
            and valid_deck_id(child.name)
            and (child / "cards.tsv").is_file()
        )
    ]
    return sorted(deck_ids)


def navigation_steps(
    deck_ids: list[str],
    current_index: int,
    target_deck_id: str,
    label: str,
) -> tuple[list[DriverStep], int]:
    try:
        target_index = deck_ids.index(target_deck_id)
    except ValueError as exc:
        raise DriverError(
            f"{target_deck_id} is not installed in the smoke deck root"
        ) from exc

    if target_index == current_index:
        return [], target_index

    deck_count = len(deck_ids)
    down_steps = (target_index - current_index) % deck_count
    up_steps = (current_index - target_index) % deck_count
    if down_steps <= up_steps:
        button = "DOWN"
        step_count = down_steps
    else:
        button = "UP"
        step_count = up_steps

    steps = []
    for step_index in range(step_count):
        step_label = (
            label
            if step_count == 1
            else f"{label} {step_index + 1}/{step_count}"
        )
        steps.append(DriverStep(step_label, button, 0.25))
    return steps, target_index


def smoke_steps(deck_ids: list[str] | None = None) -> list[DriverStep]:
    """Return the deterministic M7 smoke flow.

    When deck IDs are supplied, the sequence navigates by sorted deck-folder id
    so personal imported decks can coexist with the tracked sample decks.
    """
    steps = [DriverStep("wait for fresh deck selector", wait=2.0)]
    current_index = 0

    if deck_ids is not None:
        if len(deck_ids) == 0:
            raise DriverError("no smoke decks found")
        navigation, current_index = navigation_steps(
            deck_ids,
            current_index,
            EXPECTED_STUDY_DECK,
            "select limits-demo deck",
        )
        steps.extend(navigation)

    steps.extend(
        [
            DriverStep("open limits-demo", "A", 0.7),
            DriverStep("open study settings", "X", 0.35),
            DriverStep("change new limit 2 -> 5", "RIGHT", 0.25),
            DriverStep("select review limit", "DOWN", 0.25),
            DriverStep("change review limit 5 -> 10", "RIGHT", 0.25),
            DriverStep("save study settings", "X", 0.7),
            DriverStep("reveal first card", "A", 0.35),
            DriverStep("rate first card Good", "X", 0.7),
            DriverStep("undo saved rating", "B", 0.7),
            DriverStep("reveal first card again", "A", 0.35),
            DriverStep("open suspend confirmation", "R", 0.35),
            DriverStep("confirm suspend", "X", 0.7),
            DriverStep("open second suspend confirmation", "R", 0.35),
            DriverStep("confirm second suspend", "X", 0.7),
            DriverStep("open third suspend confirmation", "R", 0.35),
            DriverStep("confirm third suspend", "X", 0.7),
            DriverStep("open fourth suspend confirmation", "R", 0.35),
            DriverStep("confirm fourth suspend", "X", 0.7),
            DriverStep("open fifth suspend confirmation", "R", 0.35),
            DriverStep("confirm fifth suspend", "X", 0.7),
            DriverStep("open sixth suspend confirmation", "R", 0.35),
            DriverStep("confirm sixth suspend", "X", 0.7),
            DriverStep("open restore confirmation", "R", 0.35),
            DriverStep("confirm restore", "X", 0.7),
            DriverStep("return to deck selector before exit", "SELECT", 0.35),
            DriverStep("open exit confirmation", "Y", 0.35),
            DriverStep("confirm exit after study actions", "A", 1.5),
            DriverStep("relaunch app to prove persistence", relaunch=True, wait=2.0),
            DriverStep("wait for relaunched deck selector", wait=1.0),
        ]
    )

    if deck_ids is not None:
        current_index = 0
        navigation, current_index = navigation_steps(
            deck_ids,
            current_index,
            EXPECTED_STUDY_DECK,
            "select limits-demo deck after relaunch",
        )
        steps.extend(navigation)

    steps.extend(
        [
            DriverStep("reopen limits-demo after relaunch", "A", 0.7),
            DriverStep(
                "return to deck selector after persistence check",
                "SELECT",
                0.35,
            ),
        ]
    )

    if deck_ids is not None:
        navigation, current_index = navigation_steps(
            deck_ids,
            current_index,
            EXPECTED_RESET_DECK,
            "select sample reset deck",
        )
        steps.extend(navigation)
    else:
        steps.append(DriverStep("select sample reset deck", "DOWN", 0.25))

    steps.extend(
        [
            DriverStep("open sample reset deck", "A", 0.7),
            DriverStep("reveal sample reset card", "A", 0.35),
            DriverStep("rate sample reset card Good", "X", 0.7),
            DriverStep("open reset confirmation", "Y", 0.35),
            DriverStep("confirm sample reset", "X", 0.7),
            DriverStep("return to deck selector before final exit", "SELECT", 0.35),
            DriverStep("open final exit confirmation", "Y", 0.35),
            DriverStep("confirm final exit", "A", 0.8),
        ]
    )
    return steps


def button_script(button: str) -> str:
    try:
        command = BUTTON_APPLESCRIPT[button]
    except KeyError as exc:
        raise ValueError(f"unknown driver button: {button}") from exc

    return "\n".join(
        (
            'tell application "Azahar" to activate',
            "delay 0.05",
            'tell application "System Events"',
            '  tell process "Azahar"',
            f"    {command}",
            "  end tell",
            "end tell",
        )
    )


def print_dry_run(steps: list[DriverStep]) -> None:
    for index, step in enumerate(steps, start=1):
        if step.relaunch:
            action = "relaunch"
        elif step.button is None:
            action = "wait"
        else:
            action = step.button
        print(f"{index:02d}. {action:8s} {step.label}")

    print()
    print("After a direct driver run, verify with:")
    print(VERIFY_COMMAND)
    print(f"Equivalent artifact command: {VERIFY_COMMAND_DETAILS}")


def run_osascript(script: str) -> None:
    try:
        subprocess.run(
            ["osascript", "-e", script],
            check=True,
            capture_output=True,
            text=True,
        )
    except subprocess.CalledProcessError as exc:
        stderr = exc.stderr.strip()
        if "not allowed to send keystrokes" in stderr:
            raise DriverError(
                "macOS denied osascript keystrokes. Grant Accessibility "
                "permission to the terminal/Codex host running this command, "
                "then rerun make run-emulator-m7-smoke."
            ) from exc
        if stderr:
            raise DriverError(stderr) from exc
        raise DriverError(f"osascript failed with exit code {exc.returncode}") from exc


def check_keyboard_automation() -> None:
    run_osascript(AUTOMATION_CHECK_APPLESCRIPT)


def relaunch_app(azahar_app: Path, app_3dsx: Path) -> None:
    try:
        subprocess.run(["open", "-a", str(azahar_app), str(app_3dsx)], check=True)
    except subprocess.CalledProcessError as exc:
        raise DriverError(f"failed to relaunch Azahar: {exc}") from exc


def run_steps(
    steps: list[DriverStep],
    azahar_app: Path,
    app_3dsx: Path,
) -> None:
    for step in steps:
        print(step.label, flush=True)
        if step.relaunch:
            relaunch_app(azahar_app, app_3dsx)
        elif step.button is not None:
            run_osascript(button_script(step.button))
        time.sleep(step.wait)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--azahar-app",
        type=Path,
        default=DEFAULT_AZAHAR_APP,
        help="Path to Azahar.app.",
    )
    parser.add_argument(
        "--app-3dsx",
        type=Path,
        default=DEFAULT_APP_3DSX,
        help="Path to app-3ds/anki3ds.3dsx.",
    )
    parser.add_argument(
        "--config",
        type=Path,
        default=DEFAULT_CONFIG,
        help="Path to Azahar qt-config.ini.",
    )
    parser.add_argument(
        "--sdmc",
        type=Path,
        default=DEFAULT_SDMC,
        help="Azahar SDMC root used to discover deck-selector order.",
    )
    parser.add_argument(
        "--skip-control-check",
        action="store_true",
        help="Do not verify Azahar key bindings before sending input.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print the planned key sequence without driving Azahar.",
    )
    parser.add_argument(
        "--check-automation",
        action="store_true",
        help="Only verify macOS System Events keystroke permission.",
    )
    args = parser.parse_args(argv)

    deck_ids = None
    try:
        deck_ids = discover_deck_ids(args.sdmc)
    except DriverError as exc:
        if not args.dry_run and not args.check_automation:
            print(f"error: {exc}", file=sys.stderr)
            return 1

    steps = smoke_steps(deck_ids)
    if args.dry_run:
        print_dry_run(steps)
        return 0

    if args.check_automation:
        try:
            check_keyboard_automation()
        except DriverError as exc:
            print(f"error: {exc}", file=sys.stderr)
            return 1

        print("Azahar smoke automation check ok")
        return 0

    if not args.skip_control_check:
        errors = verify_azahar_controls(args.config.expanduser())
        if errors:
            for error in errors:
                print(f"error: {error}", file=sys.stderr)
            return 1

    azahar_app = args.azahar_app.expanduser()
    app_3dsx = args.app_3dsx.expanduser()
    if not azahar_app.is_dir():
        print(f"error: missing Azahar app: {azahar_app}", file=sys.stderr)
        return 1
    if not app_3dsx.is_file():
        print(f"error: missing 3DSX app: {app_3dsx}", file=sys.stderr)
        return 1

    try:
        check_keyboard_automation()
        run_steps(steps, azahar_app, app_3dsx)
    except DriverError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    print()
    print("Driver finished. Verify artifacts with:")
    print(VERIFY_COMMAND)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
