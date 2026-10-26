#!/usr/bin/env bash
# Build and test the independent accent popup application.
set -euo pipefail
cd "$(dirname "$0")"
if [[ $# -gt 1 ]]; then echo 'Usage: ./build.sh [--build|--test|--package]' >&2; exit 1; fi
./scripts/bootstrap-sun.sh
case "${1:-}" in
  "") "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- build; "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- test ;;
  --build|--test|--package) "${SUN_EXECUTABLE:-sun}" src/tools/build.sun -- "${1#--}" ;;
  *) echo 'Usage: ./build.sh [--build|--test|--package]' >&2; exit 1 ;;
esac
