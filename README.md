# kwin-keymapper

Application specific keyboard remapper for KWin

## Install
- Run
  ```sh
  make install DEVICE=/dev/input/by-id/usb-<YOUR_KEYBOARD>-event-kbd`
  ```
  - Example:
    ```sh
    make install DEVICE=/dev/input/by-id/usb-DZTECH_DZ65RGB-event-kbd
    ```
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
The default keymap aims to mirror MacOS shortcuts (<kbd>Alt</kbd>+<kbd>C</kbd> to copy, <kbd>Alt</kbd>+<kbd>V</kbd> to paste), and works in both terminals and GUI apps

Edit your own keymaps in [`src/config.cpp`](src/config.cpp), see `user_key_map()`

Limitations: Only modifier keys combined with a single non-modifier key are supported (e.g. <kbd>Alt</kbd>+<kbd>Shift</kbd>+<kbd>F</kbd>)

## Development
- Generate compile_commands.json:
  ```sh
  make clean && rm -rf compile_commands.json && CXXFLAGS=-DAUTO_EXIT bear -- make all
  ```
- Run tests:
  ```sh
  make test
  ```
- Run the tool:
  ```sh
  CXXFLAGS="-DAUTO_EXIT -DREPORT_STATS" make && sudo out/kwin-keymapper --dbus-addr $DBUS_SESSION_BUS_ADDRESS --device-file /dev/input/eventX
  ```
