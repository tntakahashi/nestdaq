# OpenTelemetry Collectorコンテナ構成

[English](README.md) | [日本語](README.ja.md)

[トップ: NestDAQ](../../README.ja.md) | [前へ: Redis container](../redis-stack-container/README.ja.md) | [次へ: OpenSearch設定](opensearch/README.ja.md)

この文書で**Compose**とは、Docker Compose (`docker compose`)またはPodman Compose (`podman compose`)を指します。
このディレクトリには、ローカル検証用のCompose構成が含まれています。
各構成はOpenTelemetry CollectorでOpenTelemetryデータを受信し、選択したバックエンドに保存します。

これらのスタックはローカル検証専用です。
ホスト上にサービスポートを公開し、構成によっては簡易なローカル認証情報を使用します。
公開ネットワークや共有ネットワークには公開しないでください。

デフォルトの`compose.yaml`または`docker-compose.yaml`はインストールされません。
バックエンドのディレクトリを明示的に1つ選択してください。

- [`opensearch/`](opensearch/README.ja.md): OpenSearchにログとトレースを保存し、OpenSearch Dashboardsで表示します。
- [`victoria/`](victoria/README.ja.md): VictoriaLogs、VictoriaMetrics、VictoriaTracesにログ、メトリクス、トレースを保存し、Grafanaで表示します。
  このバックエンドは実験的で、まだ十分に検証されていません。
- [`clickhouse/`](clickhouse/README.ja.md): ClickStackにログ、メトリクス、トレースを保存し、ClickStackユーザーインターフェース(UI)で表示します。
  このバックエンドは実験的で、まだ十分に検証されていません。

<a id="1-start"></a>
## 1. 起動

インストール後、インストール済みの構成を作業ディレクトリへコピーします。
`./otel-collector-compose`がすでに存在する場合は、先に削除するか別のコピー先を選択してください。
以下のshellコマンド例では、`#`で始まる行は読者向けの説明コメントであり、shellでは実行されません。

```bash
# インストール済みの構成をコピーし、作業用コピーへ移動します。
cp -a <install-prefix>/share/otel-collector-compose ./otel-collector-compose
cd ./otel-collector-compose
```

使用するバックエンドのディレクトリから、バックエンドスタックを1つ起動します。

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
OTLPはOpenTelemetry Protocol、gRPCはGoogle remote procedure call、HTTPはHypertext Transfer Protocolを意味します。

各バックエンドディレクトリは自己完結しています。
バックエンドディレクトリだけをコピーし、そのコピー先からスタックを実行できます。

<a id="2-stop"></a>
## 2. 停止

選択したバックエンドスタックを、そのバックエンドディレクトリから停止します。

```bash
# OpenSearchの検証用コンテナーとネットワークを停止して削除します。
docker compose -f compose-opensearch.yaml down
```

Podmanでは、同じComposeファイルを`podman compose`で使用してください。

`down`コマンドはローカル検証用のコンテナーとネットワークを停止して削除します。
bind mountされたバックエンドデータディレクトリは削除しません。
同じデータディレクトリを使用して同じバックエンドを再び起動すると、以前のデータが再利用されます。
保存されたバックエンドデータを破棄したい場合に限り、これらのディレクトリを削除してください。
正確なディレクトリ名は、各バックエンドのREADMEを参照してください。

<a id="3-telemetry-endpoints"></a>
## 3. テレメトリーエンドポイント

NestDAQプロセスを実行する場所に応じて、テレメトリーエンドポイントを選択してください。
同じ規則がNestDAQ deviceプロセスと`daq-webctl`の両方に適用されます。

| 送信元の場所 | OpenSearch/Victoriaエンドポイント | ClickStackエンドポイント |
| :-- | :-- | :-- |
| 公開ポートを使用するホストプロセス | `localhost:4317`または`http://localhost:4318` | `localhost:4317`または`http://localhost:4318` |
| 同じComposeネットワーク内のコンテナー | `otel-collector:4317`または`http://otel-collector:4318` | `clickstack:4317`または`http://clickstack:4318` |
| Composeネットワーク外からホストの公開ポートを使用するコンテナー | Docker: `host.docker.internal:4317`; Podman: `host.containers.internal:4317` | Docker: `host.docker.internal:4317`; Podman: `host.containers.internal:4317` |

OTLP HTTPでは、`/v1/logs`、`/v1/metrics`、`/v1/traces`など、テレメトリークライアントが必要とするsignal固有のパスを使用してください。

<a id="4-backend-details"></a>
## 4. バックエンドの詳細

各バックエンドのREADMEを参照してください。

- `opensearch/README.ja.md`
- `victoria/README.ja.md`
- `clickhouse/README.ja.md`

すべてのスタックは固定されたデフォルトのイメージを使用します。
バックエンドのREADMEに記載された環境変数でイメージを上書きできます。

Security-Enhanced Linux(SELinux)が有効なシステムでは、Composeファイルがbind mountされたパスに`:Z`ラベルオプションを適用します。
