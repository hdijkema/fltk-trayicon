#pragma once
#include "../fltk_trayicon.h"
#include <FL/Fl_RGB_Image.H>
#include <utility>
namespace fltk_trayicon_detail {
using Events = std::vector<std::pair<Fltk_TrayIcon::Event, int>>;
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool icon(const Fl_RGB_Image&) { return false; }
    virtual void tooltip(const std::string&) {}
    virtual void menu(const std::vector<Fltk_TrayIcon::MenuItem>&, int) {}
    virtual void show(bool) {}
    virtual bool available() const { return false; }
    virtual Events poll() { return {}; }
};
std::unique_ptr<Backend> make_backend(const Fltk_TrayIcon::Options& options);
}
