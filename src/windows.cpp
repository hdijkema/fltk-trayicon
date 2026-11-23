#include "backend.h"
#include <windows.h>
#include <shellapi.h>
#include <chrono>
#include <stdexcept>

namespace fltk_trayicon_detail {
namespace {
std::wstring wide(const std::string& text)
{
    if (text.empty()) return {};
    const int size = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (!size) return {};
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), static_cast<int>(text.size()), result.data(), size);
    return result;
}
HICON native_icon(const Fl_RGB_Image& image)
{
    const int width = image.data_w(), height = image.data_h();
    BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
    info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
    void* memory = nullptr;
    HBITMAP bitmap = CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &memory, nullptr, 0);
    if (!bitmap) return nullptr;
    auto* pixels = static_cast<DWORD*>(memory);
    const int stride = image.ld() ? image.ld() : width * image.d();
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        const auto* rgba = image.array + y * stride + x * image.d();
        const DWORD alpha = image.d() == 4 ? rgba[3] : 255;
        pixels[y * width + x] = (alpha << 24) | ((rgba[0] * alpha / 255) << 16) |
            ((rgba[1] * alpha / 255) << 8) | (rgba[2] * alpha / 255);
    }
    const std::vector<unsigned char> mask_bits(((width + 15) / 16) * 2 * height, 0);
    HBITMAP mask = CreateBitmap(width, height, 1, 1, mask_bits.data());
    ICONINFO icon{}; icon.fIcon = TRUE; icon.hbmColor = bitmap; icon.hbmMask = mask;
    HICON result = mask ? CreateIconIndirect(&icon) : nullptr;
    if (mask) DeleteObject(mask);
    DeleteObject(bitmap);
    return result;
}
class WindowsBackend : public Backend {
    HWND window_ = nullptr;
    HICON icon_ = nullptr;
    NOTIFYICONDATAW tray_{};
    std::vector<Fltk_TrayIcon::MenuItem> menu_;
    Events events_;
    bool requested_ = false, shown_ = false, popup_ = false, suppress_release_ = false;
    std::chrono::steady_clock::time_point click_{};
    UINT taskbar_created_ = RegisterWindowMessageW(L"TaskbarCreated");
    static constexpr UINT tray_message = WM_APP + 71;
    static LRESULT CALLBACK procedure(HWND window, UINT message, WPARAM w, LPARAM l)
    {
        auto* self = reinterpret_cast<WindowsBackend*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        if (message == WM_NCCREATE) {
            self = static_cast<WindowsBackend*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
        }
        if (self) {
            if (message == self->taskbar_created_) {
                self->shown_ = false;
                self->update();
            }
            if (message == tray_message) {
                if (l == WM_LBUTTONUP) {
                    if (self->suppress_release_) { self->suppress_release_ = false; self->click_ = {}; }
                    else self->click_ = std::chrono::steady_clock::now() + std::chrono::milliseconds(GetDoubleClickTime());
                }
                if (l == WM_LBUTTONDBLCLK) {
                    self->click_ = {}; self->suppress_release_ = true;
                    self->events_.push_back({Fltk_TrayIcon::Event::DoubleClick, 0});
                }
                if (l == WM_RBUTTONUP || l == WM_CONTEXTMENU) self->popup_ = true;
                if (l == NIN_KEYSELECT) self->events_.push_back({Fltk_TrayIcon::Event::DoubleClick, 0});
            }
        }
        return DefWindowProcW(window, message, w, l);
    }
    void update()
    {
        if (!window_ || !requested_ || !icon_) return;
        tray_.hIcon = icon_;
        shown_ = Shell_NotifyIconW(shown_ ? NIM_MODIFY : NIM_ADD, &tray_) != FALSE;
    }
public:
    explicit WindowsBackend(const Fltk_TrayIcon::Options&)
    {
        WNDCLASSW cls{}; cls.lpfnWndProc = procedure; cls.hInstance = GetModuleHandleW(nullptr);
        cls.lpszClassName = L"FLTK.TrayIcon.1";
        RegisterClassW(&cls);
        window_ = CreateWindowExW(0, cls.lpszClassName, L"FLTK tray", 0, 0, 0, 0, 0, nullptr, nullptr, cls.hInstance, this);
        tray_.cbSize = sizeof(tray_); tray_.hWnd = window_; tray_.uID = 1;
        tray_.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP; tray_.uCallbackMessage = tray_message;
    }
    ~WindowsBackend() override
    {
        show(false);
        if (icon_) DestroyIcon(icon_);
        if (window_) DestroyWindow(window_);
    }
    bool icon(const Fl_RGB_Image& image) override
    {
        HICON replacement = native_icon(image);
        if (!replacement) return false;
        const auto old = icon_; icon_ = replacement; update();
        if (old) DestroyIcon(old);
        return true;
    }
    void tooltip(const std::string& text) override
    {
        const auto tip = wide(text); wcsncpy_s(tray_.szTip, tip.c_str(), _TRUNCATE); update();
    }
    void menu(const std::vector<Fltk_TrayIcon::MenuItem>& items, int) override { menu_ = items; }
    void show(bool visible) override
    {
        requested_ = visible;
        if (!visible) {
            if (shown_) Shell_NotifyIconW(NIM_DELETE, &tray_);
            shown_ = false; click_ = {}; popup_ = false; events_.clear();
        } else update();
    }
    bool available() const override { return shown_; }
    Events poll() override
    {
        MSG message;
        if (window_) while (PeekMessageW(&message, window_, 0, 0, PM_REMOVE)) DispatchMessageW(&message);
        if (click_ != std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now() >= click_) {
            click_ = {}; events_.push_back({Fltk_TrayIcon::Event::Activate, 0});
        }
        if (popup_) {
            popup_ = false;
            HMENU popup = CreatePopupMenu();
            for (std::size_t i = 0; i < menu_.size(); ++i) {
                const auto& item = menu_[i];
                if (item.label.empty()) AppendMenuW(popup, MF_SEPARATOR, 0, nullptr);
                else AppendMenuW(popup, MF_STRING | (item.checkable && item.checked ? MF_CHECKED : 0) |
                    (item.enabled ? 0 : MF_GRAYED), 100 + i, wide(item.label).c_str());
            }
            POINT pointer; GetCursorPos(&pointer); SetForegroundWindow(window_);
            const UINT selected = TrackPopupMenuEx(popup, TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
                pointer.x, pointer.y, window_, nullptr);
            PostMessageW(window_, WM_NULL, 0, 0); DestroyMenu(popup);
            if (selected >= 100 && selected - 100 < menu_.size()) events_.push_back({Fltk_TrayIcon::Event::Menu, menu_[selected - 100].id});
        }
        Events result; result.swap(events_); return result;
    }
};
}
std::unique_ptr<Backend> make_backend(const Fltk_TrayIcon::Options& options) { return std::make_unique<WindowsBackend>(options); }
}
