#!/usr/bin/env bash
# Build configured Git dependencies and retain their license notices for packaging.
set -euo pipefail
cd "$(dirname "$0")/.."
if (($#)); then
  echo 'Usage: install-dependencies.sh' >&2
  exit 1
fi
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
  echo 'Sun Accent Popup supports Linux x86_64.' >&2
  exit 1
fi
./scripts/bootstrap-sun.sh
compiler=${SUN_EXECUTABLE:-sun}
for dependency in sun_ui sun_gobject; do
  # Resolve emits build progress before its final JSON record.
  resolution=$("$compiler" resolve --config sun-config.json --dependency "namo-robotics/$dependency")
  root=$(python3 -c 'import json, sys
lines = sys.stdin.read().splitlines()
for line in lines:
    print(line, file=sys.stderr)
print(json.loads(lines[-1] if lines else "")["root"])' <<< "$resolution")
  mkdir -p "build/dependencies/$dependency"
  install -m 644 "$root/share/$dependency/LICENSE" "build/dependencies/$dependency/LICENSE"
done
echo 'Git dependencies are built and license notices are ready.'
