#!/usr/bin/env bash

set -euo pipefail

source "$(dirname "${BASH_SOURCE[0]}")/layout.sh"

repair="0"
install_missing="0"
preset=""

is_git_repo() {
  local dir="$1"
  [[ -d "$dir" ]] || return 1
  git -C "$dir" rev-parse --is-inside-work-tree >/dev/null 2>&1
}

report_tool() {
  local label="$1"
  local binary="$2"
  if command -v "$binary" >/dev/null 2>&1; then
    printf '[ok] %s: %s\n' "$label" "$(command -v "$binary")"
  else
    printf '[warn] %s: not found on PATH\n' "$label"
  fi
}

report_python_module() {
  local label="$1"
  local module="$2"
  if python3 -c "import ${module}" >/dev/null 2>&1; then
    printf '[ok] %s: importable\n' "$label"
  else
    printf '[warn] %s: not importable by python3\n' "$label"
  fi
}

report_dotnet_sdk() {
  if ! command -v dotnet >/dev/null 2>&1; then
    return
  fi
  if dotnet --list-sdks | grep -q '^10\.'; then
    printf '[ok] .NET SDK 10: installed\n'
  else
    printf '[warn] .NET SDK 10: missing; managed runtime targets net10.0\n'
  fi
}

repair_git_dir() {
  local label="$1"
  local dir="$2"
  if [[ ! -e "$dir" ]]; then
    printf '[info] %s cache absent: %s\n' "$label" "$dir"
    return
  fi
  if is_git_repo "$dir"; then
    printf '[ok] %s cache healthy: %s\n' "$label" "$dir"
    return
  fi
  printf '[warn] %s cache invalid: %s\n' "$label" "$dir"
  if [[ "$repair" == "1" ]]; then
    rm -rf -- "$dir"
    printf '[fix] removed invalid %s cache\n' "$label"
  fi
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --install)
      install_missing="1"
      shift
      ;;
    --repair)
      repair="1"
      shift
      ;;
    --preset)
      preset="$2"
      shift 2
      ;;
    *)
      printf 'unknown argument: %s\n' "$1" >&2
      exit 2
      ;;
  esac
done

printf 'Octaryn build doctor\n'
printf 'workspace: %s\n' "$(octaryn_workspace_root)"

if [[ "$install_missing" == "1" ]]; then
  bash "$(octaryn_workspace_root)/references/old-architecture/tools/build/install_deps.sh" --yes
  octaryn_source_toolchain_env
fi

if [[ "$repair" == "1" ]]; then
  octaryn_acquire_build_lock "doctor repair"
fi

report_tool "cmake" cmake
report_tool "ninja" ninja
report_tool "clang" clang
report_tool "lld" ld.lld
report_tool "git" git
report_tool "pkg-config" pkg-config
report_tool "python3" python3
report_tool "dotnet" dotnet
report_dotnet_sdk
report_tool "MinGW C" x86_64-w64-mingw32-gcc
report_tool "MinGW C++" x86_64-w64-mingw32-g++
report_tool "MinGW windres" x86_64-w64-mingw32-windres
report_tool "sccache" sccache
report_python_module "PySide6" PySide6

spirv_tools_dir="$(octaryn_shared_cpm_cache_dir)/spirv-tools/octaryn-spirv-tools-src"
shaderc_dir="$(octaryn_shared_cpm_cache_dir)/shaderc/octaryn-shaderc-src"
glslang_dir="$(octaryn_shared_cpm_cache_dir)/glslang/octaryn-glslang-src"
spirv_headers_dir="$(octaryn_shared_cpm_cache_dir)/spirv-headers/octaryn-spirv-headers-src"

repair_git_dir "SPIRV-Tools" "$spirv_tools_dir"
repair_git_dir "shaderc" "$shaderc_dir"
repair_git_dir "glslang" "$glslang_dir"
repair_git_dir "SPIRV-Headers" "$spirv_headers_dir"

if [[ -n "$preset" ]]; then
  if ! octaryn_known_presets | grep -qx "$preset"; then
    printf 'unknown preset: %s\n' "$preset" >&2
    exit 2
  fi
  deps_dir="$(octaryn_shared_deps_root "$preset")"
  workspace_dir="$(octaryn_workspace_preset_root "$preset")"
  printf '[info] preset %s deps dir: %s\n' "$preset" "$deps_dir"
  printf '[info] preset %s workspace dir: %s\n' "$preset" "$workspace_dir"
  if [[ "$repair" == "1" ]]; then
    rm -rf -- "$deps_dir" "$workspace_dir"
    printf '[fix] removed preset state for %s\n' "$preset"
  fi
fi

printf 'Doctor complete\n'
