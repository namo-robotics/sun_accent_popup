#!/usr/bin/env bash
# Test packaged component discovery and engine launch on an isolated desktop bus.
set -euo pipefail
if [[ ${1:-} == --deb ]]; then export ACCENT_PACKAGE_KIND=deb; shift; fi
cd "$(dirname "$0")/.."
if [[ ${1:-} != --session ]]; then
  exec timeout 40s dbus-run-session -- xvfb-run -a bash scripts/test-snap-payload.sh --session < /dev/null
fi
mkdir -p tmp/snap-test
test_directory=$(mktemp -d "$PWD/tmp/snap-test/run.XXXXXX")
unset WAYLAND_DISPLAY
export SNAP="$PWD/build/accent-package/root"
if [[ ${ACCENT_PACKAGE_KIND:-snap} == deb ]]; then export SNAP="$PWD/build/accent-deb/root"; fi
export SNAP_REAL_HOME="$test_directory/home"
export XDG_CONFIG_HOME="$SNAP_REAL_HOME/.config"
export XDG_CACHE_HOME="$test_directory/cache"
export IBUS_COMPONENT_PATH="$test_directory/components"
export IBUS_ADDRESS="unix:path=$test_directory/bus"
ulimit -c 0
export G_DEBUG=fatal-criticals
export IBUS_TIMEOUT=2000
mkdir -p "$IBUS_COMPONENT_PATH" "$XDG_CONFIG_HOME" "$XDG_CACHE_HOME"
python3 - <<'PY'
"""Validate the packaged component and adapt its launcher to an isolated test."""
import os
from pathlib import Path
import shlex
import xml.etree.ElementTree as ET
payload = Path(os.environ['SNAP'])
component = ET.parse(payload / 'usr/share/ibus/component/sun-accent-popup.xml')
deb = os.environ.get('ACCENT_PACKAGE_KIND') == 'deb'
assert component.findtext('exec') == ('/usr/bin/sun_accent_popup --ibus' if deb else '/snap/bin/sun-accent-popup.engine')
assert component.findtext('engines/engine/name') == 'sun-accent-popup'
if not deb:
    assert os.access(payload / 'bin/engine', os.X_OK)
assert os.access(payload / 'usr/bin/sun_accent_popup', os.X_OK)
# Force discovery through the host IBus address file rather than inherited IBUS_ADDRESS.
component.find('exec').text = '/usr/bin/env -u IBUS_ADDRESS SNAP=' + shlex.quote(str(payload)) + ' SNAP_REAL_HOME=' + shlex.quote(os.environ['SNAP_REAL_HOME']) + ' ' + shlex.quote(str(payload / 'bin/engine'))
if deb:
    component.find('exec').text = shlex.quote(str(payload / 'usr/bin/sun_accent_popup')) + ' --ibus'
component.write(Path(os.environ['IBUS_COMPONENT_PATH']) / 'sun-accent-popup.xml')
PY
env -u SNAP -u SNAP_REAL_HOME ibus-daemon --verbose --address="$IBUS_ADDRESS" --replace --panel disable --config disable --emoji-extension disable > tmp/snap-test/daemon.log 2>&1 &
daemon_pid=$!
# Stop the private daemon and any engine it launched even when the client fails.
cleanup() {
  local status=$?
  if [[ $status -ne 0 ]]; then cat tmp/snap-test/daemon.log tmp/snap-test/client.log >&2; fi
  G_DEBUG= timeout 2s ibus exit >/dev/null 2>&1 || true
  kill "$daemon_pid" 2>/dev/null || true
  wait "$daemon_pid" 2>/dev/null || true
}
trap cleanup EXIT
ready=false
for attempt in $(seq 1 5); do
  if G_DEBUG= timeout 3s ibus engine sun-accent-popup >tmp/snap-test/client.log 2>&1; then ready=true; break; fi
  if ! kill -0 "$daemon_pid" 2>/dev/null; then echo "Private IBus daemon exited." >&2; exit 1; fi
  sleep 0.05
done
if [[ $ready != true ]]; then echo 'Packaged IBus engine failed to activate.' >&2; exit 1; fi
./build/ibus-client
printf '%s\n' 'Package registration and native typing acceptance passed.'
