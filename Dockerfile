FROM ghcr.io/namo-robotics/sun-dev:ubuntu26.04-llvm20
USER root
# Add the application's native libraries and isolated desktop test tools.
RUN apt-get update && apt-get install -y --no-install-recommends \
    libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libx11-dev libxext-dev \
    libgtk-4-dev libibus-1.0-dev ibus librsvg2-2 fonts-dejavu-core libgirepository1.0-dev gobject-introspection \
    xvfb xauth dbus-x11 \
    && rm -rf /var/lib/apt/lists/*
COPY scripts/bootstrap-sun.sh /usr/local/bin/install-sun-dev
RUN bash /usr/local/bin/install-sun-dev --update
# Install transitive system requirements before running builds as an unprivileged user.
COPY sun-config.json /tmp/sun-dependencies/sun-config.json
RUN cd /tmp/sun-dependencies && apt-get update \
    && GIT_CONFIG_COUNT=1 GIT_CONFIG_KEY_0=url.https://github.com/.insteadOf \
       GIT_CONFIG_VALUE_0=git@github.com: sun dep install --config sun-config.json --yes \
    && cd / && rm -rf /tmp/sun-dependencies /var/lib/apt/lists/*
USER ubuntu
WORKDIR /workspaces/sun_accent_popup
