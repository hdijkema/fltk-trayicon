#include "fltk_trayicon.h"
#include "src/backend.h"
#include <FL/Fl.H>
#include <FL/Fl_SVG_Image.H>
struct Fltk_TrayIcon::Data {
    std::unique_ptr<fltk_trayicon_detail::Backend> backend;
    Callback callback;
    std::shared_ptr<bool> alive = std::make_shared<bool>(true);
};
Fltk_TrayIcon::Fltk_TrayIcon() : Fltk_TrayIcon(Options{}) {}
Fltk_TrayIcon::Fltk_TrayIcon(const Options& options) : data_(std::make_unique<Data>())
{
    data_->backend = options.enabled ? fltk_trayicon_detail::make_backend(options)
        : std::make_unique<fltk_trayicon_detail::Backend>();
    if (options.enabled && options.automatic_poll) Fl::add_timeout(0.05, timer, this);
}
Fltk_TrayIcon::~Fltk_TrayIcon() { *data_->alive = false; Fl::remove_timeout(timer, this); }
bool Fltk_TrayIcon::icon_svg(const std::string& svg)
{
    if (svg.empty()) return false;
    Fl_SVG_Image image(nullptr, svg.c_str());
    if (image.fail() || image.w() <= 0 || image.h() <= 0) return false;
    image.resize(64, 64); image.normalize();
    return icon(image);
}
bool Fltk_TrayIcon::icon(const Fl_RGB_Image& image)
{
    if (image.fail() || !image.array || image.data_w() <= 0 || image.data_h() <= 0 ||
        (image.d() != 3 && image.d() != 4)) return false;
    return data_->backend->icon(image);
}
void Fltk_TrayIcon::tooltip(const std::string& text) { data_->backend->tooltip(text); }
void Fltk_TrayIcon::menu(const std::vector<MenuItem>& items, int default_action) { data_->backend->menu(items, default_action); }
void Fltk_TrayIcon::callback(Callback action) { data_->callback = std::move(action); }
void Fltk_TrayIcon::show() { data_->backend->show(true); }
void Fltk_TrayIcon::hide() { data_->backend->show(false); }
bool Fltk_TrayIcon::available() const { return data_->backend->available(); }
void Fltk_TrayIcon::poll()
{
    const auto events = data_->backend->poll();
    const auto action = data_->callback;
    const auto alive = data_->alive;
    // A callback may destroy this tray: use only local values from here on.
    if (action) for (const auto& event : events) {
        action(event.first, event.second);
        if (!*alive) break;
    }
}
void Fltk_TrayIcon::timer(void* self)
{
    auto* tray = static_cast<Fltk_TrayIcon*>(self);
    Fl::repeat_timeout(0.05, timer, self); // destructor cancels this, also in callbacks
    tray->poll();
}
