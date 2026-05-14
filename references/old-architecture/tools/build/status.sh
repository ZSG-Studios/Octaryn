#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

printf 'Build status\n'

mapfile -t presets < <(octaryn_known_presets)

for preset in "${presets[@]}"; do
  log_dir="$(octaryn_shared_deps_log_dir "$preset")"
  if [[ -d "$log_dir" ]]; then
    latest_log="$(ls -1t "$log_dir" 2>/dev/null | head -n 1 || true)"
    if [[ -n "$latest_log" ]]; then
      printf '  shared deps latest %s log: %s/%s\n' "$preset" "$log_dir" "$latest_log"
    fi
  fi
done

printf '  %s\n' "$(octaryn_product_name)"
for preset in "${presets[@]}"; do
  log_dir="$(octaryn_product_log_dir "$preset")"
  if [[ -d "$log_dir" ]]; then
    latest_log="$(ls -1t "$log_dir" 2>/dev/null | head -n 1 || true)"
    if [[ -n "$latest_log" ]]; then
      printf '    latest %s log: %s/%s\n' "$preset" "$log_dir" "$latest_log"
    fi
  fi
done

if command -v sccache >/dev/null 2>&1; then
  sccache --show-stats
else
  printf '  sccache: not found\n'
fi
