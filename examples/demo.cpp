#include "fltk_trayicon.h"
#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <string>

struct Demo {
    Fl_Window window{460, 220, "FLTK tray icon"};
    Fl_Box description{20, 20, 420, 70, "Right-click the tray icon for its menu.\nClick to change colour; double-click to show this window."};
    Fl_Button next{30, 120, 190, 38, "Change colour"};
    Fl_Button quit{240, 120, 190, 38, "Quit"};
    Fltk_TrayIcon tray;
    bool blue = false;
    Demo() {
        window.end();
        next.callback([](Fl_Widget*, void* self) { static_cast<Demo*>(self)->change(); }, this);
        quit.callback([](Fl_Widget*, void* self) { static_cast<Demo*>(self)->stop(); }, this);
        window.callback([](Fl_Widget*, void* self) {
            auto* demo = static_cast<Demo*>(self);
            if (demo->tray.available()) demo->window.hide(); else demo->stop();
        }, this);
        tray.callback([this](Fltk_TrayIcon::Event event, int id) {
            if (event == Fltk_TrayIcon::Event::Activate || (event == Fltk_TrayIcon::Event::Menu && id == 2)) change();
            else if (event == Fltk_TrayIcon::Event::DoubleClick || id == 1) window.show();
            else if (id == 3) stop();
        });
        update(); tray.show();
    }
    void change() { blue = !blue; update(); }
    void update() {
        const std::string svg = "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"64\" height=\"64\"><rect x=\"4\" y=\"4\" width=\"56\" height=\"56\" rx=\"14\" fill=\"" +
            std::string(blue ? "#42a5f5" : "#55d6a0") + "\"/><path d=\"M23 18v28h7V35h13v-6H30v-5h16v-6z\" fill=\"#17252b\"/></svg>";
        tray.icon_svg(svg); tray.tooltip("FLTK tray example");
        tray.menu({{"Show window", 1}, {"Blue", 2, true, true, blue}, {}, {"Quit", 3}}, 1);
    }
    void stop() { tray.hide(); window.hide(); }
};
int main() {
    Demo demo; demo.window.show();
    while (demo.window.shown() || demo.tray.available()) Fl::wait();
}
