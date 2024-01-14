# minput-hop

minput-hop (a play on input-leap) is a compact client-only library written in C89 for talking to Synergy KVM server and its forks.

Rough docs for API available at: [https://indigoparadox.github.io/minput-hop/](https://indigoparadox.github.io/minput-hop/)!

## Purpose

To have something simple and (relatively) easy to port to obsolete or embedded operating systems so they can share input devices with a KVM server.

## Architecture

The architecture is a little convoluted, but it's roughly like this:

 * Platform-specific startup happens in osio\_\*.c. This is WinMain() on Windows and main() elsewhere.
 * Platform-specific main sets up a common environment, gets configuration (server, client name, port, etc) into a NETIO\_CFG struct which it then passes to minput\_main() in main.c.
 * minput\_main() initializes common subsystems (network, UI) in order and calls osio\_loop(), again platform-specific in osio\_\*.c.
 * osio\_loop() repeated calls the loop iteration; either by setting up a timer and then starting the message loop on Windows, or directly elsewhere.

This might be simplified somewhat later, but it works for now.

## Ideas

 - Abstract socket send/recv/etc into layer that can be switched out for e.g.
   serial communication over RS-232.

## Known Issues

 - Does not handle the mouse being held down gracefully, especially in MSPAINT.
 - Does not handle mouse interactions well inside of its own window.

## Debug Defines

In addition to the DEBUG compile-time definition, some fine-grained debug statements have been placed inside the following definitions to keep verbosity in the logs down when not needed:

 - DEBUG\_FLOW
 - DEBUG\_CALV
 - DEBUG\_SEND
 - DEBUG\_PACKETS\_IN
 - DEBUG\_PROTO\_MOUSE
 - DEBUG\_PROTO\_CLIP

## References

- Programming Windows, 3rd Edition - Petzold, Charles
- [qemu](https://www.qemu.org/docs/master/interop/barrier.html) docs for Barrier protocol information.
- @cvtsi2sd@hachyderm.io for undocumented \*\_event() calls in Windows 3.
- [Bobobobo's weblog](https://bobobobo.wordpress.com/2009/03/30/adding-an-icon-system-tray-win32-c/) for a detailed example of using the tray notification area.

