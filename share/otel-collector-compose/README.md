# OpenTelemetry Collector with OpenSearch

This directory contains a Compose setup for receiving OpenTelemetry data with
OpenTelemetry Collector, storing logs and traces in OpenSearch, and viewing them
with OpenSearch Dashboards.

## Components

- `otel-collector`: receives OTLP data over gRPC and HTTP.
- `opensearch`: stores logs and traces exported by the collector.
- `opensearch-dashboards`: provides the web UI for OpenSearch.

## Prerequisites

Create the external network before starting the stack:

```bash
docker network create otel-net
```

For Podman:

```bash
podman network create otel-net
```

## Start

Run Compose from this directory:

```bash
docker compose up
```

For Podman:

```bash
podman compose up
```

By default, the services are available on these host ports:

- OpenSearch: `http://localhost:9200`
- OpenSearch Dashboards: `http://localhost:5601`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

## Runtime Options

The host-side ports and OpenSearch data directory can be changed with
environment variables:

```bash
OPENSEARCH_PORT=19200 \
OPENSEARCH_DASHBOARDS_PORT=15601 \
OTEL_COLLECTOR_GRPC_PORT=14317 \
OTEL_COLLECTOR_HTTP_PORT=14318 \
OPENSEARCH_DATA_DIR=/path/to/opensearch-data \
docker compose up
```

If `OPENSEARCH_DATA_DIR` is not set, OpenSearch data is stored in
`./opensearch-data` relative to this directory. On SELinux-enabled systems, the
Compose file applies the `:Z` label option to the mounted paths.

## Collector Pipelines

The collector accepts OTLP data on both receivers:

- gRPC: `0.0.0.0:4317`
- HTTP: `0.0.0.0:4318`

Logs are exported to OpenSearch indices named:

```text
otel-logs-%{service.name}-yyyy.MM.dd
```

Traces are exported to OpenSearch indices named:

```text
otel-traces-%{service.name}-yyyy.MM.dd
```

If `service.name` is missing, `unknown-service` is used as the fallback.

## Stop

Stop the stack:

```bash
docker compose down
```

To remove data stored in the default bind-mounted directory:

```bash
rm -rf ./opensearch-data
```

