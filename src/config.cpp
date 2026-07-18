#include "config.h"

#include <linux/input-event-codes.h>
#include <set>

std::set<std::tuple<std::string_view, std::string_view>> terminal_emulators = {
    std::make_tuple("Alacritty", "alacritty"),
    std::make_tuple("org.kde.konsole", "konsole"),
    std::make_tuple("jetbrains-idea", "idea"), // intellij with ideavim uses a very similar keymapping to a terminal :)
};

bool is_terminal_emulator(const Window& window) {
    return terminal_emulators.find(std::make_tuple(window.xclass, window.name)) != terminal_emulators.end();
}

std::tuple<ModMask, u16> user_key_map(const Box<Window>& active_window, ModMask mods, u16 evdev_key) {
    Window* win;
    if ((win = active_window.get()) != nullptr && is_terminal_emulator(*win)) {
        switch (mods) {
        case LEFT_ALT:
            switch (evdev_key) {
            case KEY_C:
                return std::make_tuple(LEFT_CTRL | LEFT_SHIFT, KEY_C);
            case KEY_X:
                return std::make_tuple(LEFT_CTRL | LEFT_SHIFT, KEY_X);
            case KEY_V:
                return std::make_tuple(LEFT_CTRL | LEFT_SHIFT, KEY_V);
            }
            break;
        }
    } else {
        switch (mods) {
        case LEFT_ALT:
            switch (evdev_key) {
            case KEY_C: // copy
                return std::make_tuple(LEFT_CTRL, KEY_C);
            case KEY_X: // cut
                return std::make_tuple(LEFT_CTRL, KEY_X);
            case KEY_V: // paste
                return std::make_tuple(LEFT_CTRL, KEY_V);
            case KEY_A: // select all
                return std::make_tuple(LEFT_CTRL, KEY_A);
            case KEY_Z: // undo
                return std::make_tuple(LEFT_CTRL, KEY_Z);
            case KEY_F: // find
                return std::make_tuple(LEFT_CTRL, KEY_F);
            case KEY_R: // reload
                return std::make_tuple(LEFT_CTRL, KEY_R);
            case KEY_T: // new tab
                return std::make_tuple(LEFT_CTRL, KEY_T);
            case KEY_W: // close tab
                return std::make_tuple(LEFT_CTRL, KEY_W);
            }
            break;
        case LEFT_ALT | LEFT_SHIFT: {
            switch (evdev_key) {
            case KEY_Z: // redo
                return std::make_tuple(LEFT_CTRL | LEFT_SHIFT, KEY_Z);
            case KEY_F: // find all
                return std::make_tuple(LEFT_CTRL | LEFT_SHIFT, KEY_F);
            }
            break;
        }
        case LEFT_CTRL: {
            switch (evdev_key) {
            case KEY_N: // next
                return std::make_tuple(NONE, KEY_DOWN);
            case KEY_P: // previous
                return std::make_tuple(NONE, KEY_UP);
            }
            break;
        }
        }
    }
    return std::make_tuple(mods, evdev_key);
}
