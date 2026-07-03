# NestDAQ
A streaming data acquisition (DAQ) implementation for the particle measurements

## Project guide

| Path / document | Purpose |
| :-- | :-- |
| [INSTALL.md](INSTALL.md) | Prerequisites, external dependency versions and build options, NestDAQ build options, runtime service choices, examples, and optional documentation generation. |
| [examples/](examples/README.md) | Example devices such as `Sampler`, `Sink`, and `NullDevice`; see its README for detailed local run steps and [custom user device creation](examples/README.md#creating-your-own-user-device). |
| [scripts/](scripts/README.md) | Runtime start/topology helper scripts and the `generate-device-skeleton.py` device skeleton generator. |
| [controller/](controller/README.md) | `daq-webctl` HTTP/WebSocket server, Redis control, and telemetry setup. |
| [share/controller/](share/controller/README.md) | Browser assets served by `daq-webctl`. |
| `nestdaq/` | Public NestDAQ headers and runtime helpers. |
| [nestdaq/telemetry/](nestdaq/telemetry/README.md) | Optional OpenTelemetry integration. |
| [plugins/](plugins/README.md) | FairMQ plugins for DAQ service, metrics, and parameter configuration. |
| [cmake/](cmake/README.md) | CMake helpers, installed package files, and external dependency build project. |
| [tests/](tests/) | C++ tests and test support files. |
| `share/` | Runtime/configuration assets installed with NestDAQ. |
| [share/otel-collector-compose/](share/otel-collector-compose/README.md) | Local OpenTelemetry Collector and backend Compose stacks for [OpenSearch](share/otel-collector-compose/opensearch/README.md), [Victoria](share/otel-collector-compose/victoria/README.md), and [ClickStack](share/otel-collector-compose/clickhouse/README.md). |
| [share/redis-stack-container/](share/redis-stack-container/README.md) | Redis Stack container helper scripts. |
| [share/installers/](share/installers/README.md) | Host package installer helper scripts for runtime services with `apt` or `dnf`. |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution workflow, formatting, static analysis, and naming rules. |

## Tested systems
| Distro    | Version | Compiler    | CMake  | FairMQ |
| ---       | ---     | ---         | ---    | ---    |
| AlmaLinux | 8.10    | GCC 8.5.0   | 3.26.5 | 1.9.2  |
| AlmaLinux | 9.8     | GCC 11.5.0  | 3.31.8 | 1.10.0 |
| AlmaLinux | 10.2    | GCC 14.3.1  | 3.31.8 | 1.10.0 |
| Debian    | 12      | GCC 12.2.0  | 3.25.1 | 1.10.0 |
| Debian    | 13      | GCC 14.2.0  | 3.31.6 | 1.10.0 |
| Ubuntu    | 22.04   | GCC 11.4.0  | 3.22.1 | 1.10.0 |
| Ubuntu    | 24.04   | GCC 13.3.0  | 3.28.3 | 1.10.0 |
| Ubuntu    | 26.04   | GCC 15.2.0  | 4.2.3  | 1.10.0 |

## Dependencies

NestDAQ uses Boost, FairLogger, FairMQ, hiredis, redis-plus-plus, and optional
telemetry/logging dependencies such as opentelemetry-cpp and spdlog. See
[INSTALL.md](INSTALL.md) for prerequisite packages, default dependency
versions, CMake override options, runtime service choices, and platform-specific
build caveats.
