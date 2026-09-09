#!/usr/bin/env python3
"""Build personalized Kustom artifacts from the checked-in ghinfo template.

The generated ``.clip`` is a native Kustom clipboard payload.  It contains
the complete component (globals, flows, layout, formulas, and touch actions),
so a free KWGT installation can import it without a premium preset library.
The setup tool only handles the public ghinfo URL; GitHub credentials never
belong in a widget or in this module.
"""

from __future__ import annotations

import copy
import json
import shutil
import subprocess
import urllib.error
import urllib.parse
import urllib.request
import zipfile
from pathlib import Path
from typing import Any, Optional


REPO_ROOT = Path(__file__).resolve().parents[1]
TEMPLATE_KWGT = REPO_ROOT / "assets" / "ghinfo-kustom-widget.kwgt"
DEFAULT_GHINFO_URL = "http://100.118.53.107:8080"
DEFAULT_REFRESH_MINUTES = 5
ACTIVITY_LIMIT = 3

EMPTY_ACTIVITY = {
    "schemaVersion": 1,
    "generation": 0,
    "generatedAt": "",
    "stale": True,
    "activity": {"items": []},
}

FLOW_PATHS = {
    "ghinfo": f"/v1/activity/items?limit={ACTIVITY_LIMIT}",
    "ghinfo_workflows": f"/v1/activity/items?category=workflows&limit={ACTIVITY_LIMIT}",
    "ghinfo_pullrequests": f"/v1/activity/items?category=pull_requests&limit={ACTIVITY_LIMIT}",
    "ghinfo_issues": f"/v1/activity/items?category=issues&limit={ACTIVITY_LIMIT}",
}


def normalize_base_url(value: str) -> str:
    """Validate and normalize a ghinfo base URL without accepting credentials."""

    candidate = value.strip()
    if any(character.isspace() for character in candidate):
        raise ValueError("a URL não pode conter espaços")
    parsed = urllib.parse.urlsplit(candidate)
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        raise ValueError("use uma URL http:// ou https:// com host")
    if parsed.username or parsed.password:
        raise ValueError("a URL não pode conter usuário ou senha")
    if parsed.query or parsed.fragment:
        raise ValueError("a URL base não pode conter query string ou fragmento")
    try:
        parsed.port
    except ValueError as exc:
        raise ValueError("a porta da URL não é válida") from exc
    return candidate.rstrip("/")


def validate_refresh_minutes(value: Any) -> int:
    """Return a conservative five-minute cron interval in the valid range."""

    try:
        minutes = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError("o intervalo deve ser um número inteiro") from exc
    if not 1 <= minutes <= 59:
        raise ValueError("o intervalo deve estar entre 1 e 59 minutos")
    return minutes


def is_activity_payload(value: Any) -> bool:
    return (
        isinstance(value, dict)
        and isinstance(value.get("activity"), dict)
        and isinstance(value["activity"].get("items"), list)
    )


