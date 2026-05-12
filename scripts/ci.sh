#!/usr/bin/env bash
# Local CI gate — see EXECUTION.md §5.
# Fail-fast in cheap-first order.
set -euo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${repo_root}"

step() { printf '\n=== %s ===\n' "$*"; }

step "1/7  configure debug"
cmake --preset debug

step "2/7  build debug (warnings-as-errors)"
cmake --build --preset debug

step "3/7  clang-format dry-run"
mapfile -t fmt_files < <(git ls-files \
    'client/*.cpp' 'client/*.hpp' \
    'server/*.cpp' 'server/*.hpp')
if [[ ${#fmt_files[@]} -gt 0 ]]; then
    clang-format --dry-run -Werror "${fmt_files[@]}"
else
    echo "no source files yet"
fi

step "4/7  clang-tidy"
if [[ ${#fmt_files[@]} -gt 0 ]]; then
    run-clang-tidy -p build/debug -quiet -warnings-as-errors='*' \
        "${fmt_files[@]}" \
        | tee /tmp/tvshow-tidy.out
    if grep -E '(warning|error):' /tmp/tvshow-tidy.out >/dev/null; then
        echo "clang-tidy reported issues" >&2
        exit 1
    fi
else
    echo "no source files yet"
fi

step "5/7  ctest debug"
ctest --preset debug

step "6/7  asan: configure + build + ctest"
cmake --preset asan
cmake --build --preset asan
ctest --preset asan

step "7/7  release build"
cmake --preset release
cmake --build --preset release

printf '\nci.sh: ALL GATES GREEN\n'
