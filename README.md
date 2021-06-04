# OSC Serial to UDP Bridge

Work in progress for an OSC bridge between UDP and a serial port.
OSC packets over serial are SLIP encoded.

To start the server simply call:
```
osc_udp_serial_bridge <remote-udp-port> <local-udp-port> <serial-port>
```