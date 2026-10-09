# weatherstation_gui

A Qt Quick app that shows the latest 360° panorama of the Panomax webcam 9015 (Patscherkofel
summit near Innsbruck, Austria) and keeps it up to date. It checks for a newer image every
poll interval and turns the panorama slowly by itself. Drag or flick to pan, pinch or scroll to
zoom; after a few seconds without input it zooms back to fit and turns again.

It is developed on a Linux desktop and meant for an embedded 1280x800 touch screen, fullscreen.
It uses only Qt Core, Gui, Network, Qml and Quick.

## Prerequisites

- Qt 6.12 in `~/Qt/6.12.0/gcc_64` (the `debug` preset points there)
- CMake 3.28 or newer and a C++20 compiler
- On Fedora: `libglvnd-devel` (Qt's CMake config needs the OpenGL headers)
- Network access on the first configure: CMake downloads GoogleTest

## Build, run, test

```sh
cmake --preset debug
cmake --build --preset debug -j$(nproc)
ctest --preset debug
build/debug/src/app/weatherstation_gui
```

`cmake --build --preset debug --target all_qmllint` lints the QML.

## Settings

Each setting comes from the command line, else from the INI file, else from the default. An invalid
value is logged and ignored, so the next lower layer's value applies. The INI file is
`~/.config/weatherstation/weatherstation_gui.ini` (under `$XDG_CONFIG_HOME` if that is set); the
app logs the path at startup and in `--help`.

| Option | INI key | Default | Meaning |
|---|---|---|---|
| `--camera <id>` | `camera` | `9015` | Panomax camera id |
| `--size <name>` | `size` | `auto` | size variant (`thumb`, `small`, `optimized`, `reduced`, `default`, `hd`, `full`), or `auto`: the smallest one at least 800 px high |
| `--autoscroll-speed <deg/s>` | `autoscroll-speed` | `3` | 0.1 to 360 degrees per second |
| `--autoscroll-direction <left\|right>` | `autoscroll-direction` | `right` | `right` turns the view right, so the image content moves left |
| `--idle-timeout <s>` | `idle-timeout` | `5` | seconds without input before autoscroll resumes, 1 to 3600 |
| `--poll-interval <s>` | `poll-interval` | `60` | seconds between checks for a newer image, 10 to 3600 |
| `--fullscreen`, `--windowed` | `fullscreen` (`true`/`false`) | windowed | windowed is 1280x800 |
| `--log-rules <rules>` | `log-rules` | empty | logging rules, see [Logging](#logging) |

Larger size variants need much more memory. Peak memory (RSS) measured on a desktop with camera 9015:
`default` about 150 MB, `hd` about 175 MB, `full` about 650 MB. Check the device's RAM before choosing `full`.

Example INI file:

```ini
size=hd
fullscreen=true
log-rules=ws.*.debug=false, ws.panomax.image.debug=true
```

In the INI file `;` starts a comment, so `log-rules=a;b` keeps only `a`. Separate rules with
commas there. On the command line, separate them with `;` as in `QT_LOGGING_RULES`.

## Logging

Each line shows the time, level and category, for example
`2026-10-09 17:35:50.327 info ws.panomax.image: loaded 2026-10-09 17:30:00: 3725x800 px, ...`

| Category | Logs |
|---|---|
| `ws.app` | startup, the first image on screen (from `Main.qml`) |
| `ws.config` | the INI path and each setting with its source |
| `ws.panomax.api` | each HTTP request and response: URL, status, bytes, duration |
| `ws.panomax.image` | each check (newest image or no change), size choice, load, next check |
| `ws.ui.panorama` | the view: texture strips, autoscroll stopped and resumed, view resets |

By default every `ws.*` category logs from debug up. That default is one line in
`src/app/logging.cpp`; for info and up, change it to:

```cpp
constexpr auto defaultRules = "ws.*.debug=false";
```

Filter rules use the `QT_LOGGING_RULES` syntax and apply in this order; the last matching rule wins:

1. the default above
2. `log-rules`, from the command line if given, else from the INI file. The command line replaces
   the INI value; the two are not merged.
3. the `QT_LOGGING_RULES` environment variable. Qt applies it last, so it beats both.

`QT_MESSAGE_PATTERN` replaces the line format, for example
`QT_MESSAGE_PATTERN='%{time hh:mm:ss.zzz} %{threadid} %{category}: %{message}'`.

## Zoom

Zoom goes from fit-height (the image fills the window height) to 1:1 (one image pixel per screen
pixel). With the default size (`auto` picks the 800 px high `default` variant of camera 9015) in an
800 px tall window, both limits are the same, so wheel and pinch do nothing. To zoom, choose a
taller size variant (for example `--size hd`, 1080 px) or make the window smaller.

## Project layout

- `src/app/`: the executable: `main.cpp`, `Main.qml`, settings (`config.*`) and logging setup (`logging.*`)
- `src/panomax/`: QML module `Panomax`: Panomax API data, HTTP client, `PanomaxImageSource`
- `src/ui/`: QML module `Ui`: `PanoramaView` and its math (`panoramageometry.*`)
- `tests/`: GoogleTest unit tests, with real API responses in `tests/fixtures/`
- `docs/`: [`panomax-api.md`](docs/panomax-api.md), how the Panomax API works: endpoints,
  image URLs, size variants, time zone, caching
