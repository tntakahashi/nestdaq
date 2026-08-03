# OpenSearch OpenTelemetry（OTel）バックエンド

[English](README.md) | [日本語](README.ja.md)

このローカル検証用スタックは、OpenTelemetry CollectorでOpenTelemetryのログとトレースを受信し、OpenSearchに保存して、OpenSearch Dashboardsで表示します。

このディレクトリから起動します。

```bash
docker compose -f compose-opensearch.yaml up
```

Podmanの場合:

```bash
podman compose -f compose-opensearch.yaml up
```

`podman compose`を使用するには、`podman-compose`やDocker Compose pluginなどのCompose providerがインストールされ、`PATH`から見つけられる必要があります。

<a id="1-components"></a>
## 1. コンポーネント

- `otel-collector`: OpenTelemetry Protocol（OTLP）のログとトレースを、Google remote procedure call（gRPC）およびHypertext Transfer Protocol（HTTP）で受信します。
- `opensearch`: Collectorからexportされたログとトレースを保存します。
- `opensearch-dashboards`: OpenSearchのWebユーザーインターフェース（UI）を提供します。
- `opensearch-dashboards-setup`: ログとトレースの初期Data Viewが存在しない場合に作成します。

OpenSearch 3.xを含むOpenSearch 2.12以降では、同梱のdemo security設定をインストールする場合に`OPENSEARCH_INITIAL_ADMIN_PASSWORD`が必要です。このローカル検証用composeではdemo設定のinstallerとSecurity pluginを無効にしているため、このスタックではOpenSearchのadmin passwordは不要です。

スタックの起動後に`http://localhost:5601/app/discover`を開いてください。setup serviceは`otel-logs-*`と`otel-traces-*`のData Viewを作成し、default Data Viewがまだ設定されていない場合に限り`otel-logs-*`をdefaultに設定します。

<a id="2-collector-pipelines"></a>
## 2. Collectorパイプライン

Collectorは、次の名前のindexにログを保存します。

```text
otel-logs-%{service.name}-yyyy.MM.dd
```

トレースは次の名前のindexに保存します。

```text
otel-traces-%{service.name}-yyyy.MM.dd
```

`service.name`がない場合は`unknown-service`を使用します。OpenSearchのindex名には小文字が必要です。NestDAQ telemetryはexport前に`service.name`内のASCII大文字を小文字に変換します。このcompose構成を使用する外部OTLP clientも、`service.name`を小文字で送信してください。

<a id="3-ports"></a>
## 3. ポート

- OpenSearch: `http://localhost:9200`
- OpenSearch Dashboards: `http://localhost:5601`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

ホストプロセスは上記の`localhost` endpointを使用します。同じcompose network内のNestDAQ device containerまたは`daq-webctl` containerは、OTLP gRPCには`otel-collector:4317`を、OTLP HTTPには`http://otel-collector:4318`を使用してください。

<a id="4-rootless-podman"></a>
## 4. Rootless Podman

OpenSearchはcontainer内の`uid=1000,gid=1000`で実行されます。ここで`uid/gid`はuser identifier/group identifierを意味します。rootless Podmanでは、`/usr/share/opensearch/data`にbind mountするhost directoryが、Podman user namespaceから見たcontainerのuid/gidによって読み書きできる必要があります。

```bash
mkdir -p ./opensearch-data
podman unshare chown -R 1000:1000 ./opensearch-data
podman unshare chmod -R u+rwX ./opensearch-data
podman compose -f compose-opensearch.yaml up
```

代わりに、containerの`1000:1000` userをPodman Composeを起動するhost userに対応付けることもできます。

```bash
mkdir -p ./opensearch-data
PODMAN_USERNS="keep-id:uid=1000,gid=1000" \
podman compose --in-pod=false -f compose-opensearch.yaml up
```

`PODMAN_USERNS`の設定はuser namespace mappingを変更します。OpenSearch container processのuser IDは変更されず、container内では引き続き`uid=1000,gid=1000`です。

<a id="5-environment-variables"></a>
## 5. 環境変数

| 変数 | デフォルト | 説明 |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.155.0` | Collector image。 |
| `OPENSEARCH_IMAGE` | `docker.io/opensearchproject/opensearch:2.19.5` | OpenSearch image。 |
| `OPENSEARCH_DASHBOARDS_IMAGE` | `docker.io/opensearchproject/opensearch-dashboards:2.19.5` | OpenSearch Dashboards image。 |
| `OPENSEARCH_PORT` | `9200` | OpenSearchに割り当てるhost port。 |
| `OPENSEARCH_DASHBOARDS_PORT` | `5601` | OpenSearch Dashboardsに割り当てるhost port。 |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | OTLP gRPCに割り当てるhost port。 |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | OTLP HTTPに割り当てるhost port。 |
| `OPENSEARCH_DATA_DIR` | `./opensearch-data` | `/usr/share/opensearch/data`にbind mountするhost directory。 |
| `OTEL_COLLECTOR_CONFIG_FILE` | `./otel-collector-config-opensearch.yaml` | Collector config file。 |
| `OPENSEARCH_DASHBOARDS_CONFIG_FILE` | `./opensearch_dashboards.yaml` | OpenSearch Dashboards config file。 |
| `OPENSEARCH_DASHBOARDS_SETUP_SCRIPT` | `./opensearch-dashboards/setup-dashboards.js` | Dashboards初期設定script。 |

<a id="6-stop"></a>
## 6. 停止

ローカル検証用containerとnetworkを停止して削除します。

```bash
docker compose -f compose-opensearch.yaml down
```

Podmanの場合:

```bash
podman compose -f compose-opensearch.yaml down
```

OpenSearch data directoryは`down`では削除されません。デフォルトでは`./opensearch-data`が`/usr/share/opensearch/data`にbind mountされます。同じ`OPENSEARCH_DATA_DIR`でこのcompose構成を再び起動すると、OpenSearchは以前のデータを再利用します。

保存されたログ、トレース、index、OpenSearch metadataを破棄したい場合に限り、OpenSearch data directoryを削除してください。

```bash
rm -rf ./opensearch-data
```

rootless Podmanでは、file ownershipのためuser namespace経由で削除する必要がある場合があります。

```bash
podman unshare rm -rf ./opensearch-data
```
