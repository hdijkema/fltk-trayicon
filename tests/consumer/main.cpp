#include <fltk_trayicon.h>
int main() {
    Fltk_TrayIcon::Options options; options.enabled = false;
    Fltk_TrayIcon tray(options);
    tray.poll();
    return tray.available() ? 1 : 0;
}
