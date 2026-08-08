# NestDAQ

[English](README.md) | [日本語](README.ja.md)

[Next: Installation](INSTALL.md)

NestDAQ is a framework for building streaming data acquisition (DAQ) applications for particle-beam measurements.
Building and installing this repository provides common components and tools, but does not by itself create a DAQ application that works with real detector electronics.
DAQ application code for specific detectors and hardware is outside the scope of this repository.

## 1. Project guide

| Path / document | Purpose |
| :-- | :-- |
| [INSTALL.md](INSTALL.md) | Prerequisites, dependency versions and build options, NestDAQ build options, external services, examples, and documentation generation. |
| [examples/](examples/README.md) | Example devices such as `Sampler`, `Sink`, and `NullDevice`, detailed local run procedures, and guidance for [creating custom user devices](examples/README.md#4-creating-your-own-user-device). |
| [scripts/](scripts/README.md) | Process startup and topology helper scripts, including the `generate-device-skeleton.py` device skeleton generator. |
| [controller/](controller/README.md) | The `daq-webctl` HTTP/WebSocket server, Redis control, and telemetry setup. |
| [share/controller/](share/controller/README.md) | Browser files served by `daq-webctl`. |
| `nestdaq/` | Core headers and telemetry code, including the version header template and `runDevice.h`, which provides the FairMQ device application entry point and `main()`. |
| [nestdaq/telemetry/](nestdaq/telemetry/README.md) | OpenTelemetry integration that can be enabled when needed. |
| [plugins/](plugins/README.md) | FairMQ plugins for DAQ services, metrics, and parameter configuration. |
| [cmake/](cmake/README.md) | CMake helpers, installed package files, and the external dependency build project. |
| [tests/](tests/) | C++ tests and test support files. |
| `share/` | Configuration and helper files installed with NestDAQ. |
| [share/otel-collector-compose/](share/otel-collector-compose/README.md) | Local OpenTelemetry Collector and backend Compose stacks, run with `docker compose` or `podman compose`, for [OpenSearch](share/otel-collector-compose/opensearch/README.md), [Victoria](share/otel-collector-compose/victoria/README.md), and [ClickStack](share/otel-collector-compose/clickhouse/README.md). |
| [share/redis-stack-container/](share/redis-stack-container/README.md) | Helper scripts for running [Redis Stack](INSTALL.md#external-runtime-components) containers. |
| [share/installers/](share/installers/README.md) | `apt` and `dnf` helper scripts for installing external services on the host. |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Branch policy, contribution workflow, formatting, static analysis, and naming rules. |

## 2. Tested systems
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

## 3. Dependencies

NestDAQ uses Boost, FairLogger, FairMQ, hiredis, and redis-plus-plus.
When the corresponding features are enabled, NestDAQ also uses telemetry and logging dependencies such as opentelemetry-cpp and spdlog.
See [INSTALL.md](INSTALL.md) for prerequisite packages, default dependency versions, CMake override options, external service choices, and platform-specific build limitations.
