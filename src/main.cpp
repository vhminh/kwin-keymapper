#include "argparse.h"
#include "def.h"
#include "defer.h"
#include "keymapper.h"
#include "log.h"
#include "window.h"

#include <dbus/dbus.h>
#include <fcntl.h>
#include <libevdev/libevdev-uinput.h>
#include <libevdev/libevdev.h>
#include <linux/input-event-codes.h>
#include <linux/input.h>
#include <memory>
#include <string>
#include <sys/poll.h>
#include <thread>
#include <unistd.h>

// this tool grabs your keyboard inputs
// while developing, you can't send SIGINT to the process without your keyboard :)
// set this flag to make it auto exits after 6 seconds
// #define AUTO_EXIT

#ifdef AUTO_EXIT
#include <chrono>
#endif

template <>
struct std::formatter<DBusError> : std::formatter<std::string> {
    auto format(const DBusError& err, std::format_context& ctx) const {
        return std::formatter<std::string>::format(
            std::format("DBusError(name={},message={})", err.name, err.message), ctx
        );
    }
};

bool any_key_pressed(libevdev* dev) {
    for (int key = 0; key < KEY_MAX; ++key) {
        if (libevdev_has_event_code(dev, EV_KEY, key)) {
            if (libevdev_get_event_value(dev, EV_KEY, key) != 0) {
                return true;
            }
        }
    }
    return false;
}

