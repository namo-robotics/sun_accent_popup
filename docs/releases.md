# Debian and Snap releases

Run all commands from the root of a standalone `sun_accent_popup` checkout.

## Debian installation

`./build.sh --package` builds and tests the application, stages both formats, and
creates `build/sun-accent-popup_0.1.0_amd64.deb` for Ubuntu 26.04 amd64. From the
repository root on your desktop, outside the container:

```sh
sudo apt install --reinstall ./build/sun-accent-popup_0.1.0_amd64.deb
```

The package installs `/usr/bin/sun_accent_popup`, its desktop launcher, and the
corrected `/usr/share/ibus/component/sun-accent-popup.xml`. Apt installs IBus and
the linked runtime libraries. Log out and back in, then select Sun Accent Popup
in GNOME Keyboard settings. No manual component installation is needed.

If migrating from `scripts/install.sh`, the unmanaged old executable and desktop
launcher under `/usr/local` can shadow the packaged ones. Remove those two old
files after installing the Debian package:

```sh
sudo rm /usr/local/bin/sun_accent_popup \
  /usr/local/share/applications/sun-accent-popup.desktop
```

Do not remove the component under `/usr/share/ibus/component`: it is now owned by
the Debian package. If migrating from Snap, remove the Snap before installing the
Debian package so that only one engine remains active.

CI publishes tested Debian packages to the GitHub `dev` prerelease on pushes to
`main`. Once the first package has been published:

```sh
curl -fL --output sun-accent-popup-dev.deb \
  https://github.com/namo-robotics/sun_accent_popup/releases/download/dev/sun-accent-popup_0.1.0_amd64.deb
sudo apt install --reinstall ./sun-accent-popup-dev.deb
```

To uninstall, select another input source, remove Sun Accent Popup from GNOME's
input-source list, run `sudo apt remove sun-accent-popup`, and log out and back in.
The package manager removes its registered component automatically.

## Snap packaging

The package name is `sun-accent-popup`; the native executable remains
`sun_accent_popup`. The Snap targets amd64 with the `core26` base and GNOME
extension. Its version is declared in `snap/snapcraft.yaml`; keep it consistent
with both IBus component XML files and the registration version in
`native/ibus_bridge.c` when changing versions.

## Build and test

From this repository root in the development container, prepare the dependencies
as described in the README, then run:

```sh
./build.sh --package
```

This rebuilds the app, runs the Sun, GTK, and isolated IBus tests, and stages
`build/accent-package/root` and the Debian package. Additional private IBus sessions check that each
packaged component discovers and launches the engine wrapper and passes the
native typing tests. No host input source is changed.

On a host with Snapcraft installed, from the same repository root:

```sh
snapcraft
sudo snap install ./sun-accent-popup_0.1.0_amd64.snap --dangerous
sudo install -Dm644 \
  /snap/sun-accent-popup/current/usr/share/ibus/component/sun-accent-popup.xml \
  /usr/share/ibus/component/sun-accent-popup.xml
```

Log out and back in, then add and select **English → Sun Accent Popup** in GNOME
Keyboard settings. The component launches `/snap/bin/sun-accent-popup.engine`,
which follows Snap refreshes automatically. The practice window is launched with
`snap run sun-accent-popup` and need not stay open.

The host registration command also replaces the component from a previous source
installation. Avoid running the source engine and Snap engine simultaneously.

## Desktop integration and confinement

The engine uses strict confinement and the `desktop-legacy` interface to reach
the host IBus socket. Its wrapper reads the host IBus address from the user's
real configuration directory rather than the Snap's private configuration.
The practice window uses the GNOME extension and a D-Bus slot matching its GTK
application ID. The engine does not run as a root or system service.

The Snap cannot install its IBus component into the host's registry directory.
The explicit registration command above is therefore required. IBus currently
loads its system component directory by default; a component under a Snap's
private data directory is not enough. Registration is a host operation, separate
from Snap confinement, and does not grant the engine arbitrary filesystem access.

References: [snapd desktop-legacy policy](https://github.com/canonical/snapd/blob/master/interfaces/builtin/desktop_legacy.go)
and [IBus component discovery](https://github.com/ibus/ibus/blob/main/src/ibusregistry.c).

Before promoting a release, test the installed **strict** Snap on a real GNOME
Wayland desktop. Check ordinary typing, held keys, candidate selection, focus
changes, private fields, password fields, the practice window, and operation
after logout/login and refresh. Confirm another application's typing works;
opening the practice window alone is not sufficient. The isolated payload tests
do not test Snap's AppArmor policy or the GNOME session.

If the engine cannot connect, inspect `snap connections sun-accent-popup` and
connect `desktop-legacy` if necessary:

```sh
sudo snap connect sun-accent-popup:desktop-legacy
snap run sun-accent-popup.engine
```

Run this diagnostic with Sun Accent Popup deselected so it does not compete with
an already-running engine. Check `journalctl -k` for AppArmor denials and the
terminal for IBus errors. Do not use devmode as the release verification.

## Enable Store publication

Register the Snap name with your publisher account and generate credentials
restricted to this package and the `edge` channel:

```sh
snapcraft login
snapcraft register sun-accent-popup
mkdir -p tmp
snapcraft export-login --snaps=sun-accent-popup --channels=edge \
  --acls=package_access,package_push,package_update,package_release \
  tmp/snap-store-credentials
```

Add the file contents as `SNAPCRAFT_STORE_CREDENTIALS` in the
[repository Actions secrets](https://github.com/namo-robotics/sun_accent_popup/settings/secrets/actions),
then remove the local credential file. Never commit it or paste it into logs.
Store registration, any required review, and the credentials must be ready
before publication can succeed. See [Snapcraft authentication](https://snapcraft.io/docs/snapcraft-authentication/).

CI builds and tests the application, archives the tested payload with its
permissions, and builds a Snap. Pull requests only build artifacts. Successful
pushes to `main` upload to `edge` when the secret is present; otherwise CI reports
that publication was skipped. Adding this configuration does not itself register
the name or publish the current worktree.

After testing the edge build on a desktop, promote its revision using your
interactive account; the CI credentials cannot publish to stable:

```sh
snapcraft login
snapcraft status sun-accent-popup
snapcraft release sun-accent-popup REVISION stable
```

Once stable is published, users can omit `--edge` when installing. Host IBus
registration is still needed after the first install.

## Remove

Select another input source and remove Sun Accent Popup from GNOME's input-source
list. Remove the component installed by the Snap setup and the Snap itself:

```sh
sudo rm /usr/share/ibus/component/sun-accent-popup.xml
sudo snap remove sun-accent-popup
```

Log out and back in. Snap removal alone cannot remove the separately installed
host component. If switching back to a source installation, its installer can
restore the component with the source executable path.
