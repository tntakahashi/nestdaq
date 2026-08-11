# OpenSearch OpenTelemetry (OTel) バックエンド

[English](README.md) | [日本語](README.ja.md)

[トップ: NestDAQ](../../../README.ja.md) | [前へ: データ保存先の選択](../README.ja.md) | [次の保存先候補: Victoria](../victoria/README.ja.md)

このローカル検証用スタックは、OpenTelemetry CollectorでOpenTelemetryのログとトレースを受信します。
受信したデータをOpenSearchに保存し、OpenSearch Dashboardsで表示します。

この文書で**Compose**とは、Docker Compose (`docker compose`) またはPodman Compose (`podman compose`) を指します。
どちらかを使用してこのスタックを管理します。

このディレクトリから起動します。
以下のshellコマンド例では、`#`で始まる行は読者向けの説明コメントであり、shellでは実行されません。

```bash
# Docker ComposeでOpenSearchの検証用スタックを起動します。
docker compose -f compose-opensearch.yaml up
```

Podmanの場合:

```bash
# Podman ComposeでOpenSearchの検証用スタックを起動します。
podman compose -f compose-opensearch.yaml up
```

`podman compose`を使用するには、`podman-compose`やDocker Compose pluginなどのCompose providerが必要です。
Compose providerをインストールし、`PATH`から検出できるようにしてください。

<a id="1-components"></a>
## 1. コンポーネント

- `otel-collector`: OpenTelemetry Protocol (OTLP) のログとトレースを、Google remote procedure call (gRPC) およびHTTPで受信します。
- `opensearch`: Collectorがexportしたログとトレースを保存します。
- `opensearch-dashboards`: OpenSearchのWebユーザーインターフェース (UI) を提供します。
- `opensearch-dashboards-setup`: ログとトレースの初期Data Viewが存在しない場合に作成します。

OpenSearch 3.xを含むOpenSearch 2.12以降では、同梱のdemo security設定をインストールする場合に`OPENSEARCH_INITIAL_ADMIN_PASSWORD`が必要です。
このローカル検証用Compose構成ではdemo設定のinstallerとSecurity pluginを無効にしているため、OpenSearchの管理者パスワードは不要です。

スタックの起動後に`http://localhost:5601/app/discover`を開いてください。
setup serviceは`otel-logs-*`と`otel-traces-*`のData Viewを作成します。
default Data Viewがまだ設定されていない場合に限り、`otel-logs-*`をdefaultに設定します。

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

`service.name`がない場合、Collectorは`unknown-service`を使用します。
OpenSearchのindex名には小文字が必要です。
NestDAQテレメトリーはexport前に`service.name`内のASCII大文字を小文字に変換します。
このCompose構成を使用する外部OTLPクライアントも、`service.name`を小文字で送信してください。

<a id="3-ports"></a>
## 3. ポート

- OpenSearch: `http://localhost:9200`
- OpenSearch Dashboards: `http://localhost:5601`
- OTLP gRPC receiver: `localhost:4317`
- OTLP HTTP receiver: `http://localhost:4318`

ホストプロセスは上記の`localhost`エンドポイントを使用します。
同じComposeネットワーク内のNestDAQ deviceコンテナーまたは`daq-webctl`コンテナーは、OTLP gRPCには`otel-collector:4317`を、OTLP HTTPには`http://otel-collector:4318`を使用してください。

<a id="4-rootless-podman"></a>
## 4. Rootless Podman

OpenSearchはコンテナー内の`uid=1000,gid=1000`で実行されます。
ここで`uid/gid`はuser identifier/group identifierを意味します。
rootless Podmanでは、`/usr/share/opensearch/data/`にbind mountするホストディレクトリが、Podmanのユーザー名前空間から見たコンテナーのuid/gidによって読み書きできる必要があります。