int wait_until_all_keys_released(libevdev* dev) {
    int tries = 20;
    while (tries-- > 0) {
        input_event ev;
        int err = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (err < 0 && err != -EAGAIN) {
            return err;
        }
        if (!any_key_pressed(dev)) {
            return 0;
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }
    return 1;
}

void loop(const char* dbus_addr, const char* kb_device_file) {
#ifdef AUTO_EXIT
    const auto start_time = std::chrono::steady_clock::now();
#endif
    // SETUP DBUS
    DBusError err;
    dbus_error_init(&err);
    DEFER(dbus_error_free(&err));

    LOG_INFO("connecting to DBus at: {}", dbus_addr);
    DBusConnection* conn = dbus_connection_open(dbus_addr, &err);
    if (dbus_error_is_set(&err)) {
        LOG_ERROR("error getting DBus session connection: {}", err);
        return;
    }
    DEFER(dbus_connection_unref(conn));

    if (!dbus_bus_register(conn, &err)) {
        LOG_ERROR("cannot register bus: {}", err);
        return;
    }

    int ret = dbus_bus_request_name(conn, "io.github.vhminh.kwin_keymapper", DBUS_NAME_FLAG_REPLACE_EXISTING, &err);
    if (dbus_error_is_set(&err)) {
        LOG_ERROR("error requesting DBus name: {}", err);
        return;
    }
    if (ret != DBUS_REQUEST_NAME_REPLY_PRIMARY_OWNER) {
        LOG_ERROR("not DBus name primary owner: {}", ret);
        return;
    }
    int dbus_fd;
    if (!dbus_connection_get_unix_fd(conn, &dbus_fd)) {
        LOG_ERROR("cannot get dbus connection Unix fd");
        return;
    }

    // SETUP KEY GRAB AND VIRT KEYBOARD
    const int kbd_fd = open(kb_device_file, O_RDONLY | O_NONBLOCK);
    if (kbd_fd == -1) {
        LOG_ERROR("failed to open device file {}", kb_device_file);
        return;
    }
    DEFER(close(kbd_fd));
    libevdev* kbd = nullptr;
    int rc;
    if ((rc = libevdev_new_from_fd(kbd_fd, &kbd)) != 0) {
        LOG_ERROR("failed to init libevdev: {}", rc);
        return;
    }
    DEFER(libevdev_free(kbd));
    LOG_INFO("input device name: {}", libevdev_get_name(kbd));
    bool is_keyboard = libevdev_has_event_type(kbd, EV_KEY) &&        //
                       libevdev_has_event_code(kbd, EV_KEY, KEY_M) && //
                       libevdev_has_event_code(kbd, EV_KEY, KEY_I) && //
                       libevdev_has_event_code(kbd, EV_KEY, KEY_N) && //
                       libevdev_has_event_code(kbd, EV_KEY, KEY_H) && //
                       libevdev_has_event_code(kbd, EV_KEY, KEY_ENTER);
    if (!is_keyboard) {
        LOG_ERROR("device is not a keyboard");
        return;
    }
    if ((rc = wait_until_all_keys_released(kbd)) != 0) {
        if (rc < 0) {
            LOG_ERROR("error reading next evdev event: {}", rc);
        } else {
            LOG_ERROR("get your hands off the keyboard, lol");
        }
        return;
    }
    if ((rc = libevdev_grab(kbd, LIBEVDEV_GRAB)) != 0) {
        LOG_ERROR("error grabing device fd: {}", rc);
        return;
    }
    DEFER(libevdev_grab(kbd, LIBEVDEV_UNGRAB));
    const int uinput_fd = open("/dev/uinput", O_RDWR);
    if (uinput_fd == -1) {
        LOG_ERROR("failed to open /dev/uinput");
        return;
    }
    DEFER(close(uinput_fd));
    libevdev_uinput* virtual_kbd = nullptr;
    if ((rc = libevdev_uinput_create_from_device(kbd, uinput_fd, &virtual_kbd)) != 0) {
        LOG_ERROR("cannot create virtual keyboard: {}", rc);
        return;
    }
    DEFER(libevdev_uinput_destroy(virtual_kbd));

    // MAIN LOOP
    KeyMapper keymapper;
    Box<Window> active_window;
    std::vector<input_event> events_to_send; // for reuse, avoiding allocation in a hot loop
    events_to_send.reserve(16);
    pollfd poll_fds[2] = {
        pollfd{.fd = dbus_fd, .events = POLLIN, .revents = 0}, pollfd{.fd = kbd_fd, .events = POLLIN, .revents = 0}
    };
    while (true) {
#ifdef AUTO_EXIT
        auto elapsed_duration = std::chrono::steady_clock::now() - start_time;
        using namespace std::chrono_literals;
        if (elapsed_duration > 6000ms) {
            LOG_INFO("auto exit");
            return;
        }
#endif
        const int n = poll(poll_fds, 2, 1000);
        if (n == 0 || (n == -1 && errno == EINTR)) {
            continue;
        }
        if (n < 0) {
            LOG_ERROR("poll error: {}, errno: {}", n, errno);
            return;
        }
        if (poll_fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            LOG_ERROR("poll DBus revents error: {}", poll_fds[0].revents);
            return;
        }
        if (poll_fds[1].revents & (POLLERR | POLLHUP | POLLNVAL)) {
            LOG_ERROR("poll evdev revents error: {}", poll_fds[1].revents);
            return;
        }

        // PROCESS DBUS MESSAGES
        if (!dbus_connection_read_write(conn, 0)) { // non blocking read
            LOG_ERROR("cannot read messages from DBus connection");
            return;
        }
        DBusMessage* msg;
        while ((msg = dbus_connection_pop_message(conn)) != nullptr) {
            DEFER({
                dbus_message_unref(msg);
                msg = nullptr;
            });

            if (!dbus_message_is_method_call(msg, "io.github.vhminh.kwin_keymapper", "active_window_changed")) {
                LOG_INFO(
                    "message doesn't match, interface: {}, type: {}, path: {}, dest: {}",
                    dbus_message_get_interface(msg),
                    dbus_message_get_type(msg),
                    dbus_message_get_path(msg),
                    dbus_message_get_destination(msg)
                );
                continue;
            }
            char* win_class = nullptr;
            char* win_name = nullptr;
            char* win_caption = nullptr;
            dbus_message_get_args(
                msg,
                &err,
                DBUS_TYPE_STRING,
                &win_class,
                DBUS_TYPE_STRING,
                &win_name,
                DBUS_TYPE_STRING,
                &win_caption,
                DBUS_TYPE_INVALID
            );
            if (dbus_error_is_set(&err)) {
                LOG_ERROR("error parsing message args: {}", err);
                continue;
            }
            active_window = std::make_unique<Window>(win_class, win_name, win_caption);
            LOG_INFO("window activated, class: {}, name: {}, caption: {}", win_class, win_name, win_caption);
        }

        // PROCESS EVDEV EVENTS
        input_event ev;
        do {
            rc = libevdev_next_event(kbd, LIBEVDEV_READ_FLAG_NORMAL, &ev);
            if (rc == -EAGAIN) {
                break;
            }
            switch (rc) {
            case LIBEVDEV_READ_STATUS_SUCCESS: {
                int write_err = 0;
                if (ev.type == EV_KEY) {
                    events_to_send.clear();
                    keymapper.process_evdev_key(active_window, ev, events_to_send);
                    for (const auto& e : events_to_send) {
                        if ((write_err = libevdev_uinput_write_event(virtual_kbd, e.type, e.code, e.value)) != 0) {
                            LOG_ERROR("libevdev_uinput_write_event error writing key: {}", write_err);
                            return;
                        }
                    }
                    if ((write_err = libevdev_uinput_write_event(virtual_kbd, EV_SYN, SYN_REPORT, 0)) != 0) {
                        LOG_ERROR("libevdev_uinput_write_event error writing EV_SYN: {}", write_err);
                        return;
                    }
                } else {
                    // pass through
                    if ((write_err = libevdev_uinput_write_event(virtual_kbd, ev.type, ev.code, ev.value)) != 0) {
                        LOG_ERROR("libevdev_uinput_write_event error writing event of type {}: {}", ev.type, write_err);
                        return;
                    }
                }
                break;
            }
            case LIBEVDEV_READ_STATUS_SYNC: {
                TODO("LIBEVDEV_READ_STATUS_SYNC not supported");
                break;
            }
            default: {
                LOG_ERROR("error reading next evdev event: {}", rc);
                return;
            }
            }
        } while (rc == LIBEVDEV_READ_STATUS_SUCCESS || rc == LIBEVDEV_READ_STATUS_SYNC);
    }
}

int main(int argc, const char* argv[]) {
    auto parser = ArgParser(argv[0])
                      .add_option(
                          ArgDef{
                              .name = "--dbus-addr",
                              .type = ArgType::STRING,
                              .desc = "User session DBus address",
                              .example = "$DBUS_SESSION_BUS_ADDRESS",
                              .required = true,
                          }
                      )
                      .add_option(
                          ArgDef{
                              .name = "--device-file",
                              .type = ArgType::STRING,
                              .desc = "Path to your keyboard input device file",
                              .example = "/dev/input/event8",
                              .required = true,
                          }
                      );
    if (parser.should_print_help(argc, argv)) {
        parser.print_help(std::cout);
        return 0;
    }
    try {
        parser.parse(argc, argv);
    } catch (const ArgParseException& ex) {
        LOG_ERROR("error parsing arguments: {}", ex.what());
        parser.print_help(std::cerr);
        return 1;
    }
    const char* dbus_addr = parser.opt<const char*>("--dbus-addr");
    const char* device_file = parser.opt<const char*>("--device-file");
    LOG_INFO("KWin keymapper starting");
#ifdef AUTO_EXIT
    LOG_INFO("AUTO_EXIT: true");
#else
    LOG_INFO("AUTO_EXIT: false");
#endif
    LOG_INFO("DBus address: {}", dbus_addr);
    LOG_INFO("Device file: {}", device_file);

    loop(dbus_addr, device_file);
    LOG_INFO("KWin keymapper stopped");
    return 0;
}
