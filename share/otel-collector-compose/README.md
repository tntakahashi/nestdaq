# OpenTelemetry Collector Container Setups

[English](README.md) | [日本語](README.ja.md)

[Top: NestDAQ](../../README.md) | [Previous: Redis containers](../redis-stack-container/README.md) | [Next: OpenSearch setup](opensearch/README.md)

In this documentation, **Compose** means either Docker Compose (`docker compose`) or Podman Compose (`podman compose`).
This directory contains Compose setups for local validation.
Each setup uses OpenTelemetry Collector to receive OpenTelemetry data and store it in the selected backend.

These stacks are intended for local validation only.
They publish service ports on the host and, where applicable, use simple local credentials.
Do not expose them on a public or shared network.

No default `compose.yaml` or `docker-compose.yaml` is installed.
Choose one backend directory explicitly:

- [`opensearch/`](opensearch/README.md): Stores logs and traces in OpenSearch and displays them in OpenSearch Dashboards.
- [`victoria/`](victoria/README.md): Stores logs, metrics, and traces in VictoriaLogs, VictoriaMetrics, and VictoriaTraces, and displays them in Grafana.
  This backend is experimental and not yet fully verified.
- [`clickhouse/`](clickhouse/README.md): Stores logs, metrics, and traces in ClickStack and displays them in the ClickStack user interface (UI).
  This backend is experimental and not yet fully verified.

## 1. Start

After installation, copy the installed setup to a working directory.
If `./otel-collector-compose/` already exists, remove it first or choose a different destination.
In the shell command examples below, lines beginning with `#` are explanatory comments for the reader and are not executed by the shell.

```bash
# Copy the installed setup and enter the working copy.
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
cd ./otel-collector-compose
```

Start one backend stack from its backend directory:

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
OTLP means OpenTelemetry Protocol, and gRPC means Google remote procedure call.

Each backend directory is self-contained.
You can copy one backend directory and run the stack from the copied directory.

## 2. Stop

Stop the selected backend stack from its backend directory:

```bash
# Stop and remove the OpenSearch validation containers and network.
docker compose -f compose-opensearch.yaml down
```

For Podman, use the same Compose file with `podman compose`.

The `down` command stops and removes the local validation containers and network.
It does not delete bind-mounted backend data directories.
Starting the same backend again with the same data directories reuses the previous data.
Delete those directories only when you want to discard the stored backend data.
See the backend-specific README for the exact directory names.

## 3. Telemetry Endpoints

Choose the telemetry endpoint according to where the NestDAQ process runs.
The same rule applies to NestDAQ device processes and `daq-webctl`.

| Sender location | OpenSearch/Victoria endpoint | ClickStack endpoint |
| :-- | :-- | :-- |
| Host process using published ports | `localhost:4317` or `http://localhost:4318` | `localhost:4317` or `http://localhost:4318` |
| Container in the same Compose network | `otel-collector:4317` or `http://otel-collector:4318` | `clickstack:4317` or `http://clickstack:4318` |
| Container outside the Compose network using host-published ports | Docker: `host.docker.internal:4317`; Podman: `host.containers.internal:4317` | Docker: `host.docker.internal:4317`; Podman: `host.containers.internal:4317` |

For OTLP HTTP, use the signal-specific paths required by the telemetry client, such as `/v1/logs`, `/v1/metrics`, and `/v1/traces`.

## 4. Backend Details

See the backend-specific README:

- `opensearch/README.md`
- `victoria/README.md`
- `clickhouse/README.md`

All stacks use pinned image defaults.
You can override the images with environment variables documented in the backend-specific README.

On Security-Enhanced Linux (SELinux)-enabled systems, the Compose files apply the `:Z` label option to bind-mounted paths.
