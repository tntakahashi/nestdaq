# Web Controller Assets

This directory contains the browser assets installed for `daq-webctl`, the
NestDAQ web controller. The controller implementation and runtime behavior are
documented in [`controller/README.md`](../../controller/README.md).

## `daq-webctl.html`

`daq-webctl.html` is the default browser GUI served by `daq-webctl`. It is
installed under the controller document root as `daq-webctl.html`.

The install step also creates `index.html` as a symlink to this file, so the UI
can be opened either at `/daq-webctl.html` or `/`.

See [`controller/README.md`](../../controller/README.md) for startup commands,
Redis requirements, command-line options, and browser usage notes.
