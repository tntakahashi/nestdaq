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
| Distro    | Version | Compiler    | CMake  | FairMQ |
| ---       | ---     | ---         | ---    | ---    |
| AlmaLinux | 8       | GCC 8.5.0   | 3.26.5 | 1.9.2  |
| AlmaLinux | 9       | GCC 11.5.0  | 3.31.8 | 1.10.0 |
| AlmaLinux | 10      | GCC 14.3.1  | 3.31.8 | 1.10.0 |
| Debian    | 12      | GCC 12.2.0  | 3.25.1 | 1.10.0 |
| Debian    | 13      | GCC 14.2.0  | 3.31.6 | 1.10.0 |
| Ubuntu    | 22.04   | GCC 11.4.0  | 3.22.1 | 1.10.0 |
| Ubuntu    | 24.04   | GCC 13.3.0  | 3.28.3 | 1.10.0 |
| Ubuntu    | 26.04   | GCC 15.2.0  | 4.2.3  | 1.10.0 |

## Dependencies to build NestDAQ

| Packages         | Version                              | Uniform Resource Locator (URL) |
| ---              | ---                                  | --- |
| Boost            | 1.72.0 or later                      | |
| FairLogger       | 1.9.0  or later                      | |
| FairMQ           | 1.4.26 or later                      | |
| hiredis          | 1.0.0  or later                      | https://github.com/redis/hiredis/ |
| redis-plus-plus  | 1.2.1 <br> (recipes branch) or later | https://github.com/sewenew/redis-plus-plus|

FairMQ 1.10.0 is used by default with GCC 9.1 or later. For GCC versions older
than 9.1, the dependency build defaults to FairMQ 1.9.2 because GCC 8.5 does not
provide the `std::pmr` support required by FairMQ 1.10.0.
