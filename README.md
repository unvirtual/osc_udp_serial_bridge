# OSC Serial UDP Bridge

A `boost::asio` based UDP <-> Serial bridge for OSC messages encoded in SLIP.

Dependencies:
* Boost
* [libusbp](https://github.com/pololu/libusbp) (for the executable)

If you encounter issues with dropped UDP packets, increase OS UDP buffers:
```
$ sudo sysctl -w net.core.rmem_max=26214400
$ sudo sysctl -w net.core.rmem_default=26214400
```