# Build `libsndfile` for the Web with Emscripten

This builds `libsndfile` as a static WebAssembly library on macOS using
`emsdk`.

## Requirements

- macOS
- `git`
- `cmake`
- `emsdk`

## 1) Install and activate Emscripten

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

## 2) Clone `libsndfile`

```bash
git clone https://github.com/libsndfile/libsndfile.git
cd libsndfile
```

## 3) Configure for WebAssembly

Use a fresh build directory and disable tests, examples, and shared libraries:

```bash
rm -rf build-web

emcmake cmake -S . -B build-web \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_PROGRAMS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF \
  -DENABLE_PACKAGE_CONFIG=OFF \
  -DINSTALL_PKGCONFIG_MODULE=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release
```

## 4) Build

```bash
cmake --build build-web -j
```

## Notes

- This produces a static library for use in your own WebAssembly app.
- `emcmake` is only needed for the configure step, not the build step.
- MP3 support is usually not included in this web build.
- For browser use, WAV/AIFF/FLAC workflows are the easiest.

## Example full flow

```bash
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

cd ..
git clone https://github.com/libsndfile/libsndfile.git
cd libsndfile

rm -rf build-web
emcmake cmake -S . -B build-web \
  -DBUILD_SHARED_LIBS=OFF \
  -DBUILD_PROGRAMS=OFF \
  -DBUILD_EXAMPLES=OFF \
  -DBUILD_TESTING=OFF \
  -DENABLE_PACKAGE_CONFIG=OFF \
  -DINSTALL_PKGCONFIG_MODULE=OFF \
  -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build-web -j
```

If you want, I can also turn this into a shorter “one-liner” README section
or add a minimal example of linking `libsndfile` into your wasm app.
