#pragma once
#include "fltk-trayicon_global.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>
class Fl_RGB_Image;
// All calls belong on the FLTK main thread. No native types in the public API.
class FLTK_TRAYICON_EXPORT Fltk_TrayIcon
{
public:
    struct Options {
        std::string id = "fltk-trayicon";
        std::string title = "FLTK application";
        bool enabled = true;             // false: no native resources (previews/tests)
        bool automatic_poll = true;      // false: call poll() yourself
    };
    struct MenuItem {
        std::string label;               // empty label: separator
        int id = 0;
        bool enabled = true;
        bool checkable = false;
        bool checked = false;
    };
    enum class Event { Activate, DoubleClick, Menu };
    using Callback = std::function<void(Event, int)>; // id is meaningful for Menu
    Fltk_TrayIcon();
    explicit Fltk_TrayIcon(const Options& options);
    ~Fltk_TrayIcon();
    Fltk_TrayIcon(const Fltk_TrayIcon&) = delete;
    Fltk_TrayIcon& operator=(const Fltk_TrayIcon&) = delete;
    bool icon_svg(const std::string& svg); // invalid input retains the previous icon
    bool icon(const Fl_RGB_Image& image); // RGB/RGBA pixels are copied
    void tooltip(const std::string& text);
    void menu(const std::vector<MenuItem>& items, int default_action = 0);
    void callback(Callback action);
    void show();
    void hide();
    bool available() const;              // a usable visible tray, not merely a loaded DLL
    void poll();
private:
    struct Data;
    std::unique_ptr<Data> data_;
    static void timer(void* self);
};
