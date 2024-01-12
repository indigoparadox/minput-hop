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

## References

- Programming Windows, 3rd Edition - Petzold, Charles
- [https://www.qemu.org/docs/master/interop/barrier.html](qemu) docs for Barrier protocol information.
- @cvtsi2sd@hachyderm.io for undocumented \*\_event() calls in Windows 3.

