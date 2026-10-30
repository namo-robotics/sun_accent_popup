# Sun Accent Popup

Hold a letter to choose an accent, like on macOS. Type accented characters on a
US QWERTY keyboard in compatible Ubuntu/GNOME applications.

![Hold a key to choose an accented character](demo.gif)

Hold **e**, then press **2** for **é**. You can also click an accent or use the
arrow keys and Enter. Press Escape to keep the plain letter. Supports
**a c e i n o s u y z**, including uppercase; ordinary typing stays unchanged.

## Install · Ubuntu 26.04, x86_64

Download the development release and install it on your desktop, outside the container:

```sh
curl -fL --output sun-accent-popup-dev.deb \
  https://github.com/namo-robotics/sun_accent_popup/releases/download/dev/sun-accent-popup_0.1.0_amd64.deb &&
  sudo apt install --reinstall ./sun-accent-popup-dev.deb
```

Log out and back in, then add and select **English → Sun Accent Popup** in
**Settings → Keyboard → Input Sources**. Requires an IBus-compatible application
on GNOME Wayland or X11. Run `sun_accent_popup` for the optional practice window.

## Build from source

Open this repository in VS Code and select **Dev Containers: Reopen in Container**.
From its root inside the container:

```sh
./scripts/install-dependencies.sh
./build.sh --package
```

From the same directory on your desktop, outside the container:

```sh
sudo apt install --reinstall ./build/sun-accent-popup_0.1.0_amd64.deb
```

Enable the input source as described above.

See [development and troubleshooting](docs/development.md) or
[releases, Snap, and migration](docs/releases.md) for details.

## Shared development container

See [.devcontainer/README.md](.devcontainer/README.md) for the applied Sun template
version, project-specific settings, and rebuild instructions.
