# OpenTelemetry Collectorコンテナー構成

[English](README.md) | [日本語](README.ja.md)

[トップ: NestDAQ](../../README.ja.md) | [前へ: Redisコンテナー](../redis-stack-container/README.ja.md) | [次へ: OpenSearch設定](opensearch/README.ja.md)

この文書で**Compose**とは、Docker Compose (`docker compose`) またはPodman Compose (`podman compose`) を指します。
このディレクトリには、ローカル検証用のCompose構成が含まれています。
各構成では、OpenTelemetry CollectorがOpenTelemetryデータを受信し、選択した保存先へ転送します。
この文書で**保存先構成**とは、OpenSearchなどのデータ保存先と、OpenSearch Dashboardsなどの表示ツールを組み合わせた構成を指します。

これらのスタックはローカル検証専用です。
ホスト上にサービスポートを公開し、構成によっては簡易なローカル認証情報を使用します。
公開ネットワークや共有ネットワークには公開しないでください。

使用する保存先構成のディレクトリを1つ選択してください。

- [`opensearch/`](opensearch/README.ja.md): OpenSearchにログとトレースを保存し、OpenSearch Dashboardsで表示します。
- [`victoria/`](victoria/README.ja.md): VictoriaLogs、VictoriaMetrics、VictoriaTracesにログ、メトリクス、トレースを保存し、Grafanaで表示します。
  この保存先構成は実験的で、まだ十分に検証されていません。
- [`clickhouse/`](clickhouse/README.ja.md): ClickStackにログ、メトリクス、トレースを保存し、ClickStackユーザーインターフェース (UI) で表示します。
  この保存先構成は実験的で、まだ十分に検証されていません。

<a id="1-start"></a>
## 1. 起動

インストール後、インストール済みの構成を作業ディレクトリへコピーします。
`./otel-collector-compose/`がすでに存在する場合は、先に削除するか別のコピー先を選択してください。
以下のシェルコマンド例では、`#`で始まる行は読者向けの説明コメントであり、シェルでは実行されません。

```bash
# インストール済みの構成をコピーし、作業用コピーへ移動します。
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
cd ./otel-collector-compose
```

使用する保存先構成のディレクトリから、Composeスタックを1つ起動します。

```bash
# OpenSearchディレクトリへ移動し、そのスタックを起動します。
cd opensearch
docker compose -f compose-opensearch.yaml up
```

```bash
# Victoriaディレクトリへ移動し、そのスタックを起動します。
cd victoria
docker compose -f compose-victoria.yaml up
```

```bash
# ClickHouseディレクトリへ移動し、そのスタックを起動します。
cd clickhouse
docker compose -f compose-clickhouse.yaml up
```

Podmanでは、同じファイルを`podman compose`で使用してください。

複数のスタックを同時に実行する場合は、`GRAFANA_PORT`、`CLICKSTACK_UI_PORT`、`OTEL_COLLECTOR_GRPC_PORT`、`OTEL_COLLECTOR_HTTP_PORT`など、競合するホストポートを上書きしてください。
OTLPはOpenTelemetry Protocolを意味します。
デフォルトでは、ポート`4317`がOTLP gRPC、ポート`4318`がOTLP HTTPです。

各保存先構成のディレクトリは自己完結しています。
使用するディレクトリだけをコピーし、そのコピー先からスタックを実行できます。

<a id="2-stop"></a>
## 2. 停止

選択したComposeスタックを、その保存先構成のディレクトリから停止します。

```bash
# OpenSearchの検証用コンテナーとネットワークを停止して削除します。
docker compose -f compose-opensearch.yaml down
```

Podmanでは、同じComposeファイルを`podman compose`で使用してください。

`down`コマンドはローカル検証用のコンテナーとネットワークを停止して削除します。
バインドマウントされたデータディレクトリは削除しません。
同じデータディレクトリを使用して同じ保存先構成を再び起動すると、以前のデータが再利用されます。
保存されたデータを破棄したい場合に限り、これらのディレクトリを削除してください。
正確なディレクトリ名は、各保存先構成のREADMEファイルを参照してください。

<a id="3-telemetry-endpoints"></a>
## 3. テレメトリーエンドポイント

NestDAQプロセスを実行する場所に応じて、テレメトリーエンドポイントを選択してください。
同じ規則がNestDAQデバイスプロセスと`daq-webctl`の両方に適用されます。

| 送信元の場所 | OpenSearch/Victoriaエンドポイント | ClickStackエンドポイント |
| :-- | :-- | :-- |
| 公開ポートを使用するホストプロセス | gRPC: `localhost:4317`、HTTP: `http://localhost:4318` | gRPC: `localhost:4317`、HTTP: `http://localhost:4318` |
| 同じComposeネットワーク内のコンテナー | gRPC: `otel-collector:4317`、HTTP: `http://otel-collector:4318` | gRPC: `clickstack:4317`、HTTP: `http://clickstack:4318` |
| Composeネットワーク外からホストの公開ポートを使用するコンテナー | DockerではgRPC: `host.docker.internal:4317`、HTTP: `http://host.docker.internal:4318`。PodmanではgRPC: `host.containers.internal:4317`、HTTP: `http://host.containers.internal:4318` | DockerではgRPC: `host.docker.internal:4317`、HTTP: `http://host.docker.internal:4318`。PodmanではgRPC: `host.containers.internal:4317`、HTTP: `http://host.containers.internal:4318` |

OTLP HTTPでは、`/v1/logs`、`/v1/metrics`、`/v1/traces`など、テレメトリークライアントが必要とするシグナル固有のパスを使用してください。

<a id="4-backend-details"></a>
## 4. 保存先構成の詳細

各保存先構成のREADMEファイルを参照してください。

- `opensearch/README.ja.md`
- `victoria/README.ja.md`
- `clickhouse/README.ja.md`

すべてのスタックは固定されたデフォルトのイメージを使用します。
各READMEファイルに記載された環境変数でイメージを上書きできます。

すべてのComposeファイルは、バインドマウントの指定に`:Z`ラベルオプションをあらかじめ含んでいます。
Security-Enhanced Linux (SELinux) が有効なシステムでは、DockerまたはPodmanが対象パスへコンテナー専用のSELinuxラベルを付け直します。
通常はComposeファイルを変更する必要はありません。
