#include "argparse.h"
#include "defer.h"
#include "intercept.h"
#include "log.h"
#include "window.h"

#include <argp.h>
#include <atomic>
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

void monitor_active_window(
    const std::stop_token& token, const char* dbus_addr, std::atomic<Arc<Window>>& active_window
) {
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

    // poll with 1000ms timeout so stop token is checked
    while (dbus_connection_read_write(conn, 1000) && !token.stop_requested()) {
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

            Arc<Window> newly_active = std::make_shared<Window>(win_class, win_name, win_caption);
            active_window.store(newly_active);
            LOG_INFO("window activated, class: {}, name: {}, caption: {}", win_class, win_name, win_caption);
        }
    }
}

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

bool wait_until_all_keys_released(libevdev* dev) {
    int tries = 20;
    while (tries-- > 0) {
        input_event ev;
        int err = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (err < 0 && err != -EAGAIN) {
            LOG_ERROR("error reading next event: {}", err);
            return true;
        }
        if (!any_key_pressed(dev)) {
            return true;
        }
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(100ms);
    }
    return false;
}

void process_key(
    const std::stop_token& token, const char* kb_device_file, const std::atomic<Arc<Window>>& active_window
) {
    const int fd = open(kb_device_file, O_RDONLY | O_NONBLOCK);
    if (fd == -1) {
        LOG_ERROR("failed to open device file {}", kb_device_file);
        return;
    }
    DEFER(close(fd));
    libevdev* kbd = nullptr;
    int err;
    if ((err = libevdev_new_from_fd(fd, &kbd)) != 0) {
        LOG_ERROR("failed to init libevdev: {}", err);
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
    if (!wait_until_all_keys_released(kbd)) {
        LOG_ERROR("get your hands off the keyboard, lol");
        return;
    }
    if ((err = libevdev_grab(kbd, LIBEVDEV_GRAB)) != 0) {
        LOG_ERROR("error grabing device fd: {}", err);
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
    if ((err = libevdev_uinput_create_from_device(kbd, uinput_fd, &virtual_kbd)) != 0) {
        LOG_ERROR("cannot create virtual keyboard: {}", err);
        return;
    }
    DEFER(libevdev_uinput_destroy(virtual_kbd));
    EvDevInterceptor interceptor;
    pollfd pfd{.fd = fd, .events = POLLIN, .revents = 0};
    std::vector<input_event> events_to_send; // for reuse, avoiding allocation in a hot loop
    events_to_send.reserve(16);
    while (!token.stop_requested()) {
        const int n = poll(&pfd, 1, 1000);
        if (n < 0) {
            LOG_ERROR("poll error: {}", n);
            return;
        }
        if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
            LOG_ERROR("poll revents error: {}", pfd.revents);
            return;
        }
        if (n == 0) {
            continue;
        }
        input_event ev;
        Arc<Window> cur_active_window = active_window.load(); // atomic load once per poll wakeup
        do {
            err = libevdev_next_event(kbd, LIBEVDEV_READ_FLAG_NORMAL, &ev);
            if (err == -EAGAIN) {
                break;
            }
            switch (err) {
            case LIBEVDEV_READ_STATUS_SUCCESS: {
                if (ev.type == EV_KEY) {
                    events_to_send.clear();
                    interceptor.process_evdev_key(cur_active_window, ev, events_to_send);
                    for (const auto& ev : events_to_send) {
                        libevdev_uinput_write_event(virtual_kbd, ev.type, ev.code, ev.value);
                    }
                    libevdev_uinput_write_event(virtual_kbd, EV_SYN, SYN_REPORT, 0);
                } else {
                    // pass through
                    libevdev_uinput_write_event(virtual_kbd, ev.type, ev.code, ev.value);
                }
                break;
            }
            case LIBEVDEV_READ_STATUS_SYNC: {
                TODO("LIBEVDEV_READ_STATUS_SYNC not supported");
                break;
            }
            default: {
                LOG_ERROR("error reading next evdev event: {}", err);
                return;
            }
            }
        } while (!token.stop_requested() && (err == LIBEVDEV_READ_STATUS_SUCCESS || err == LIBEVDEV_READ_STATUS_SYNC));
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

    std::atomic<Arc<Window>> active_window;
    std::stop_source cancel;
    std::jthread t1([&]() {
        LOG_INFO("starting new thread to monitor DBus for active window change");
        try {
            monitor_active_window(cancel.get_token(), dbus_addr, active_window);
            cancel.request_stop();
        } catch (...) {
            cancel.request_stop();
            throw;
        }
        LOG_INFO("stopped monitoring active window thread");
    });
    std::jthread t2([&]() {
        LOG_INFO("starting new thread to process evdev events");
        try {
            process_key(cancel.get_token(), device_file, active_window);
            cancel.request_stop();
        } catch (...) {
            cancel.request_stop();
            throw;
        }
        LOG_INFO("stopped processing evdev event thread");
    });

#ifdef AUTO_EXIT
    {
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(6000ms);
        cancel.request_stop();
    }
#endif

    t1.join();
    t2.join();
    LOG_INFO("KWin keymapper stopped");
    return 0;
}
