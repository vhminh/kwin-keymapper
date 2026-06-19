#include "config.h"
#include "defer.h"
#include "log.h"
#include "window.h"

#include <atomic>
#include <cstring>
#include <dbus/dbus.h>
#include <fcntl.h>
#include <libevdev/libevdev.h>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

template <typename T>
using Arc = std::shared_ptr<T>;

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
        DBusMessage* msg = dbus_connection_pop_message(conn);
        if (msg == nullptr) {
            continue;
        }
        DEFER(dbus_message_unref(msg));

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

        Arc<Window> old = active_window.load();
        if (old != nullptr) {
            LOG_INFO("window become inactive, class: {}, name: {}, caption: {}", old->xclass, old->name, old->caption);
        }
        Arc<Window> newly_active = std::make_shared<Window>(win_class, win_name, win_caption);
        active_window.store(newly_active);
        LOG_INFO("window activated, class: {}, name: {}, caption: {}", win_class, win_name, win_caption);
    }
}

void process_key(const std::stop_token& token, const std::atomic<Arc<Window>>& active_window) {
    // for (int id = 0; id < 25; ++id) {
    //     const int fd = open((string("/dev/input/event") + to_string(id)).c_str(), O_RDONLY | O_NONBLOCK);
    //     if (fd == -1) {
    //         cerr << "Failed to open /dev/input/event" << id << endl;
    //         return;
    //     }
    //     DEFER(close(fd));
    //     libevdev* dev = nullptr;
    //     int err = libevdev_new_from_fd(fd, &dev);
    //     if (err) {
    //         cerr << "Failed to init libevdev (" << err << ")" << endl;
    //         return;
    //     }
    //     cerr << id << " Input device name: " << libevdev_get_name(dev) << endl;
    // }
    while (!token.stop_requested()) {
        auto w = active_window.load();
        user_process_key(active_window.load(), 0);
        // input_event ev;
        // libevdev_next_event();
    }
}

void print_help() {
    std::cout << "Usage: sudo kwin-keymapper $DBUS_SESSION_BUS_ADDRESS" << std::endl;
}

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        print_help();
        return 1;
    }
    if (std::strcmp(argv[1], "--help") == 0 || std::strcmp(argv[1], "-h") == 0) {
        print_help();
        return 0;
    }
    LOG_INFO("KWin keymapper starting");
    LOG_INFO("DBus address: {}", argv[1]);

    std::atomic<Arc<Window>> active_window;
    std::stop_source cancel;
    std::jthread t1([&]() {
        LOG_INFO("starting new thread to monitor DBus for active window change");
        try {
            monitor_active_window(cancel.get_token(), argv[1], active_window);
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
            process_key(cancel.get_token(), active_window);
            cancel.request_stop();
        } catch (...) {
            cancel.request_stop();
            throw;
        }
        LOG_INFO("stopped processing evdev event thread");
    });

    t1.join();
    t2.join();
    LOG_INFO("KWin keymapper stopped");
    return 0;
}
