# DAQ Web Controller Implementation {#nestdaq_controller}

This directory contains the implementation of `daq-webctl`, the NestDAQ web
controller process. It provides an HTTP server for the browser UI, WebSocket
sessions for interactive clients, and Redis-backed control operations for DAQ
devices.

The static browser assets served by `daq-webctl` are documented separately in
[`share/controller/README.md`](../share/controller/README.md).

## Runtime Role

`daq-webctl` listens on an HTTP endpoint, serves the configured document root,
and accepts WebSocket clients. Commands from the browser are translated into
Redis-backed DAQ control operations, while state updates are sent back to
connected WebSocket clients.

At startup, `daq-webctl` configures FairLogger output and can load the optional
NestDAQ OpenTelemetry plugin through the shared telemetry loader. The controller
does not link OpenTelemetry directly.

## Main Components

| Component | Purpose |
| :-- | :-- |
| `run_daq-webctl.cxx` | Executable entry point, command-line parsing, logging, telemetry, Redis setup, and server startup. |
| `HttpWebSocketServer` | Owns the Boost.Asio I/O context, signal handling, listener, and worker threads. |
| `listener` | Accepts TCP connections and starts HTTP sessions. |
| `http_session` | Handles HTTP requests and upgrades WebSocket requests. |
| `websocket_session` | Manages one WebSocket client connection. |
| `WebSocketHandle` | Dispatches JSON messages received from WebSocket clients. |
| `WebGui` | Implements Redis-backed DAQ control, state polling, and command publication. |
| `beast_tools` | Provides shared Boost.Beast HTTP response helpers. |
| `DaqWebControlDefaultDocRootPath.h.in` | Generates the default installed document root path used by `--doc-root`. |

## Typical Usage

```sh
daq-webctl --http-uri=http://0.0.0.0:8080 --redis-uri=tcp://127.0.0.1:6379
```

Open `http://localhost:8080/` or `http://localhost:8080/daq-webctl.html` after
the process starts. The Redis server and DAQ devices must be available for
control operations to succeed.

Use `daq-webctl --help` to inspect the available HTTP, Redis, FairLogger, and
OpenTelemetry options.
