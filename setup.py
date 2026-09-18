#!/usr/bin/env python3
"""Interactive setup wizard for the ghinfo Kustom widget."""

from __future__ import annotations

import argparse
import os
from pathlib import Path

from scripts.kustom_setup import (
    DEFAULT_GHINFO_URL,
    DEFAULT_REFRESH_MINUTES,
    TEMPLATE_KWGT,
    EMPTY_ACTIVITY,
    copy_to_clipboard,
    customize_preset,
    fetch_initial_activity,
    load_template_preset,
    normalize_base_url,
    validate_refresh_minutes,
    write_artifacts,
)


def prompt_url(value: str | None) -> str:
    default = os.environ.get("GHINFO_WIDGET_URL") or DEFAULT_GHINFO_URL
    candidate = value.strip() if value else input(f"? ghinfo URL [{default}]: ").strip() or default
    while True:
        try:
            return normalize_base_url(candidate)
        except ValueError as exc:
            print(f"Invalid URL: {exc}")
            candidate = input(f"? ghinfo URL [{default}]: ").strip() or default


def prompt_refresh(value: int | None) -> int:
    default = os.environ.get("GHINFO_WIDGET_REFRESH_MINUTES") or str(DEFAULT_REFRESH_MINUTES)
    candidate = str(value) if value is not None else input(f"? Refresh interval in minutes [{default}]: ").strip() or default
    while True:
        try:
            return validate_refresh_minutes(candidate)
        except ValueError as exc:
            print(f"Invalid interval: {exc}")
            candidate = input(f"? Refresh interval in minutes [{default}]: ").strip() or default


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate a personalized Kustom clip for ghinfo.")
    parser.add_argument("--url", help="ghinfo base URL (e.g. http://100.118.53.107:8080)")
    parser.add_argument("--refresh-minutes", type=int, help="main Flow interval, from 1 to 59 minutes")
    parser.add_argument("--output-dir", default="dist", help="output directory (default: dist)")
    parser.add_argument("--no-probe", action="store_true", help="do not query the snapshot to seed the widget")
    parser.add_argument("--no-clipboard", action="store_true", help="do not try to copy the clip to the clipboard")
    args = parser.parse_args()

    print("╔══════════════════════════════════════════════════════════╗")
    print("║                    ghinfo Kustom Setup                   ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print("The wizard never asks for or handles a GitHub token.\n")

    base_url = prompt_url(args.url)
    refresh_minutes = prompt_refresh(args.refresh_minutes)
    initial_payload = None
    if not args.no_probe:
        print(f"\nFetching snapshot from {base_url}...")
        initial_payload, reason = fetch_initial_activity(base_url)
        if initial_payload is None:
            print(f"Warning: {reason}; the widget will start with an empty cache.")
        else:
            count = len(initial_payload["activity"]["items"])
            generation = initial_payload.get("generation", 0)
            print(f"✓ Snapshot found: generation {generation}, {count} item(s).")
    if initial_payload is None:
        initial_payload = EMPTY_ACTIVITY

    print("\nGenerating artifacts...")
    preset = customize_preset(load_template_preset(TEMPLATE_KWGT), base_url, refresh_minutes, initial_payload)
    paths = write_artifacts(preset, Path(args.output_dir).expanduser())
    print(f"✓ Complete clip: {paths['clip']}")
    print(f"✓ Layout-only clip: {paths['loose_clip']}")
    print(f"✓ Premium package: {paths['kwgt']}")

    if not args.no_clipboard:
        tool = copy_to_clipboard(paths["clip"].read_text(encoding="utf-8"))
        if tool:
            print(f"✓ Complete clip copied to the clipboard via {tool}.")
        else:
            print("ℹ No clipboard utility found; copy the .clip file manually.")

    print("\nOn Android: create a blank KWGT widget, open the editor, choose Add → Komponent, and")
    print("go back once so KWGT offers 'Paste Komponent from Clipboard'.")
    print("Save the widget. The file starts and ends with ##KUSTOMCLIP##.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nCancelled.")
        raise SystemExit(130)
