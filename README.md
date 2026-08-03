# kwin-keymapper

Application specific keyboard remapper for KWin

## Install
- Run `make install DEVICE=/dev/input/by-id/usb-<YOUR_KEYBOARD>-event-kbd`
  - Example: `make install DEVICE=/dev/input/by-id/usb-DZTECH_DZ65RGB-event-kbd`
- Enable the KWin script in System Settings
- Add DBus policy to allow root user to access session bus. Create the file `/etc/dbus-1/session-local.conf` with the following content:
```xml
<busconfig>
  <policy context="mandatory">
    <allow user="root"/>
  </policy>
</busconfig>
```
- Check Systemd service log: `sudo journalctl -u kwin-keymapper -fb`

## Configuration
Edit your keymaps in [`src/config.cpp`](src/config.cpp), see `user_key_map()`

## Usage
```sh
make clean
make && sudo out/kwin-keymapper --dbus-addr $DBUS_SESSION_BUS_ADDRESS --device-file /dev/input/eventX
```

## Development
```sh
make clean
rm -rf compile_commands.json && CXXFLAGS=-DAUTO_EXIT bear -- make all
CXXFLAGS=-DAUTO_EXIT make && sudo out/kwin-keymapper --dbus-addr $DBUS_SESSION_BUS_ADDRESS --device-file /dev/input/eventX
```
