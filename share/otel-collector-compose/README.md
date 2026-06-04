# OpenTelemetry Collector Compose Setups

This directory contains local validation Compose setups for receiving
OpenTelemetry data with OpenTelemetry Collector and storing it in one selected
backend.

These stacks are intended for local validation only. They publish service ports
on the host, use simple local credentials where applicable, and should not be
exposed on a public or shared network.

No default `compose.yaml` or `docker-compose.yaml` is installed. Choose one
backend directory explicitly:

- `opensearch/`: logs and traces in OpenSearch, viewed with OpenSearch
  Dashboards.
- `victoria/`: logs, metrics, and traces in VictoriaLogs, VictoriaMetrics, and
  VictoriaTraces, viewed with Grafana.
- `clickhouse/`: logs, metrics, and traces in ClickHouse, viewed with Grafana.

## Start

After installation, copy the installed setup to a working directory. If
`./otel-collector-compose` already exists, remove it first or choose a
different destination.

```bash
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
cd ./otel-collector-compose
```

Start one backend stack from its backend directory:

```bash
cd opensearch
docker compose -f compose-opensearch.yaml up
```

```bash
cd victoria
docker compose -f compose-victoria.yaml up
```

```bash
cd clickhouse
docker compose -f compose-clickhouse.yaml up
```

For Podman, use the same files with `podman compose`.

If you run multiple stacks at the same time, override conflicting host ports
such as `GRAFANA_PORT`, `OTEL_COLLECTOR_GRPC_PORT`, and
`OTEL_COLLECTOR_HTTP_PORT`.

Each backend directory is self-contained. It can be copied on its own and run
from that copied directory.

## Backend Details

See the backend-local README:

- `opensearch/README.md`
- `victoria/README.md`
- `clickhouse/README.md`

All stacks use pinned image defaults and allow overriding them with environment
variables documented in the backend README.

On SELinux-enabled systems, the Compose files apply the `:Z` label option to
bind-mounted paths.
