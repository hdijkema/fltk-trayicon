# fltk-trayicon

<img src="docs/assets/logo.svg" width="80" height="80" alt="fltk-trayicon website logo">

[Homepage](https://hdijkema.github.io/fltk-trayicon/) | [API reference and examples](docs/API.md)

A small C++17 static library for system tray icons in FLTK 1.4 applications.
Uses the native platform event loop without an extra tray DLL.

- Windows: native `Shell_NotifyIconW`, menus, UTF-8 tooltips, Explorer restart recovery.
- Linux: optional GTK3/Ayatana AppIndicator, loaded at runtime with `dlopen`.
- macOS: native AppKit `NSStatusItem` and `NSMenu` (Objective-C++).
- SVG or RGB/RGBA icons, disabled/checked menu entries, separators, integer action IDs.
- Click, double-click and menu callbacks on the FLTK main thread.

Windows builds and native resource cleanup are tested locally. Linux and macOS
builds are configured in GitHub Actions; their desktop integration still needs
verification on those systems. On Linux, gesture support depends on the panel:
many AppIndicator hosts open the menu for a primary click and don't provide a
double-click event. The default menu action is the secondary activation target.

## Build

```sh
cmake -S . -B build/dev -DCMAKE_BUILD_TYPE=Release
cmake --build build/dev --config Release
ctest --test-dir build/dev -C Release --output-on-failure
cmake --install build/dev --config Release --prefix build/install
```

CMake uses an installed FLTK 1.4.5 or newer package, or fetches FLTK 1.4.5 at its stable release tag.
Set `FLTK_DIR` to choose your package. Its architecture, build configuration and
runtime must match your application. `FLTK_TRAYICON_FETCH_FLTK=OFF` disables fetching.
The optional `fltk-trayicon-demo` shows a coloured icon, checkable menu and callbacks.
Examples and tests default to on only for standalone builds; disable them with
`FLTK_TRAYICON_BUILD_EXAMPLE=OFF` and `FLTK_TRAYICON_BUILD_TESTS=OFF`.

Linux build dependencies are FLTK's usual X11/image libraries plus `libdl`.
Install `libayatana-appindicator3-1` (or compatible `libappindicator3-1`) for tray
support at runtime. No GTK development headers or GTK link dependency are needed.
An absent library/display/panel leaves `available()` false. Keep a normal way to
close the application when a tray isn't available.

## Use

```cmake
find_package(fltk-trayicon 0.1 CONFIG REQUIRED)
target_link_libraries(my-app PRIVATE fltk-trayicon::fltk-trayicon)
```

It also works with `add_subdirectory` and CMake `FetchContent`. An existing
`fltk::images` target is reused so the application and tray share one FLTK build.

```cpp
#include <fltk_trayicon.h>

Fltk_TrayIcon tray;
tray.icon_svg(svg_text); // complete SVG document; copied and rendered by FLTK
tray.tooltip("My application");
tray.menu({{"Open settings", 10}, {"Enabled", 20, true, true, true},
           {}, {"Quit", 30}}, 10);
tray.callback([&](Fltk_TrayIcon::Event event, int id) {
    if (event == Fltk_TrayIcon::Event::Menu && id == 10) window.show();
    if (event == Fltk_TrayIcon::Event::Menu && id == 30) quitting = true;
});
tray.show();
while (!quitting) Fl::wait(); // a tray can outlive the last visible window
```

An empty menu label is a separator. IDs may be any integer; only menu events
carry an action ID. Check marks don't mutate automatically: update your model
and call `menu()` again. Invalid icon data returns false and retains the old
icon. `show()` and `hide()` preserve the icon/menu; destruction removes native
resources and cancels its FLTK timer. Call every API on the FLTK main thread.
Callbacks may delete the tray; later queued events are then discarded.

For an existing application polling loop, set `Options::automatic_poll=false`
and call `poll()` there. `Options::enabled=false` creates no native resources,
loads no GTK modules, and emits no actions, useful for previews and tests.

Linux icon PNGs live in a private temporary directory until destruction so the
panel can read previous images asynchronously. Only owned files are removed;
there is no recursive cleanup. An abnormal process exit may leave that temporary
directory behind.

## License

LGPL-2.1; see [LICENSE](LICENSE). FLTK has its own LGPL license with the FLTK
linking exception. GTK/AppIndicator and AppKit are system runtime components.
Platform design follows the author's `racket-tray` and `razer-mouse-lite` work.
API references: [FLTK timers](https://www.fltk.org/doc-1.4/classFl.html) and
[AppKit status items](https://developer.apple.com/documentation/appkit/nsstatusitem).

## Release versions

The current library version is **0.1.0**, identified by tag `v0.1.0`.
Dependencies use named, tested releases: FLTK **1.4.5** (`release-1.4.5`),
not a development branch or a moving `latest` reference. Local source/package
paths remain available for development. Upgrade a dependency deliberately,
run the tests and publish a new library release when appropriate.

Push the matching `v<project-version>` tag after committing the release sources.
The workflow tests the platforms first and then creates a GitHub source release.
For the first release, publish fltk-css before fltk-cell, which depends on its
`v0.1.0` release. pgadmin-jr consumes those tags after publication. Release tags
must remain immutable; do not move a published tag to different sources.
