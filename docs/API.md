# fltk-trayicon API and examples

[README](../README.md) · [Source](https://github.com/hdijkema/fltk-trayicon)

A C++17 static library for native notification-area icons and menus in FLTK
applications. The public API exposes no platform types. Every operation,
including construction and destruction, belongs on the FLTK main thread.

## Headers and linking

The complete API is declared in [fltk_trayicon.h](../fltk_trayicon.h).
`Fltk_TrayIcon` is in the global namespace.

```cmake
cmake_minimum_required(VERSION 3.21)
project(tray-example LANGUAGES CXX)
find_package(fltk-trayicon 0.1 CONFIG REQUIRED)
add_executable(example main.cpp)
target_compile_features(example PRIVATE cxx_std_17)
target_link_libraries(example PRIVATE fltk-trayicon::fltk-trayicon)
```

The target supplies FLTK images and the platform libraries. An application can
also use `add_subdirectory` and the same target. Use one compatible FLTK build
for the application and library.

## Options

`Fltk_TrayIcon::Options` has these public fields:

| Field | Default | Meaning |
| --- | --- | --- |
| `std::string id` | `"fltk-trayicon"` | Application/indicator identity. |
| `std::string title` | `"FLTK application"` | Native indicator title. |
| `bool enabled` | `true` | Set false to create no native resources, load no GTK modules and emit no actions; useful for previews and tests. |
| `bool automatic_poll` | `true` | Use an FLTK timer to dispatch events. Set false when your own main-thread loop calls `poll()`. |

Options are supplied at construction. Keep the FLTK event loop running for
automatic event dispatch.

## MenuItem, Event and Callback

`Fltk_TrayIcon::MenuItem` is an application-supplied value:

| Field | Default | Meaning |
| --- | --- | --- |
| `std::string label` | Empty | UTF-8 label. An empty label creates a separator. |
| `int id` | `0` | Application action ID; any integer is allowed. |
| `bool enabled` | `true` | Whether the entry can be activated. |
| `bool checkable` | `false` | Whether it displays a check mark. |
| `bool checked` | `false` | Current check state. |

`enum class Event { Activate, DoubleClick, Menu };` distinguishes activation,
double-click and menu actions. `Callback` is
`std::function<void(Event, int)>`. The integer is meaningful for `Event::Menu`.
Callbacks run on the FLTK main thread when events are dispatched.

Check marks do not toggle automatically. Update your own model and call `menu()`
again with the new state. Gesture availability depends on the platform and panel.

## Fltk_TrayIcon

| Public declaration | Preconditions and result |
| --- | --- |
| `Fltk_TrayIcon()` | Construct with default `Options`. |
| `explicit Fltk_TrayIcon(const Options& options)` | Construct the platform backend, or an inert backend when disabled. Start automatic polling if enabled by the options. |
| `~Fltk_TrayIcon()` | Remove native resources and cancel the FLTK timer. The class is not copyable; copy construction and copy assignment are deleted. |
| `bool icon_svg(const std::string& svg)` | Supply a complete SVG document, not a filename. FLTK renders it; invalid input returns false and retains the previous icon. |
| `bool icon(const Fl_RGB_Image& image)` | Copy valid RGB/RGBA pixels. The caller need not retain the source image afterwards. Invalid data returns false and retains the previous icon. |
| `void tooltip(const std::string& text)` | Set UTF-8 tooltip text. Native platform display limits apply. |
| `void menu(const std::vector<MenuItem>& items, int default_action = 0)` | Replace the menu and its default action ID. Items are supplied by value through the backend; no caller-owned vector needs to stay alive. |
| `void callback(Callback action)` | Replace the event callback. Keep captured application objects alive while events can arrive; an empty callback disables application event handling. |
| `void show()` | Request native visibility, preserving the icon and menu. |
| `void hide()` | Remove the visible icon, preserving its configured icon and menu. |
| `bool available() const` | Report a usable visible tray, not just a loaded platform library. Check after `show()` and retain a normal window/exit path if unavailable. |
| `void poll()` | Collect and dispatch queued events on the FLTK main thread. Call this from your existing loop when automatic polling is disabled. |

A callback may destroy the tray. Subsequent events from that polling batch are
then discarded. Do not access the destroyed instance from your callback.
`fltk-trayicon_global.h` supplies the export macro; it has no runtime API.

## Complete example: show settings or quit

Save this as `main.cpp` and use the CMake file above. The tray can keep the
process alive after the settings window is hidden. When no tray is available,
closing the window exits instead.

```cpp
#include <FL/Fl.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Double_Window.H>
#include <fltk_trayicon.h>

int main()
{
    bool quitting = false;
    Fl_Double_Window window(420, 180, "Tray example");
    new Fl_Box(20, 40, 380, 80, "Use the tray menu to reopen this window or quit.");
    window.end();

    Fltk_TrayIcon tray;
    if (!tray.icon_svg(R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 32 32">
        <rect x="4" y="4" width="24" height="24" rx="7" fill="#ad4a38"/>
        <circle cx="16" cy="16" r="5" fill="white"/>
    </svg>)")) return 1;
    tray.tooltip("Tray example");
    tray.menu({ {"Open settings", 10}, {}, {"Quit", 20} }, 10);
    tray.callback([&](Fltk_TrayIcon::Event event, int id) {
        if (event == Fltk_TrayIcon::Event::Activate ||
            event == Fltk_TrayIcon::Event::DoubleClick) window.show();
        if (event == Fltk_TrayIcon::Event::Menu && id == 10) window.show();
        if (event == Fltk_TrayIcon::Event::Menu && id == 20) quitting = true;
    });
    tray.show();
    window.show();
    while (!quitting && (window.shown() || tray.available())) Fl::wait();
    window.hide();
    return 0;
}
```

The tray is destroyed before the window. The callback's captures remain alive
until event dispatch stops. The example does not access profiles, settings files
or databases.

## Manual polling and isolated use

For an application that already polls on the main thread:

```cpp
Fltk_TrayIcon::Options options;
options.automatic_poll = false;
Fltk_TrayIcon tray(options);
// Configure and show the tray, then call tray.poll() from your event loop.
```

For previews/tests, set `options.enabled = false` before constructing the tray.
It then creates no native tray resources and emits no user actions. This is
distinct from `hide()`, which preserves a configured native backend.

## Platform behavior and limits

- Windows uses `Shell_NotifyIconW`, native menus and Explorer restart recovery.
- Linux loads GTK3/Ayatana AppIndicator at runtime without a GTK link-time
  dependency. A missing runtime, display or indicator host leaves the tray
  unavailable. Many panels use the primary click for a menu and do not provide
  double-click events; the default action is the secondary activation target.
- macOS uses AppKit `NSStatusItem` and `NSMenu` through Objective-C++.

Windows builds and native resource cleanup are tested locally. Linux/macOS
desktop integration still requires verification on those systems. On Linux,
rendered icon files stay in a private temporary directory until destruction so
the panel can read them asynchronously. Only owned files are removed; abnormal
termination may leave that directory behind.

## Build and installation

Library build options are `FLTK_TRAYICON_FETCH_FLTK`,
`FLTK_TRAYICON_BUILD_EXAMPLE` and `FLTK_TRAYICON_BUILD_TESTS`. Examples/tests
default to ON only for standalone builds. An installed FLTK 1.4.5 or newer
package is used when available; otherwise the named FLTK 1.4.5 release is fetched.

See the [README](../README.md) for build/install commands. To build this page's
example, save its C++ program and CMake file in an empty directory, then use:

```sh
cmake -S . -B build -DCMAKE_PREFIX_PATH="C:/sdk/widgets;C:/sdk/fltk"
cmake --build build --config Release
```

Replace the paths with your installations. On Windows, use a Visual Studio
developer terminal and matching architecture, runtime and build configuration.
This reference covers all declared public types, fields and operations; backend
implementation details are private. Keep it aligned with the public header.
