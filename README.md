# Mascot Mouth for OBS

`Mascot Mouth` is a lightweight native OBS Studio source that switches between
closed-mouth and open-mouth PNG images using the level of an existing OBS audio
source. It does not open the microphone a second time and does not require a
captured helper window.

## Current MVP

- native C++17 OBS input source;
- audio source selection from the source properties;
- two transparent PNG images;
- separate open and close thresholds (hysteresis);
- configurable close delay;
- Russian and English UI;
- dependency-free tests for the mouth state machine.

The two PNG files should use the same canvas size and character position. If
their sizes differ, the OBS source reports the largest width and height and
draws both images from the top-left corner.

## Use in OBS

1. Add your microphone to OBS as an audio input source.
2. Add a new source and select **Mascot Mouth** / **Говорящий маскот**.
3. Select the microphone source.
4. Select the closed-mouth and open-mouth PNG files.
5. Start with `-35 dB` for opening, `-42 dB` for closing, and `120 ms` for the
   close delay.
6. Raise the open threshold toward `0 dB` if background noise opens the mouth.
   Lower it toward `-60 dB` if normal speech is not detected.

Audio filters placed on the selected OBS source are included in the level seen
by the plugin. A noise-suppression filter followed by a noise gate is useful in
noisy rooms.

## Build requirements

- OBS Studio development files, version 30 or newer;
- CMake 3.28 or newer;
- a C++17 compiler;
- Ninja for optional out-of-tree builds.

For a tested Windows build based on OBS Studio 32.1.2, see
[docs/BUILD_WINDOWS.md](docs/BUILD_WINDOWS.md).

### Build the plugin out of tree

Point `CMAKE_PREFIX_PATH` at an OBS build/install tree that exports
`OBS::libobs`:

```sh
cmake -S . -B build/plugin -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/path/to/obs/install
cmake --build build/plugin
cmake --install build/plugin --prefix build/package
```

On Windows, use the same commands from a Visual Studio Developer PowerShell and
replace `/path/to/obs/install` with the path containing `libobsConfig.cmake`.
The install step creates an OBS-compatible directory layout under
`build/package`.

### Build inside the OBS source tree

Place this directory under `plugins/obs-mascot-mouth`, add
`add_obs_plugin(obs-mascot-mouth PLATFORMS WINDOWS)` to
`plugins/CMakeLists.txt`, and configure OBS normally. Since `OBS::libobs`
already exists, the project will use it without calling `find_package`.

### Run the core tests without OBS

```sh
cmake -S . -B build/core -G Ninja \
  -DMASCOT_BUILD_PLUGIN=OFF \
  -DMASCOT_BUILD_TESTS=ON
cmake --build build/core
ctest --test-dir build/core --output-on-failure
```

The state-machine core can also be compiled directly:

```sh
c++ -std=c++17 -Isrc src/mouth-state.cpp tests/mouth-state-test.cpp -o mouth-tests
./mouth-tests
```

## Design notes

The OBS volume-meter callback only calculates the loudest channel and writes a
single atomic float. Hysteresis and the close timer run during `video_tick`.
Textures are loaded once and rendered directly by OBS. No audio buffers are
copied and no per-frame heap allocation is performed.

## License

GPL-2.0-or-later. See [LICENSE](LICENSE).
