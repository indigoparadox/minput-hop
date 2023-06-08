# minput-hop

minput-hop (a play on input-leap) is a compact client-only library written in C89 for talking to Synergy KVM server and its forks.

Rough docs for API available at: [https://indigoparadox.github.io/minput-hop/](https://indigoparadox.github.io/minput-hop/)!

## Purpose

To have something simple and (relatively) easy to port to obsolete or embedded operating systems so they can share input devices with a KVM server.

## Ideas

 - Abstract socket send/recv/etc into layer that can be switched out for e.g.
   serial communication over RS-232.

