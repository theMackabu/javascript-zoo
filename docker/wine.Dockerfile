# Builds Wine's jscript.dll and a minimal host for it.
# https://gitlab.winehq.org/wine/wine/-/tree/master/dlls/jscript
#
# SPDX-FileCopyrightText: 2026 Ivan Krasilnikov
# SPDX-License-Identifier: MIT

ARG BASE=jsz-debian
FROM $BASE

RUN apt-get update -y && \
    apt-get install -y --no-install-recommends \
      gcc-multilib \
      g++-multilib \
      libc6-dev-i386 \
      binutils-mingw-w64-i686 \
      gcc-mingw-w64-i686 \
      g++-mingw-w64-i686 \
      gcc-mingw-w64-x86-64 \
      g++-mingw-w64-x86-64 \
      libxkbcommon-dev

#ARG REPO=https://gitlab.winehq.org/wine/wine.git
ARG REPO=https://github.com/wine-mirror/wine.git
ARG REV=master

WORKDIR /src
RUN git clone --depth=1 --branch="$REV" "$REPO" . || \
    (git clone --depth=1 "$REPO" . && git fetch --depth=1 origin "$REV" && git checkout FETCH_HEAD)

ARG WINEARCH=win32

RUN mkdir -p build && cd build && \
    /src/configure \
      --without-x \
      --without-freetype \
      --disable-tests \
      $(if [ "$WINEARCH" = "win64" ]; then echo --enable-win64; fi)

RUN cd build && make -C dlls/jscript -j"$(nproc)"

# Runtime dependencies needed to test built binaries
RUN if [ "$WINEARCH" = win32 ]; then dpkg --add-architecture i386; fi && \
    apt-get update -y && \
    apt-get install -y --no-install-recommends wine $(echo "$WINEARCH" | sed 's/win/wine/')

ARG DIST_BINARY=/dist/wine
ENV DIST_DIR=$DIST_BINARY-dist
RUN mkdir -p "$DIST_DIR" && strip -o "$DIST_DIR/jscript.dll" build/dlls/jscript/*-windows/jscript.dll

COPY wine.cc jscript-host.cc
RUN if [ "$WINEARCH" = "win64" ]; then cxx=x86_64-w64-mingw32-g++; else cxx=i686-w64-mingw32-g++; fi; \
    $cxx -Os -s -municode -o "$DIST_DIR/jscript.exe" jscript-host.cc -lole32 -loleaut32 -luuid

COPY dist.py ./
RUN ./dist.py "$DIST_BINARY" \
      --dist_files="$DIST_DIR/jscript.dll" \
      --wrapper='export XDG_RUNTIME_DIR="${XDG_RUNTIME_DIR:-/tmp}" WINEDEBUG="${WINEDEBUG:--all,+err}"; exec wine "$SCRIPT_DIR/'"$(basename "$DIST_DIR")"'/jscript.exe" "$@"' \
      console_log=console.log
