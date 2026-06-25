# NestDAQ
A streaming data acquisition (DAQ) implementation for the particle measurements

## Quick links

- [Installation](INSTALL.md): prerequisites, external dependencies, NestDAQ build, examples, and optional documentation generation.
- [Examples](examples/README.md): sample NestDAQ devices and a local run sequence for Redis, OpenTelemetry, `daq-webctl`, `NullDevice`, `Sampler`, and `Sink`.
- [Scripts](scripts/README.md): helper scripts and topology examples for running DAQ processes.
- [DAQ web controller implementation](controller/README.md): `daq-webctl` server, WebSocket, Redis control, and telemetry setup.
- [Web controller assets](share/controller/README.md): static files used by `daq-webctl`.
- [OpenTelemetry Collector Compose setup](share/otel-collector-compose/README.md): local OpenTelemetry, OpenSearch, and OpenSearch Dashboards stack.

## Directory layout

| Path                                     | Purpose |
| :--                                      | :--     |
| `cmake/`                                 | External dependency build project. |
| `nestdaq/`                               | Public NestDAQ headers and runtime helpers. |
| [nestdaq/telemetry/](nestdaq/telemetry/README.md) | Optional OpenTelemetry integration. |
| [plugins/](plugins/README.md)            | FairMQ plugins for DAQ service, metrics, and parameter configuration. |
| [controller/](controller/README.md)      | `daq-webctl` implementation. |
| [examples/](examples/README.md)          | Example devices such as `Sampler`, `Sink`, and `NullDevice`; see its README for detailed run steps. |
| [scripts/](scripts/README.md)            | Runtime helper scripts and topology examples. |
| `share/`                                 | Installed runtime/configuration assets. |

## Tested system
| System    | Version | Compiler                     | CMake           |
| ---       | ---     | ---                          | ---             | 
| AlmaLinux | 9       | GCC 11.5.0                   | 3.26.5 or later |
| AlmaLinux | 9       | GCC 14.2.1 (gcc-toolset-14)  | 3.26.5 or later |
| AlmaLinux | 10      | GCC 14.2.1                   | 3.30.5 or later |

## Dependencies to build NestDAQ

| Packages         | Version                              | Uniform Resource Locator (URL) |
| ---              | ---                                  | --- |
| Boost            | 1.72.0 or later                      | |
| FairLogger       | 1.9.0  or later                      | |
| FairMQ           | 1.4.26 or later                      | |
| hiredis          | 1.0.0  or later                      | https://github.com/redis/hiredis/ |
| redis-plus-plus  | 1.2.1 <br> (recipes branch) or later | https://github.com/sewenew/redis-plus-plus|
