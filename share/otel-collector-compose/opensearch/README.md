# OpenSearch OTel Backend

This local validation stack receives OpenTelemetry logs and traces with
OpenTelemetry Collector, stores them in OpenSearch, and opens them in
OpenSearch Dashboards.

Start from this directory:

```bash
docker compose -f compose-opensearch.yaml up
```

For Podman:

```bash
podman compose -f compose-opensearch.yaml up
```

## Components

- `otel-collector`: receives OTLP logs and traces over gRPC and HTTP.
- `opensearch`: stores logs and traces exported by the collector.
- `opensearch-dashboards`: provides the web UI for OpenSearch.
- `opensearch-dashboards-setup`: creates initial Data Views for logs and
  traces if they do not already exist.

OpenSearch 2.12 and later, including OpenSearch 3.x, requires
`OPENSEARCH_INITIAL_ADMIN_PASSWORD` when the bundled demo security
configuration is installed. This local validation compose disables that demo
configuration installer and the Security plugin, so no OpenSearch admin
password is required for this stack.

Open `http://localhost:5601/app/discover` after the stack starts. The setup
service creates Data Views for `otel-logs-*` and `otel-traces-*`, and sets
`otel-logs-*` as the default only when no default Data View is already
configured.

## Collector Pipelines

The collector stores logs in indices named:

```text
otel-logs-%{service.name}-yyyy.MM.dd
```

It stores traces in indices named:

```text
otel-traces-%{service.name}-yyyy.MM.dd
```

If `service.name` is missing, `unknown-service` is used. OpenSearch requires
lowercase index names. NestDAQ telemetry lowercases ASCII uppercase letters in
`service.name` before export; external OTLP clients should also send lowercase
`service.name` values when using this compose setup.

## Ports

- OpenSearch: `http://localhost:9200`
- OpenSearch Dashboards: `http://localhost:5601`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

## Rootless Podman

OpenSearch runs as container `uid=1000,gid=1000`. With rootless Podman, the
host directory bind-mounted to `/usr/share/opensearch/data` must be readable
and writable by that container uid/gid as seen from the Podman user namespace:

```bash
mkdir -p ./opensearch-data
podman unshare chown -R 1000:1000 ./opensearch-data
podman unshare chmod -R u+rwX ./opensearch-data
podman compose -f compose-opensearch.yaml up
```

Alternatively, map the container's `1000:1000` user to the host user that
starts Podman Compose:

```bash
mkdir -p ./opensearch-data
PODMAN_USERNS="keep-id:uid=1000,gid=1000" \
podman compose --in-pod=false -f compose-opensearch.yaml up
```

The `PODMAN_USERNS` setting changes the user namespace mapping. It does not
change the OpenSearch image's runtime user, which remains container
`uid=1000,gid=1000`.

## Runtime Options

| Variable | Default | Description |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.150.1` | Collector image. |
| `OPENSEARCH_IMAGE` | `docker.io/opensearchproject/opensearch:3.6.0` | OpenSearch image. |
| `OPENSEARCH_DASHBOARDS_IMAGE` | `docker.io/opensearchproject/opensearch-dashboards:3.6.0` | OpenSearch Dashboards image. |
| `OPENSEARCH_PORT` | `9200` | Host port mapped to OpenSearch. |
| `OPENSEARCH_DASHBOARDS_PORT` | `5601` | Host port mapped to OpenSearch Dashboards. |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | Host port mapped to OTLP gRPC. |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | Host port mapped to OTLP HTTP. |
| `OPENSEARCH_DATA_DIR` | `./opensearch-data` | Host directory bind-mounted to `/usr/share/opensearch/data`. |
| `OTEL_COLLECTOR_CONFIG_FILE` | `./otel-collector-config-opensearch.yaml` | Collector config file. |
| `OPENSEARCH_DASHBOARDS_CONFIG_FILE` | `./opensearch_dashboards.yaml` | OpenSearch Dashboards config file. |
| `OPENSEARCH_DASHBOARDS_SETUP_SCRIPT` | `./opensearch-dashboards/setup-dashboards.js` | Initial Dashboards setup script. |

## Stop

```bash
docker compose -f compose-opensearch.yaml down
rm -rf ./opensearch-data
```

For rootless Podman:

```bash
podman compose -f compose-opensearch.yaml down
podman unshare rm -rf ./opensearch-data
```
