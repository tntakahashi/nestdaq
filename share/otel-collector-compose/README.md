# OpenTelemetry Collector Container Setups

[English](README.md) | [日本語](README.ja.md)

[Top: NestDAQ](../../README.md) | [Previous: Redis containers](../redis-stack-container/README.md) | [Next: OpenSearch setup](opensearch/README.md)

In this documentation, **Compose** means either Docker Compose (`docker compose`) or Podman Compose (`podman compose`).
This directory contains Compose setups for local validation.
Each setup uses OpenTelemetry Collector to receive OpenTelemetry data and forward it to the selected storage destination.
In this document, a **storage stack** combines a data store such as OpenSearch with a viewing tool such as OpenSearch Dashboards.

These stacks are intended for local validation only.
They publish service ports on the host and, where applicable, use simple local credentials.
Do not expose them on a public or shared network.

Choose one storage-stack directory:

- [`opensearch/`](opensearch/README.md): Stores logs and traces in OpenSearch and displays them in OpenSearch Dashboards.
- [`victoria/`](victoria/README.md): Stores logs, metrics, and traces in VictoriaLogs, VictoriaMetrics, and VictoriaTraces, and displays them in Grafana.
  This storage stack is experimental and not yet fully verified.
- [`clickhouse/`](clickhouse/README.md): Stores logs, metrics, and traces in ClickStack and displays them in the ClickStack user interface (UI).
  This storage stack is experimental and not yet fully verified.

## 1. Start

After installation, copy the installed setup to a working directory.
If `./otel-collector-compose/` already exists, remove it first or choose a different destination.
In the shell command examples below, lines beginning with `#` are explanatory comments for the reader and are not executed by the shell.

```bash
# Copy the installed setup and enter the working copy.
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
cd ./otel-collector-compose
```

Start one Compose stack from its storage-stack directory:

```bash
# Enter the OpenSearch directory and start its stack.
cd opensearch
docker compose -f compose-opensearch.yaml up
```

```bash
# Enter the Victoria directory and start its stack.
cd victoria
docker compose -f compose-victoria.yaml up
```

```bash
# Enter the ClickHouse directory and start its stack.
cd clickhouse
docker compose -f compose-clickhouse.yaml up
```

For Podman, use the same files with `podman compose`.

If you run multiple stacks at the same time, override conflicting host ports such as `GRAFANA_PORT`, `CLICKSTACK_UI_PORT`, `OTEL_COLLECTOR_GRPC_PORT`, and `OTEL_COLLECTOR_HTTP_PORT`.
OTLP means OpenTelemetry Protocol.
By default, port `4317` carries OTLP over gRPC, and port `4318` carries OTLP over HTTP.

Each storage-stack directory is self-contained.
You can copy only the selected directory and run the stack from that copy.

## 2. Stop

Stop the selected Compose stack from its storage-stack directory:

```bash
# Stop and remove the OpenSearch validation containers and network.
docker compose -f compose-opensearch.yaml down
```

For Podman, use the same Compose file with `podman compose`.

The `down` command stops and removes the local validation containers and network.
It does not delete bind-mounted data directories.
Starting the same storage stack again with the same data directories reuses the previous data.
Delete those directories only when you want to discard the stored data.
See the storage-stack README for the exact directory names.

## 3. Telemetry Endpoints

Choose the telemetry endpoint according to where the NestDAQ process runs.
The same rule applies to NestDAQ device processes and `daq-webctl`.

| Sender location | OpenSearch/Victoria endpoint | ClickStack endpoint |
| :-- | :-- | :-- |
| Host process using published ports | gRPC: `localhost:4317`; HTTP: `http://localhost:4318` | gRPC: `localhost:4317`; HTTP: `http://localhost:4318` |
| Container in the same Compose network | gRPC: `otel-collector:4317`; HTTP: `http://otel-collector:4318` | gRPC: `clickstack:4317`; HTTP: `http://clickstack:4318` |
| Container outside the Compose network using host-published ports | Docker, gRPC: `host.docker.internal:4317`; Docker, HTTP: `http://host.docker.internal:4318`; Podman, gRPC: `host.containers.internal:4317`; Podman, HTTP: `http://host.containers.internal:4318` | Docker, gRPC: `host.docker.internal:4317`; Docker, HTTP: `http://host.docker.internal:4318`; Podman, gRPC: `host.containers.internal:4317`; Podman, HTTP: `http://host.containers.internal:4318` |

For OTLP HTTP, use the signal-specific paths required by the telemetry client, such as `/v1/logs`, `/v1/metrics`, and `/v1/traces`.

## 4. Storage Stack Details

See the README for each storage stack:

- `opensearch/README.md`
- `victoria/README.md`
- `clickhouse/README.md`

All stacks use pinned image defaults.
You can override the images with environment variables documented in the corresponding README.

All Compose files already include the `:Z` label option on their bind mounts.
On Security-Enhanced Linux (SELinux)-enabled systems, Docker or Podman relabels each path for private use by the container.
No Compose-file changes are normally required.
