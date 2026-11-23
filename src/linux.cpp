#include "backend.h"
#include <FL/Fl_PNG_Image.H>
#include <dlfcn.h>
#include <filesystem>
#include <chrono>
#include <atomic>
#include <unistd.h>

namespace fltk_trayicon_detail {
namespace {
// GLib registers global types: keep the runtime modules loaded until exit.
struct GtkApi
{
    void* indicatorLib = nullptr;
    void* gtkLib = nullptr;
    void* objectLib = nullptr;
    void* glibLib = nullptr;
    void* (*indicatorNew)(const char*, const char*, int) = nullptr;
    void (*setStatus)(void*, int) = nullptr;
    void (*setMenu)(void*, void*) = nullptr;
    void (*setIcon)(void*, const char*, const char*) = nullptr;
    void (*setTitle)(void*, const char*) = nullptr;
    void (*setSecondary)(void*, void*) = nullptr;
    unsigned long (*indicatorType)() = nullptr;
    int (*gtkInit)(int*, char***) = nullptr;
    void* (*menuNew)() = nullptr;
    void* (*itemNew)(const char*) = nullptr;
    void* (*checkNew)(const char*) = nullptr;
    void* (*separatorNew)() = nullptr;
    void (*checkActive)(void*, int) = nullptr;
    void (*append)(void*, void*) = nullptr;
    void (*showAll)(void*) = nullptr;
    void (*sensitive)(void*, int) = nullptr;
    void (*destroy)(void*) = nullptr;
    void* (*refSink)(void*) = nullptr;
    void (*unref)(void*) = nullptr;
    unsigned long (*connect)(void*, const char*, void(*)(), void*, void(*)(void*, void*), unsigned) = nullptr;
    unsigned (*signalLookup)(const char*, unsigned long) = nullptr;
    unsigned (*disconnect)(void*, unsigned, unsigned, unsigned, void*, void*, void*) = nullptr;
    void (*objectGet)(void*, const char*, ...) = nullptr;
    int (*iterate)(void*, int) = nullptr;
    template<class T> bool symbol(void* module, T& target, const char* name) {
        target = module ? reinterpret_cast<T>(dlsym(module, name)) : nullptr; return target != nullptr;
    }
    bool load()
    {
        for (const char* name : {"libayatana-appindicator3.so.1", "libappindicator3.so.1"}) {
            indicatorLib = dlopen(name, RTLD_NOW | RTLD_LOCAL); if (indicatorLib) break;
        }
        if (!indicatorLib) return false;
        gtkLib = dlopen("libgtk-3.so.0", RTLD_NOW | RTLD_LOCAL);
        objectLib = dlopen("libgobject-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
        glibLib = dlopen("libglib-2.0.so.0", RTLD_NOW | RTLD_LOCAL);
        const bool available =
            symbol(indicatorLib, indicatorNew, "app_indicator_new") &&
            symbol(indicatorLib, setStatus, "app_indicator_set_status") &&
            symbol(indicatorLib, setMenu, "app_indicator_set_menu") &&
            symbol(indicatorLib, setIcon, "app_indicator_set_icon_full") &&
            symbol(indicatorLib, indicatorType, "app_indicator_get_type") &&
            symbol(gtkLib, gtkInit, "gtk_init_check") &&
            symbol(gtkLib, menuNew, "gtk_menu_new") &&
            symbol(gtkLib, itemNew, "gtk_menu_item_new_with_label") &&
            symbol(gtkLib, checkNew, "gtk_check_menu_item_new_with_label") &&
            symbol(gtkLib, separatorNew, "gtk_separator_menu_item_new") &&
            symbol(gtkLib, checkActive, "gtk_check_menu_item_set_active") &&
            symbol(gtkLib, append, "gtk_menu_shell_append") &&
            symbol(gtkLib, showAll, "gtk_widget_show_all") &&
            symbol(gtkLib, sensitive, "gtk_widget_set_sensitive") &&
            symbol(gtkLib, destroy, "gtk_widget_destroy") &&
            symbol(objectLib, refSink, "g_object_ref_sink") &&
            symbol(objectLib, unref, "g_object_unref") &&
            symbol(objectLib, connect, "g_signal_connect_data") &&
            symbol(objectLib, signalLookup, "g_signal_lookup") &&
            symbol(objectLib, disconnect, "g_signal_handlers_disconnect_matched") &&
            symbol(objectLib, objectGet, "g_object_get") &&
            symbol(glibLib, iterate, "g_main_context_iteration");
        symbol(indicatorLib, setTitle, "app_indicator_set_title");
        symbol(indicatorLib, setSecondary, "app_indicator_set_secondary_activate_target");
        return available && gtkInit(nullptr, nullptr);
    }
};
GtkApi& gtk() { static GtkApi api; return api; }

class LinuxBackend : public Backend {
    void* indicator_ = nullptr;
    void* menu_ = nullptr;
    bool connected_ = false, shown_ = false;
    Events events_;
    std::filesystem::path directory_;
    std::vector<std::filesystem::path> icons_;
    std::string tooltip_;
    struct Action { LinuxBackend* owner; int id; };
    static void activate(void*, void* value) {
        auto* action = static_cast<Action*>(value);
        action->owner->events_.push_back({Fltk_TrayIcon::Event::Menu, action->id});
    }
    static void free_action(void* value, void*) { delete static_cast<Action*>(value); }
    static void activated(void*, int, int, void* value) {
        static_cast<LinuxBackend*>(value)->events_.push_back({Fltk_TrayIcon::Event::Activate, 0});
    }
    static void connection(void*, int connected, void* value) {
        static_cast<LinuxBackend*>(value)->connected_ = connected != 0;
    }
public:
    explicit LinuxBackend(const Fltk_TrayIcon::Options& options)
    {
        static const bool loaded = gtk().load();
        if (!loaded) return;
        auto& api = gtk();
        indicator_ = api.indicatorNew(options.id.c_str(), "input-mouse", 3);
        if (!indicator_) return;
        if (api.setTitle) api.setTitle(indicator_, options.title.c_str());
        api.connect(indicator_, "connection-changed", reinterpret_cast<void(*)()>(connection), this, nullptr, 0);
        int connected = 0; api.objectGet(indicator_, "connected", &connected, nullptr); connected_ = connected != 0;
        if (api.signalLookup("activate", api.indicatorType()))
            api.connect(indicator_, "activate", reinterpret_cast<void(*)()>(activated), this, nullptr, 0);
        static std::atomic<unsigned> sequence{0};
        std::error_code ec;
        const auto temp = std::filesystem::temp_directory_path(ec);
        if (!ec) {
            const auto name = "fltk-trayicon-" + std::to_string(getpid()) + "-" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(sequence++);
            const auto candidate = temp / name;
            if (std::filesystem::create_directory(candidate, ec)) {
                directory_ = candidate;
                std::filesystem::permissions(directory_, std::filesystem::perms::owner_all,
                    std::filesystem::perm_options::replace, ec);
            }
        }
    }
    ~LinuxBackend() override
    {
        if (indicator_) {
            gtk().setStatus(indicator_, 0);
            gtk().disconnect(indicator_, 16, 0, 0, nullptr, nullptr, this);
            gtk().unref(indicator_);
            if (menu_) { gtk().destroy(menu_); gtk().unref(menu_); }
        }
        std::error_code ec;
        for (const auto& path : icons_) std::filesystem::remove(path, ec);
        if (!directory_.empty()) std::filesystem::remove(directory_, ec); // never recursively remove
    }
    bool icon(const Fl_RGB_Image& image) override
    {
        if (!indicator_ || directory_.empty()) return false;
        const auto path = directory_ / ("icon-" + std::to_string(icons_.size()) + ".png");
        // FLTK's PNG writer takes a mutable image; keep the caller's image const.
        Fl_RGB_Image png(image.array, image.data_w(), image.data_h(), image.d(), image.ld());
        if (fl_write_png(path.c_str(), &png) != 0) {
            std::error_code ec; std::filesystem::remove(path, ec); return false;
        }
        icons_.push_back(path); // retain while the host may still read previous images
        gtk().setIcon(indicator_, path.c_str(), tooltip_.c_str());
        return true;
    }
    void tooltip(const std::string& text) override
    {
        tooltip_ = text;
        if (indicator_ && !icons_.empty()) gtk().setIcon(indicator_, icons_.back().c_str(), tooltip_.c_str());
    }
    void menu(const std::vector<Fltk_TrayIcon::MenuItem>& items, int default_action) override
    {
        if (!indicator_) return;
        auto& api = gtk(); void* menu = api.menuNew(); api.refSink(menu);
        void* secondary = nullptr;
        for (const auto& item : items) {
            void* widget = item.label.empty() ? api.separatorNew() : item.checkable ? api.checkNew(item.label.c_str()) : api.itemNew(item.label.c_str());
            if (!item.label.empty()) {
                if (item.checkable) api.checkActive(widget, item.checked);
                api.sensitive(widget, item.enabled);
                api.connect(widget, "activate", reinterpret_cast<void(*)()>(activate), new Action{this, item.id}, free_action, 0);
                if (item.id == default_action) secondary = widget;
            }
            api.append(menu, widget);
        }
        api.showAll(menu); api.setMenu(indicator_, menu);
        if (api.setSecondary) api.setSecondary(indicator_, secondary);
        if (menu_) { api.destroy(menu_); api.unref(menu_); }
        menu_ = menu;
    }
    void show(bool visible) override {
        shown_ = visible;
        if (indicator_) gtk().setStatus(indicator_, visible ? 1 : 0);
        if (!visible) events_.clear();
    }
    bool available() const override { return indicator_ && connected_ && shown_; }
    Events poll() override {
        if (indicator_) for (int i = 0; i < 32 && gtk().iterate(nullptr, 0); ++i) {}
        Events result; result.swap(events_); return result;
    }
};
}
std::unique_ptr<Backend> make_backend(const Fltk_TrayIcon::Options& options) { return std::make_unique<LinuxBackend>(options); }
}