def compact_json(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def load_template_preset(template_path: Path = TEMPLATE_KWGT) -> dict[str, Any]:
    """Read the native preset tree from the checked-in KWGT archive."""

    if not template_path.is_file():
        raise FileNotFoundError(f"template não encontrado: {template_path}")
    with zipfile.ZipFile(template_path) as archive:
        try:
            preset = json.loads(archive.read("preset.json"))
        except KeyError as exc:
            raise ValueError("o template não contém preset.json") from exc
    root = preset.get("preset_root")
    if not isinstance(root, dict) or not isinstance(root.get("internal_flows"), list):
        raise ValueError("preset.json não contém uma árvore Kustom válida")
    if not isinstance(root.get("globals_list"), dict) or not isinstance(root.get("viewgroup_items"), list):
        raise ValueError("preset.json não contém globals/layout válidos")
    return preset


def customize_preset(
    preset: dict[str, Any],
    base_url: str,
    refresh_minutes: int = DEFAULT_REFRESH_MINUTES,
    initial_payload: Optional[dict[str, Any]] = None,
) -> dict[str, Any]:
    """Apply user configuration while preserving the native layout tree."""

    normalized_url = normalize_base_url(base_url)
    interval = validate_refresh_minutes(refresh_minutes)
    customized = copy.deepcopy(preset)
    root = customized["preset_root"]

    payload = initial_payload if is_activity_payload(initial_payload) else EMPTY_ACTIVITY
    root["globals_list"]["ghinfo"]["value"] = compact_json(payload)

    seen_flows: set[str] = set()
    for flow in root["internal_flows"]:
        name = flow.get("name")
        if name not in FLOW_PATHS:
            continue
        seen_flows.add(name)
        wget_actions = [action for action in flow.get("a", []) if action.get("type") == "A_WGET"]
        if len(wget_actions) != 1:
            raise ValueError(f"Flow {name!r} deve conter exatamente uma ação WebGet")
        wget_actions[0].setdefault("params", {})["uri"] = normalized_url + FLOW_PATHS[name]
        if name == "ghinfo":
            cron_triggers = [trigger for trigger in flow.get("t", []) if trigger.get("type") == "T_CRON"]
            if len(cron_triggers) != 1:
                raise ValueError("o Flow ghinfo deve conter exatamente um trigger cron")
            cron_triggers[0].setdefault("params", {})["cron_string"] = f"*/{interval} * * * *"

    missing = set(FLOW_PATHS) - seen_flows
    if missing:
        raise ValueError("Flows ausentes no template: " + ", ".join(sorted(missing)))
    return customized


def build_komponent(preset: dict[str, Any]) -> dict[str, Any]:
    """Wrap the native root as a complete component for clipboard import."""

    root = preset["preset_root"]
    return {
        "internal_type": "KomponentModule",
        "internal_title": "ghinfo Activity Widget",
        "globals_list": copy.deepcopy(root["globals_list"]),
        "internal_flows": copy.deepcopy(root["internal_flows"]),
        "viewgroup_items": copy.deepcopy(root["viewgroup_items"]),
    }


def render_clip(preset: dict[str, Any]) -> str:
    """Serialize a complete native ``##KUSTOMCLIP##`` component payload."""

    clip_data = {
        "clip_version": 1,
        "clip_cut": [],
        "clip_modules": [build_komponent(preset)],
    }
    body = json.dumps(clip_data, indent=2, ensure_ascii=False)
    return f"##KUSTOMCLIP##\n{body}\n##KUSTOMCLIP##\n"


def render_loose_clip(preset: dict[str, Any]) -> str:
    """Serialize layout-only modules for advanced/manual composition."""

    root = preset["preset_root"]
    clip_data = {
        "clip_version": 1,
        "clip_cut": [],
        "clip_modules": copy.deepcopy(root["viewgroup_items"]),
    }
    body = json.dumps(clip_data, indent=2, ensure_ascii=False)
    return f"##KUSTOMCLIP##\n{body}\n##KUSTOMCLIP##\n"


def _write_text(path: Path, content: str) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(content, encoding="utf-8")
    return path


def write_kwgt(
    preset: dict[str, Any],
    output_path: Path,
    template_path: Path = TEMPLATE_KWGT,
) -> Path:
    """Copy the template archive while replacing only its preset JSON."""

    if output_path.resolve() == template_path.resolve():
        raise ValueError("o arquivo de saída não pode sobrescrever o template")
    output_path.parent.mkdir(parents=True, exist_ok=True)
    preset_bytes = (json.dumps(preset, indent=2, ensure_ascii=False) + "\n").encode("utf-8")
    with zipfile.ZipFile(template_path) as source, zipfile.ZipFile(
        output_path, "w", compression=zipfile.ZIP_DEFLATED
    ) as destination:
        for info in source.infolist():
            data = preset_bytes if info.filename == "preset.json" else source.read(info.filename)
            destination.writestr(info, data)
    return output_path


def write_artifacts(
    preset: dict[str, Any],
    output_dir: Path,
    template_path: Path = TEMPLATE_KWGT,
) -> dict[str, Path]:
    """Write the complete clip, loose clip, and customized KWGT package."""

    output_dir.mkdir(parents=True, exist_ok=True)
    paths = {
        "clip": output_dir / "ghinfo-kustom-widget.clip",
        "loose_clip": output_dir / "ghinfo-kustom-widget-loose.clip",
        "kwgt": output_dir / "ghinfo-kustom-widget.kwgt",
    }
    if paths["kwgt"].resolve() == template_path.resolve():
        raise ValueError("o diretório de saída não pode ser o diretório do template")
    _write_text(paths["clip"], render_clip(preset))
    _write_text(paths["loose_clip"], render_loose_clip(preset))
    write_kwgt(preset, paths["kwgt"], template_path=template_path)
    return paths


def fetch_initial_activity(base_url: str, timeout: float = 5.0) -> tuple[Optional[dict[str, Any]], str]:
    """Best-effort snapshot seed; failure never prevents artifact generation."""

    url = normalize_base_url(base_url) + FLOW_PATHS["ghinfo"]
    request = urllib.request.Request(url, headers={"User-Agent": "ghinfo-kustom-setup/1.0"})
    try:
        with urllib.request.urlopen(request, timeout=timeout) as response:
            if response.status != 200:
                return None, f"HTTP {response.status}"
            payload = json.loads(response.read().decode("utf-8"))
    except urllib.error.HTTPError as exc:
        return None, f"HTTP {exc.code}"
    except (OSError, TimeoutError, ValueError, json.JSONDecodeError):
        return None, "endpoint indisponível ou resposta inválida"
    if not is_activity_payload(payload):
        return None, "resposta sem activity.items"
    return payload, "ok"


def copy_to_clipboard(content: str) -> Optional[str]:
    """Try Termux, Linux, and macOS clipboard providers without a shell."""

    commands = (
        ("termux-clipboard-set", ["termux-clipboard-set"]),
        ("wl-copy", ["wl-copy"]),
        ("xclip", ["xclip", "-selection", "clipboard"]),
        ("xsel", ["xsel", "--clipboard", "--input"]),
        ("pbcopy", ["pbcopy"]),
    )
    for label, command in commands:
        if shutil.which(command[0]) is None:
            continue
        try:
            subprocess.run(
                command,
                input=content.encode("utf-8"),
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                check=True,
            )
        except (OSError, subprocess.SubprocessError):
            continue
        return label
    return None
