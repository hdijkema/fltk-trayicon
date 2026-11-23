#include "backend.h"
#import <AppKit/AppKit.h>
#include <chrono>
#include <cstring>

@interface FLTKTrayTarget : NSObject
@property(nonatomic, assign) void* owner;
- (void)click:(id)sender;
- (void)select:(id)sender;
@end

namespace fltk_trayicon_detail {
class MacBackend : public Backend {
    NSStatusItem* __strong item_ = nil;
    NSMenu* __strong menu_ = nil;
    FLTKTrayTarget* __strong target_ = nil;
    Events events_;
    bool shown_ = false;
    std::chrono::steady_clock::time_point click_{};
    static NSString* text(const std::string& value) {
        return [[NSString alloc] initWithBytes:value.data() length:value.size() encoding:NSUTF8StringEncoding] ?: @"";
    }
public:
    explicit MacBackend(const Fltk_TrayIcon::Options& options) {
        [NSApplication sharedApplication];
        target_ = [FLTKTrayTarget new]; target_.owner = this;
        item_ = [[NSStatusBar systemStatusBar] statusItemWithLength:NSSquareStatusItemLength];
        item_.visible = NO;
        item_.button.target = target_; item_.button.action = @selector(click:);
        [item_.button sendActionOn:NSEventMaskLeftMouseUp | NSEventMaskRightMouseUp];
        item_.button.toolTip = text(options.title);
    }
    ~MacBackend() override {
        target_.owner = nullptr;
        item_.button.target = nil;
        [[NSStatusBar systemStatusBar] removeStatusItem:item_];
    }
    bool icon(const Fl_RGB_Image& image) override {
        const int w = image.data_w(), h = image.data_h();
        NSBitmapImageRep* bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nullptr
            pixelsWide:w pixelsHigh:h bitsPerSample:8 samplesPerPixel:image.d() hasAlpha:image.d() == 4
            isPlanar:NO colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:NSBitmapFormatAlphaNonpremultiplied
            bytesPerRow:w * image.d() bitsPerPixel:image.d() * 8];
        if (!bitmap) return false;
        const int stride = image.ld() ? image.ld() : w * image.d();
        for (int y = 0; y < h; ++y) std::memcpy(bitmap.bitmapData + y * bitmap.bytesPerRow, image.array + y * stride, w * image.d());
        NSImage* icon = [[NSImage alloc] initWithSize:NSMakeSize(20, 20)];
        [icon addRepresentation:bitmap]; [icon setTemplate:NO]; item_.button.image = icon;
        return true;
    }
    void tooltip(const std::string& value) override { item_.button.toolTip = text(value); }
    void menu(const std::vector<Fltk_TrayIcon::MenuItem>& items, int) override {
        menu_ = [[NSMenu alloc] initWithTitle:@""]; menu_.autoenablesItems = NO;
        for (const auto& entry : items) {
            if (entry.label.empty()) { [menu_ addItem:[NSMenuItem separatorItem]]; continue; }
            NSMenuItem* child = [[NSMenuItem alloc] initWithTitle:text(entry.label) action:@selector(select:) keyEquivalent:@""];
            child.target = target_; child.tag = entry.id; child.enabled = entry.enabled;
            child.state = entry.checkable && entry.checked ? NSControlStateValueOn : NSControlStateValueOff;
            [menu_ addItem:child];
        }
    }
    void show(bool visible) override {
        shown_ = visible; item_.visible = visible;
        if (!visible) { click_ = {}; events_.clear(); }
    }
    bool available() const override { return shown_ && item_ && item_.visible; }
    void click() {
        NSEvent* event = [NSApp currentEvent];
        if (event.type == NSEventTypeRightMouseUp) {
            if (menu_) [item_ popUpStatusItemMenu:menu_];
        } else if (event.clickCount >= 2) {
            click_ = {}; events_.push_back({Fltk_TrayIcon::Event::DoubleClick, 0});
        } else click_ = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(static_cast<int>([NSEvent doubleClickInterval] * 1000));
    }
    void select(int id) { events_.push_back({Fltk_TrayIcon::Event::Menu, id}); }
    Events poll() override {
        if (click_ != std::chrono::steady_clock::time_point{} && std::chrono::steady_clock::now() >= click_) {
            click_ = {}; events_.push_back({Fltk_TrayIcon::Event::Activate, 0});
        }
        Events result; result.swap(events_); return result;
    }
};
std::unique_ptr<Backend> make_backend(const Fltk_TrayIcon::Options& options) { return std::make_unique<MacBackend>(options); }
}
@implementation FLTKTrayTarget
- (void)click:(id)sender {
    (void)sender;
    if (self.owner) static_cast<fltk_trayicon_detail::MacBackend*>(self.owner)->click();
}
- (void)select:(id)sender {
    if (self.owner) static_cast<fltk_trayicon_detail::MacBackend*>(self.owner)->select(static_cast<int>([sender tag]));
}
@end
