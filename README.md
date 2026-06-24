# kwin-keymapper

## Install
- Install the KWin script
```
make install
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

## Usage
```sh
make clean
make && sudo ./kwin-keymapper --dbus-addr $DBUS_SESSION_BUS_ADDRESS --device-file /dev/input/eventX
```

## Development
```sh
make clean
rm -rf compile_commands.json && CXXFLAGS=-DAUTO_EXIT bear -- make
CXXFLAGS=-DAUTO_EXIT make && sudo ./kwin-keymapper --dbus-addr $DBUS_SESSION_BUS_ADDRESS --device-file /dev/input/eventX
```
