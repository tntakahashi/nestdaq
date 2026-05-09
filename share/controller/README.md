# Web Controller Assets

This directory contains the browser assets installed for `daq-webctl`, the
NestDAQ web controller. The controller implementation and runtime behavior are
documented in [`controller/README.md`](../../controller/README.md).

## `daq-webctl.html`

`daq-webctl.html` is the default browser GUI served by `daq-webctl`. It is
installed under the controller document root as `daq-webctl.html`.

The install step also creates `index.html` as a symlink to this file, so the UI
can be opened either at `/daq-webctl.html` or `/`.

```bash
# The following command shows command options.
/your-install-path/bin/daq-webctl --help

# Redis server must be started before starting daq-webctl.
/your-install-path/bin/daq-webctl

```

After starting `daq-webctl`, open `http://localhost:8080/daq-webctl.html` or
`http://localhost:8080/` in a web browser.

Note:
- Run number must be set before entering to the Running state.
