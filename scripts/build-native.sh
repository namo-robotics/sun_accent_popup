#!/usr/bin/env bash
# Build the IBus bridge and acceptance client in the selected Sun build directory.
set -euo pipefail
native_output=$1
mkdir -p "$native_output/native"
read -r -a native_cflags <<< "$(pkg-config --cflags ibus-1.0)"
read -r -a native_libs <<< "$(pkg-config --libs ibus-1.0)"
cc -Wall -Wextra -Werror -g "${native_cflags[@]}" -c native/ibus_bridge.c -o "$native_output/native/ibus_bridge.o"
ar rcs "$native_output/native/libaccent_ibus.a" "$native_output/native/ibus_bridge.o"
cc -Wall -Wextra -Werror -g "${native_cflags[@]}" tests/ibus_client.c "${native_libs[@]}" -o "$native_output/ibus-client"
