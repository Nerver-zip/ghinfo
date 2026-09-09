#!/usr/bin/env python3
"""Deterministic tests for the Kustom setup generator."""

from __future__ import annotations

import json
import tempfile
import unittest
import zipfile
from pathlib import Path

if __package__:
    from .kustom_setup import (
        DEFAULT_GHINFO_URL,
        DEFAULT_REFRESH_MINUTES,
        EMPTY_ACTIVITY,
        TEMPLATE_KWGT,
        customize_preset,
        load_template_preset,
        normalize_base_url,
        render_clip,
        validate_refresh_minutes,
        write_artifacts,
    )
else:
    from kustom_setup import (
        DEFAULT_GHINFO_URL,
        DEFAULT_REFRESH_MINUTES,
        EMPTY_ACTIVITY,
        TEMPLATE_KWGT,
        customize_preset,
        load_template_preset,
        normalize_base_url,
        render_clip,
        validate_refresh_minutes,
        write_artifacts,
    )


class KustomSetupTests(unittest.TestCase):
    def test_url_validation_rejects_credentials_and_query(self) -> None:
        self.assertEqual(normalize_base_url(" http://example.test:8080/ "), "http://example.test:8080")
        for value in (
            "example.test:8080",
            "ftp://example.test",
            "http://user:pass@example.test",
            "http://example.test/?token=x",
            "http://example.test/with space",
        ):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    normalize_base_url(value)

    def test_refresh_interval_is_bounded(self) -> None:
        self.assertEqual(validate_refresh_minutes("5"), 5)
        for value in (0, 60, "not-a-number"):
            with self.subTest(value=value):
                with self.assertRaises(ValueError):
                    validate_refresh_minutes(value)

    def test_customization_updates_all_flows_and_preserves_tree(self) -> None:
        preset = load_template_preset(TEMPLATE_KWGT)
        initial = {
            "schemaVersion": 1,
            "generation": 42,
            "generatedAt": "2026-09-09T12:00:00Z",
            "stale": False,
            "activity": {"items": []},
        }
        customized = customize_preset(preset, "http://widget.example:8080/", 7, initial)
        root = customized["preset_root"]
        flows = {flow["name"]: flow for flow in root["internal_flows"]}
        expected = {
            "ghinfo": "http://widget.example:8080/v1/activity/items?limit=3",
            "ghinfo_workflows": "http://widget.example:8080/v1/activity/items?category=workflows&limit=3",
            "ghinfo_pullrequests": "http://widget.example:8080/v1/activity/items?category=pull_requests&limit=3",
            "ghinfo_issues": "http://widget.example:8080/v1/activity/items?category=issues&limit=3",
        }
        for name, uri in expected.items():
            with self.subTest(flow=name):
                wget = next(action for action in flows[name]["a"] if action["type"] == "A_WGET")
                self.assertEqual(wget["params"]["uri"], uri)
        cron = next(trigger for trigger in flows["ghinfo"]["t"] if trigger["type"] == "T_CRON")
        self.assertEqual(cron["params"]["cron_string"], "*/7 * * * *")
        self.assertEqual(json.loads(root["globals_list"]["ghinfo"]["value"]), initial)
        self.assertEqual(root["viewgroup_items"], preset["preset_root"]["viewgroup_items"])

    def test_complete_clip_has_native_markers_and_no_credentials(self) -> None:
        preset = customize_preset(load_template_preset(TEMPLATE_KWGT), "http://widget.example:8080", 5)
        content = render_clip(preset)
        marker = "##KUSTOMCLIP##"
        self.assertTrue(content.startswith(marker + "\n"))
        self.assertTrue(content.endswith(marker + "\n"))
        payload = json.loads(content.strip()[len(marker) : -len(marker)].strip())
        component = payload["clip_modules"][0]
        self.assertEqual(component["internal_type"], "KomponentModule")
        self.assertEqual(component["internal_title"], "ghinfo Activity Widget")
        self.assertIn("internal_flows", component)
        self.assertIn("viewgroup_items", component)
        self.assertNotIn("Authorization", content)
        self.assertNotIn("github_pat_", content)

    def test_artifacts_are_valid_and_package_keeps_font(self) -> None:
        preset = customize_preset(load_template_preset(TEMPLATE_KWGT), "http://widget.example:8080", 5)
        with tempfile.TemporaryDirectory() as directory:
            paths = write_artifacts(preset, Path(directory))
            for path in paths.values():
                self.assertTrue(path.is_file(), path)
            loose = paths["loose_clip"].read_text(encoding="utf-8")
            self.assertTrue(loose.startswith("##KUSTOMCLIP##\n"))
            loose_data = json.loads(
                loose.strip()[len("##KUSTOMCLIP##") : -len("##KUSTOMCLIP##")].strip()
            )
            self.assertGreater(len(loose_data["clip_modules"]), 0)
            with zipfile.ZipFile(paths["kwgt"]) as archive:
                self.assertIn("preset.json", archive.namelist())
                self.assertIn("fonts/FiraCodeNerdFontMono.ttf", archive.namelist())
                self.assertEqual(json.loads(archive.read("preset.json")), preset)
                self.assertEqual(archive.testzip(), None)

    def test_tracked_default_clip_is_regenerated_shape(self) -> None:
        expected = render_clip(
            customize_preset(
                load_template_preset(TEMPLATE_KWGT),
                DEFAULT_GHINFO_URL,
                DEFAULT_REFRESH_MINUTES,
                EMPTY_ACTIVITY,
            )
        )
        tracked = Path(__file__).resolve().parents[1] / "assets" / "ghinfo-kustom-widget.clip"
        self.assertEqual(tracked.read_text(encoding="utf-8"), expected)


if __name__ == "__main__":
    unittest.main()
