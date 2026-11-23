#include "fltk_trayicon.h"
#include <FL/Fl_RGB_Image.H>
#include <iostream>
#include <type_traits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#endif
int main() {
    static_assert(!std::is_copy_constructible<Fltk_TrayIcon>::value, "native resources must have one owner");
    Fltk_TrayIcon::Options options; options.enabled = false;
    Fltk_TrayIcon tray(options);
    int calls = 0;
    tray.callback([&](Fltk_TrayIcon::Event, int) { ++calls; });
    tray.menu({{"Disabled", -7, false}, {}, {"Checked", 42, true, true, true}}, -7);
    tray.tooltip("UTF-8: caf\xc3\xa9");
    if (tray.icon_svg("") || tray.icon_svg("not SVG")) return 1;
    Fl_RGB_Image empty(nullptr, 0, 0);
    if (tray.icon(empty)) return 1;
    tray.show(); tray.poll(); tray.hide(); tray.poll();
    if (tray.available() || calls != 0) return 1;
#ifdef _WIN32
    DWORD gdi_before = 0;
    for (int i = 0; i < 128; ++i) {
        // Compare after warming up process-wide FLTK/Windows caches.
        if (i == 64) gdi_before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        Fltk_TrayIcon::Options native; native.automatic_poll = false;
        Fltk_TrayIcon hidden(native);
        // RGB(A) with padding and transparent/partially transparent pixels.
        const unsigned char pixels[] = {255,0,0,255, 0,255,0,128, 0,0,0,0,
                                        0,0,255,0, 255,255,255,255, 0,0,0,0};
        Fl_RGB_Image padded(pixels, 2, 2, 4, 12);
        if (!hidden.icon(padded)) return 1;
        if (!hidden.icon_svg("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"32\" height=\"32\"><circle cx=\"16\" cy=\"16\" r=\"14\" fill=\"#55d6a0\"/></svg>")) return 1;
        hidden.poll();
        if (hidden.available()) return 1; // no show(), no visible tray in tests
    }
    if (GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) != gdi_before) {
        std::cerr << "Native icon handles leaked: " << gdi_before << " -> " << GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) << "\n"; return 1;
    }
#endif
    std::cout << "PASS: disabled backend never creates a tray or dispatches actions; invalid icons rejected.\n";
}
