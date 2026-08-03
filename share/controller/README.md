# Web Controller Assets

[English](README.md) | [日本語](README.ja.md)

This directory contains the browser assets installed for `daq-webctl`, the
NestDAQ web controller. The controller implementation and behavior after
startup are documented in [`controller/README.md`](../../controller/README.md).

## 1. `daq-webctl.html`

`daq-webctl.html` is the default browser graphical user interface (GUI) served
by `daq-webctl`. It is installed under the controller document root as
`daq-webctl.html`.

The install step also creates `index.html` as a symlink to this file, so the
user interface (UI) can be opened either at `/daq-webctl.html` or `/`.

See [`controller/README.md`](../../controller/README.md) for startup commands,
Redis requirements, command-line options, and browser usage notes.
