#!/usr/bin/env bash
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

subcommand="${1:-run}"

case "$subcommand" in
  run)
    exec ai-jail --exec --terminal-passthrough -- \
      codex --dangerously-bypass-approvals-and-sandbox "${@:2}"
    ;;
  resume)
    exec ai-jail --exec --terminal-passthrough -- \
      codex --dangerously-bypass-approvals-and-sandbox resume "${@:2}"
    ;;
  resume:last)
    exec ai-jail --exec --terminal-passthrough -- \
      codex --dangerously-bypass-approvals-and-sandbox resume --last "${@:2}"
    ;;
  help|--help|-h)
    cat <<EOF
Usage: $0 [command] [args...]

Commands:
  run              Launch a new Codex session inside ai-jail (default)
  resume [id]      Resume a specific session by ID
  resume:last      Resume the most recent session
  help             Show this help message
EOF
    exit 0
    ;;
  *)
    # Pass any unrecognized commands/flags directly to codex inside ai-jail
    exec ai-jail --no-status-bar -- codex --dangerously-bypass-approvals-and-sandbox "$@"
    ;;
esac