```bash
# Podmanのユーザー名前空間内で、コンテナーのユーザー用にデータディレクトリを準備します。
mkdir -p ./opensearch-data
podman unshare chown -R 1000:1000 ./opensearch-data
podman unshare chmod -R u+rwX ./opensearch-data
# 準備したデータディレクトリを使用してスタックを起動します。
podman compose -f compose-opensearch.yaml up
```

代わりに、コンテナーの`1000:1000`ユーザーをPodman Composeを起動するホストユーザーに対応付けることもできます。

```bash
# データディレクトリを作成し、明示的なユーザーマッピングでスタックを起動します。
mkdir -p ./opensearch-data
PODMAN_USERNS="keep-id:uid=1000,gid=1000" \
podman compose --in-pod=false -f compose-opensearch.yaml up
```

`PODMAN_USERNS`の設定はユーザー名前空間のマッピングを変更します。
OpenSearchコンテナープロセスのuser IDは変更されず、コンテナー内では引き続き`uid=1000,gid=1000`です。

<a id="5-environment-variables"></a>
## 5. 環境変数

| 変数 | デフォルト | 説明 |
| :-- | :-- | :-- |
| `OTEL_COLLECTOR_IMAGE` | `docker.io/otel/opentelemetry-collector-contrib:0.155.0` | Collectorイメージ。 |
| `OPENSEARCH_IMAGE` | `docker.io/opensearchproject/opensearch:2.19.5` | OpenSearchイメージ。 |
| `OPENSEARCH_DASHBOARDS_IMAGE` | `docker.io/opensearchproject/opensearch-dashboards:2.19.5` | OpenSearch Dashboardsイメージ。 |
| `OPENSEARCH_PORT` | `9200` | OpenSearchに割り当てるホストポート。 |
| `OPENSEARCH_DASHBOARDS_PORT` | `5601` | OpenSearch Dashboardsに割り当てるホストポート。 |
| `OTEL_COLLECTOR_GRPC_PORT` | `4317` | OTLP gRPCに割り当てるホストポート。 |
| `OTEL_COLLECTOR_HTTP_PORT` | `4318` | OTLP HTTPに割り当てるホストポート。 |
| `OPENSEARCH_DATA_DIR` | `./opensearch-data` | `/usr/share/opensearch/data/`にbind mountするホストディレクトリ。 |
| `OTEL_COLLECTOR_CONFIG_FILE` | `./otel-collector-config-opensearch.yaml` | Collector設定ファイル。 |
| `OPENSEARCH_DASHBOARDS_CONFIG_FILE` | `./opensearch_dashboards.yaml` | OpenSearch Dashboards設定ファイル。 |
| `OPENSEARCH_DASHBOARDS_SETUP_SCRIPT` | `./opensearch-dashboards/setup-dashboards.js` | Dashboards初期設定script。 |

<a id="6-stop"></a>
## 6. 停止

ローカル検証用コンテナーとネットワークを停止して削除します。

```bash
# Dockerの検証用コンテナーとネットワークを停止して削除します。
docker compose -f compose-opensearch.yaml down
```

Podmanの場合:

```bash
# Podmanの検証用コンテナーとネットワークを停止して削除します。
podman compose -f compose-opensearch.yaml down
```

`down`ではOpenSearchデータディレクトリを削除しません。
デフォルトでは`./opensearch-data/`が`/usr/share/opensearch/data/`にbind mountされます。
同じ`OPENSEARCH_DATA_DIR`でこのCompose構成を再び起動すると、OpenSearchは以前のデータを再利用します。

保存されたログ、トレース、index、OpenSearch metadataを破棄したい場合に限り、OpenSearchデータディレクトリを削除してください。

```bash
# 保存されたOpenSearchデータを完全に破棄します。
rm -rf ./opensearch-data
```

rootless Podmanでは、ファイル所有権のためユーザー名前空間経由で削除する必要がある場合があります。

```bash
# rootless Podmanのデータをユーザー名前空間経由で破棄します。
podman unshare rm -rf ./opensearch-data
```
