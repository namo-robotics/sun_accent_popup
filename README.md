# Sun Accent Popup

A hold-to-accent input method for Ubuntu/GNOME, built in Sun. Select **Sun Accent
Popup** as your input source, then hold a supported letter in an IBus-enabled
application to open accent choices at the cursor.

- Tap normally for the plain letter. Hold for half a second to open the menu.
- Choose with a number, mouse click, or Left/Right followed by Enter or Space.
- Escape keeps the plain letter; Backspace removes the pending letter.
- Release the held letter before choosing if that is more comfortable.
- Uppercase input produces uppercase candidates. Ctrl/Alt/Super and AltGr combinations pass through.
- Supported letters: **a c e i n o s u y z**, including uppercase.

The input method uses IBus composition and candidate panels, so it does not
steal focus or modify the clipboard. The separate `sun_ui` practice window is
an ordinary client; closing it does not stop the input method.

## Build

The dev container provides Sun, GTK, IBus, and the test dependencies. For a native
Ubuntu setup, install Sun with `./scripts/bootstrap-sun.sh` and install GTK 4,
`libibus-1.0-dev`, `ibus`, `pkg-config`, a C compiler, Xvfb, xauth, and dbus-x11.
The Sun compiler and all dependency bundles must be built with compatible versions.

From this repository root:

```sh
vcs import --input deps.repos .
./scripts/build-dependencies.sh
./build.sh
```

`deps.repos` imports only `sun_ui` and `sun_gobject`. Do not recursively import
sun_ui's manifest, which includes this repository as an optional application.

When checked out inside sun_ui, reuse the parent's dependency builds. From the
sun_ui repository root:

```sh
SUN_UI_BUILD="$PWD/build" SUN_GOBJECT_BUILD="$PWD/src/sun_gobject/build" ./src/sun_accent_popup/build.sh
```

`./build.sh --build` builds only; `./build.sh --test` tests existing binaries.
Products and native bridge paths are declared in `sun-config.json`; the build
orchestration lives in `src/tools/`. Builds do not install or enable the input method.

## Install and enable

After building and testing, from this repository root:

```sh
sudo ./scripts/install.sh
```

Log out and back in so IBus discovers the component. In **GNOME Settings →
Keyboard → Input Sources**, add **English → Sun Accent Popup** (search for its
name if needed). Select it from GNOME's input-source menu or with Super+Space.
Open any compatible text application and hold **e**, then press **2** for **é**.

Run `sun_accent_popup` to open the practice window. The installed IBus component
starts `sun_accent_popup --ibus` when GNOME selects the input method; it does not
need a separate autostart service. `--register` is available for temporary
uninstalled development sessions and is used by the isolated acceptance tests.

The installer writes `/usr/local/bin/sun_accent_popup`,
`/usr/share/ibus/component/sun-accent-popup.xml`, and
`/usr/local/share/applications/sun-accent-popup.desktop`. `DESTDIR` can stage
those same files without installing them. To uninstall, select another input
source, remove Sun Accent Popup from GNOME's list, delete those three files,
and log out and back in.

## Compatibility

The first implementation targets Ubuntu/GNOME and IBus-aware applications on
Wayland or X11. Applications that bypass desktop input methods will not receive
this behavior. On X11, legacy clients may also need the normal IBus XIM setup.
The engine selects the US QWERTY layout; other layouts and other operating systems
are not implemented.

The plain letter stays in preedit until release or another key is pressed. The
engine explicitly requests no underline for that temporary text; applications
may still apply their own composition styling. A hold replaces that composition
with the chosen accent. Focus loss and input
context resets discard pending composition rather than delivering it to a new
application. Password/PIN fields bypass the picker when the client reports their content
type. Private browsing fields support accents: the private hint prohibits
personalized learning, and this service never records typed text.

IBus owns the candidate panel's appearance, including GNOME's theme. Shortcut
numbers are displayed alongside the accents so both share the selected text
color; selecting a candidate inserts only the accent. The menu is
not a custom floating GTK window. The `sun_ui` practice window and state model
remain independent of native GTK handles.

## VS Code on GNOME Wayland

If accents work in Chrome or GNOME Text Editor but VS Code repeats the letter,
try enabling Electron's native Wayland input-method support. Save your work and
fully exit VS Code, then launch it from a host desktop terminal:

```sh
code --ozone-platform=wayland --enable-wayland-ime --wayland-text-input-version=3
```

An existing VS Code process can reuse its original startup configuration, so
opening another window alone is not enough. These flags depend on the bundled
Electron/Chromium version; this is a diagnostic launch, not a confirmed fix for
every VS Code build. If it still fails, record `code --version` and compare input
in the editor with input in the integrated terminal.

The flags are documented in [Chromium's Ozone switches](https://github.com/chromium/chromium/blob/main/ui/ozone/public/ozone_switches.cc).

## Tests and architecture

`./build.sh` runs the pure Sun catalogue/state-machine tests, the GTK practice
window acceptance check, and a separate IBus client against a private daemon.
The protocol test verifies real timeout delivery, Unicode commits, auto-repeat
suppression, case, shortcuts, resets, sensitive fields, and two input contexts.
It does not change your desktop input source. Automated protocol tests do not
replace a visual check in a real GNOME Wayland session.

- `src/state.sun`: accent catalogue and input state machine.
- `src/platform/ibus.sun`: narrow native boundary using owned state snapshots.
- `native/ibus_bridge.c`: IBus subclass, timers, lifetime, and text transport.
- `src/view.sun`: sun_ui practice window.
- `tests/`: pure Sun tests and the independent native protocol client.
- `packaging/`: IBus component and desktop launcher.

GTK ownership stays in sun_gobject. This application requires no changes to
sun_ui or sun_gobject APIs. CI builds and tests the Linux application; it does
not publish releases or change desktop settings.

Protocol references: [IBus engine API](https://github.com/ibus/ibus/blob/main/src/ibusengine.h),
[IBus component discovery](https://github.com/ibus/ibus/blob/main/src/ibusregistry.c).
