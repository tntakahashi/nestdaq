# Examples {#nestdaq_examples}

This directory contains small NestDAQ device examples. The examples are a
standalone CMake project: build and install the main NestDAQ package first, then
configure this directory with `find_package(NestDAQ)`.

## Example Devices

| Executable | Purpose |
| :-- | :-- |
| `NullDevice` | Minimal FairMQ device that exercises the NestDAQ `runDevice.h` entry point and lifecycle hooks without data channels. |
| `Sampler` | Sends text messages through an output channel and demonstrates custom command-line options. |
| `Sink` | Receives single-part or multipart messages through an input channel and demonstrates channel callback setup. |

Each executable links to `NestDAQ::NestDAQ`, which provides the NestDAQ
`runDevice.h` integration, FairMQ/FairLogger dependencies, plugin search paths,
and optional telemetry loader support.

## Build

Install NestDAQ first, then configure the examples with the NestDAQ install
prefix in `CMAKE_PREFIX_PATH`.

```sh
cmake \
  -DCMAKE_PREFIX_PATH=<nestdaq-install-prefix> \
  -DCMAKE_INSTALL_PREFIX=<examples-install-prefix> \
  -B ./build-examples \
  -S ./examples
cmake --build ./build-examples --parallel
cmake --install ./build-examples
```

The examples do not need to be installed into the same prefix as NestDAQ, but
the runtime linker must be able to find NestDAQ, FairMQ, Boost, and related
libraries. The example CMake project sets an install rpath relative to the
example install prefix and uses link paths discovered through `NestDAQ::NestDAQ`.

## Running

Use the installed helper scripts or invoke the binaries directly with FairMQ
channel options. A typical local topology starts `Sampler` and `Sink` with
matching channel configuration.

```sh
Sampler --help
Sink --help
NullDevice --help
```

For script-based launch examples, see [`scripts/README.md`](../scripts/README.md).
