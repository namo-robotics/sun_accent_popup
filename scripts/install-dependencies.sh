#!/usr/bin/env bash
# Fetch published dependency packages and normalize their installed files for accent popup builds.
set -euo pipefail
cd "$(dirname "$0")/.."
asset_directory=
if [[ ${1:-} == --from-directory && $# == 2 ]]; then
  asset_directory=$(realpath "$2")
elif [[ $# != 0 ]]; then
  echo 'Usage: install-dependencies.sh [--from-directory DIRECTORY]' >&2
  exit 1
fi
./scripts/bootstrap-sun.sh
mkdir -p tmp build/dependencies
staging=$(mktemp -d "$PWD/tmp/releases.XXXXXX")
trap 'rm -rf "$staging"' EXIT
compiler_version=$("${SUN_EXECUTABLE:-sun}" --version 2>&1)
if [[ $(uname -s) != Linux || $(uname -m) != x86_64 ]]; then
  echo 'Sun Accent Popup release dependencies support Linux x86_64.' >&2
  exit 1
fi

# Keep both installations staged until all assets and compiler versions have been checked.
for dependency in sun_ui sun_gobject; do
  package=${dependency//_/-}
  if [[ $dependency == sun_ui ]]; then
    release=${SUN_UI_RELEASE:-dev}
  else
    release=${SUN_GOBJECT_RELEASE:-dev}
  fi
  asset="${package}_0.dev_amd64.deb"
  archive="$staging/$asset"
  if [[ -n $asset_directory ]]; then
    cp "$asset_directory/$asset" "$archive"
  else
    url="https://github.com/namo-robotics/$dependency/releases/download/$release/$asset"
    if ! curl --fail --location --retry 3 "$url" --output "$archive"; then
      echo "Missing published dependency: $url. No source-build fallback is used." >&2
      exit 1
    fi
  fi
  unpack="$staging/unpack-$dependency"
  mkdir -p "$unpack"
  dpkg-deb --extract "$archive" "$unpack"
  prefix="$unpack/usr"
  notices="$prefix/share/doc/$package"
  license=copyright
  if [[ $(cat "$notices/sun-version.txt") != "$compiler_version" ]]; then
    echo "$dependency release requires the compiler recorded in its sun-version.txt:" >&2
    cat "$notices/sun-version.txt" >&2
    echo "Installed compiler: $compiler_version" >&2
    exit 1
  fi
  target="$staging/$dependency"
  mkdir -p "$target/build"
  cp "$prefix/lib/sun/"*.moon "$target/build/"
  cp "$notices/$license" "$target/LICENSE"
  cp "$notices/sun-version.txt" "$target/build/sun-version.txt"
  printf '%s\n' "$release/$asset" > "$target/release.txt"
  if [[ $dependency == sun_ui ]]; then
    test -f "$target/build/sun_ui.moon"
    test -f "$target/build/sun_ui_gtk.moon"
    install -m 755 "$prefix/bin/sun-viewc" "$target/build/sun-viewc"
  else
    test -f "$target/build/sun_gobject.moon"
    test -f "$target/build/sun_gtk.moon"
    mkdir -p "$target/build/native"
    native="$prefix/lib/sun/native/libsun_gobject_native.a"
    cp "$native" "$target/build/native/"
  fi
done
for dependency in sun_ui sun_gobject; do
  rm -rf "build/dependencies/$dependency"
  mv "$staging/$dependency" "build/dependencies/$dependency"
done
echo 'Published dependency bundles, native library, and sun-viewc are ready.'
