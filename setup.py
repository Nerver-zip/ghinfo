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
            print(f"URL inválida: {exc}")
            candidate = input(f"? ghinfo URL [{default}]: ").strip() or default


def prompt_refresh(value: int | None) -> int:
    default = os.environ.get("GHINFO_WIDGET_REFRESH_MINUTES") or str(DEFAULT_REFRESH_MINUTES)
    candidate = str(value) if value is not None else input(f"? Intervalo de atualização em minutos [{default}]: ").strip() or default
    while True:
        try:
            return validate_refresh_minutes(candidate)
        except ValueError as exc:
            print(f"Intervalo inválido: {exc}")
            candidate = input(f"? Intervalo de atualização em minutos [{default}]: ").strip() or default


def main() -> int:
    parser = argparse.ArgumentParser(description="Gera um clip Kustom personalizado para o ghinfo.")
    parser.add_argument("--url", help="URL base do ghinfo (ex.: http://100.118.53.107:8080)")
    parser.add_argument("--refresh-minutes", type=int, help="intervalo do Flow principal, de 1 a 59 minutos")
    parser.add_argument("--output-dir", default="dist", help="diretório de saída (padrão: dist)")
    parser.add_argument("--no-probe", action="store_true", help="não consultar o snapshot para pré-popular o widget")
    parser.add_argument("--no-clipboard", action="store_true", help="não tentar copiar o clip para a área de transferência")
    args = parser.parse_args()

    print("╔══════════════════════════════════════════════════════════╗")
    print("║                    ghinfo Kustom Setup                   ║")
    print("╚══════════════════════════════════════════════════════════╝")
    print("O assistente não pede nem manipula token do GitHub.\n")

    base_url = prompt_url(args.url)
    refresh_minutes = prompt_refresh(args.refresh_minutes)
    initial_payload = None
    if not args.no_probe:
        print(f"\nConsultando snapshot em {base_url}...")
        initial_payload, reason = fetch_initial_activity(base_url)
        if initial_payload is None:
            print(f"Aviso: {reason}; o widget iniciará com cache vazio.")
        else:
            count = len(initial_payload["activity"]["items"])
            generation = initial_payload.get("generation", 0)
            print(f"✓ Snapshot encontrado: geração {generation}, {count} item(ns).")
    if initial_payload is None:
        initial_payload = EMPTY_ACTIVITY

    print("\nGerando artefatos...")
    preset = customize_preset(load_template_preset(TEMPLATE_KWGT), base_url, refresh_minutes, initial_payload)
    paths = write_artifacts(preset, Path(args.output_dir).expanduser())
    print(f"✓ Clip completo: {paths['clip']}")
    print(f"✓ Clip somente layout: {paths['loose_clip']}")
    print(f"✓ Pacote Premium: {paths['kwgt']}")

    if not args.no_clipboard:
        tool = copy_to_clipboard(paths["clip"].read_text(encoding="utf-8"))
        if tool:
            print(f"✓ Clip completo copiado para a área de transferência via {tool}.")
        else:
            print("ℹ Nenhum utilitário de clipboard encontrado; use o arquivo .clip manualmente.")

    print("\nNo Android: crie um KWGT vazio, abra o editor, use Adicionar → Komponent e")
    print("volte uma tela para o KWGT oferecer 'Paste Komponent from Clipboard'.")
    print("Depois salve o widget. O arquivo começa e termina com ##KUSTOMCLIP##.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except KeyboardInterrupt:
        print("\nCancelado.")
        raise SystemExit(130)
