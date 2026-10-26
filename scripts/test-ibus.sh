#!/usr/bin/env bash
# Exercise a private IBus session without modifying the user's desktop input sources.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ ${1:-} != --session ]]; then
  exec timeout 30s dbus-run-session -- xvfb-run -a bash scripts/test-ibus.sh --session < /dev/null
fi
mkdir -p tmp/ibus-test
test_directory=$(mktemp -d "$PWD/tmp/ibus-test/run.XXXXXX")
unset WAYLAND_DISPLAY
export XDG_CACHE_HOME="$test_directory/cache"
export XDG_CONFIG_HOME="$test_directory/config"
export G_DEBUG=fatal-criticals
ulimit -c 0
export IBUS_ADDRESS="unix:path=$test_directory/bus"
ibus-daemon --address="$IBUS_ADDRESS" --replace --panel disable --config disable --emoji-extension disable > tmp/ibus-test/daemon.log 2>&1 &
daemon_pid=$!
engine_pid=
: > tmp/ibus-test/engine.log
# Release only this test's private processes and preserve failure diagnostics.
cleanup() {
  local status=$?
  if [[ $status -ne 0 ]]; then cat tmp/ibus-test/engine.log tmp/ibus-test/daemon.log >&2; fi
  if [[ -n $engine_pid ]]; then
    kill "$engine_pid" 2>/dev/null || true
    wait "$engine_pid" 2>/dev/null || true
  fi
  kill "$daemon_pid" 2>/dev/null || true
  wait "$daemon_pid" 2>/dev/null || true
}
trap cleanup EXIT
for attempt in $(seq 1 100); do
  if ibus list-engine >/dev/null 2>&1; then break; fi
  if ! kill -0 "$daemon_pid" 2>/dev/null; then exit 1; fi
  sleep 0.05
done
./build/sun_accent_popup --register > tmp/ibus-test/engine.log 2>&1 &
engine_pid=$!
for attempt in $(seq 1 100); do
  if ibus engine sun-accent-popup >/dev/null 2>&1; then break; fi
  if ! kill -0 "$engine_pid" 2>/dev/null; then cat tmp/ibus-test/engine.log >&2; exit 1; fi
  sleep 0.05
done
./build/ibus-client
