# kwin-keymapper

## Install
```
make install
```
add policy to allow su to access session bus

/etc/dbus-1/session-local.conf
```
<busconfig>
  <policy context="mandatory">
    <allow user="root"/>
  </policy>
</busconfig>
```


## Usage
```sh
make && sudo ./kwin-keymapper $DBUS_SESSION_BUS_ADDRESS
```
