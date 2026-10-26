#!/usr/bin/env bash
# Install the built application and IBus registration, optionally under a packaging staging root.
set -euo pipefail
cd "$(dirname "$0")/.."
if [[ $# -ne 0 ]]; then echo 'Usage: [DESTDIR=/staging/root] ./scripts/install.sh' >&2; exit 1; fi
if [[ ! -x build/sun_accent_popup ]]; then echo 'Run ./build.sh first.' >&2; exit 1; fi
install -Dm755 build/sun_accent_popup "${DESTDIR:-}/usr/local/bin/sun_accent_popup"
install -Dm644 packaging/sun-accent-popup.xml "${DESTDIR:-}/usr/share/ibus/component/sun-accent-popup.xml"
install -Dm644 packaging/sun-accent-popup.desktop "${DESTDIR:-}/usr/local/share/applications/sun-accent-popup.desktop"
echo 'Installed. Log out and back in, then add English → Sun Accent Popup in GNOME Settings → Keyboard → Input Sources.'
