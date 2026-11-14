#!/usr/bin/env bash
set -euo pipefail
export PATH="$PATH:/usr/bin"
# Usage: build-deps.sh CACHE PREFIX [linux|windows]. Windows runs in an MSVC
# developer environment with MSYS2 make, nasm, cmake and ninja on PATH.
cache=$(realpath -m "${1:?cache directory required}")
prefix=$(realpath -m "${2:?install prefix required}")
target=${3:-linux}
mkdir -p "$cache" "$prefix"
fetch() {
  local name=$1 url=$2 hash=$3
  if [[ ! -f "$cache/$name" ]]; then curl --fail --location "$url" -o "$cache/$name.partial"; mv "$cache/$name.partial" "$cache/$name"; fi
  echo "$hash  $cache/$name" | sha256sum --check --status
  tar -xf "$cache/$name" -C "$cache"
}
fetch SDL3-3.4.16.tar.gz https://github.com/libsdl-org/SDL/releases/download/release-3.4.16/SDL3-3.4.16.tar.gz 7322236cd12090c3eb40b9728be4d49c76f66ad17d04369584d4ecad5cf77c68
fetch ffmpeg-9.0.2.tar.xz https://ffmpeg.org/releases/ffmpeg-9.0.2.tar.xz 8c3850283eb25fa026482078a04051e0be17347b09ef81a0849bec15a96e002e
cmake -S "$cache/SDL3-3.4.16" -B "$cache/sdl-$target-build" -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$prefix" -DSDL_SHARED=ON -DSDL_STATIC=OFF -DSDL_TESTS=OFF
cmake --build "$cache/sdl-$target-build" --parallel 8
if [[ "$target" == linux ]]; then
  config="$cache/sdl-$target-build/include-config-release/build_config/SDL_build_config.h"
  grep -q '^#define SDL_VIDEO_DRIVER_X11 1' "$config" || { echo 'SDL X11 development dependencies missing.' >&2; exit 1; }
  grep -q '^#define SDL_VIDEO_DRIVER_WAYLAND 1' "$config" || { echo 'SDL Wayland/EGL development dependencies missing.' >&2; exit 1; }
fi
cmake --install "$cache/sdl-$target-build"
mkdir -p "$cache/ffmpeg-$target-build"
cd "$cache/ffmpeg-$target-build"
extra=()
if [[ "$target" == windows ]]; then extra+=(--toolchain=msvc --target-os=win64); fi
"$cache/ffmpeg-9.0.2/configure" --prefix="$prefix" --enable-shared --disable-static --disable-autodetect --disable-programs --disable-doc --disable-debug --disable-network --disable-gpl --disable-nonfree --disable-everything --enable-avcodec --enable-avformat --enable-avutil --enable-swresample --enable-protocol=file --enable-demuxer=mp3,mov,ogg,flac,wav --enable-decoder=mp3,mp3float,aac,aac_fixed,vorbis,opus,flac,pcm_s16le,pcm_s24le,pcm_s32le,pcm_f32le,pcm_u8,adpcm_ms,adpcm_ima_wav --enable-parser=mpegaudio,aac,vorbis,opus,flac "${extra[@]}"
make -j8
make install
mkdir -p "$prefix/share/libertyvr-dependencies"
cp ffbuild/config.mak config.h "$prefix/share/libertyvr-dependencies/"
cp "$cache/ffmpeg-9.0.2/COPYING.LGPLv2.1" "$prefix/share/libertyvr-dependencies/FFMPEG_LICENSE.txt"
cp "$cache/SDL3-3.4.16/LICENSE.txt" "$prefix/share/libertyvr-dependencies/SDL_LICENSE.txt"
cp "$cache/SDL3-3.4.16.tar.gz" "$cache/ffmpeg-9.0.2.tar.xz" "$prefix/share/libertyvr-dependencies/"
