# テレメトリー

[English](README.md) | [日本語](README.ja.md)

[トップ: NestDAQ](../../README.ja.md) | [前へ: Web controller assets](../../share/controller/README.ja.md) | [次へ: Redis container](../../share/redis-stack-container/README.ja.md)

NestDAQテレメトリーは、FairMQベースのdeviceおよびcontroller process向けに、必要に応じて有効にできるOpenTelemetry統合です。
application executableはOpenTelemetryへ直接linkしません。
代わりに、NestDAQは単一のtelemetry plugin `libnestdaq_otel.so`を`dlopen()`で動的にloadし、小さなC application binary interface (ABI) を解決します。

pluginは3種類のOpenTelemetry signalをexportできます。

| Signal | デフォルト | NestDAQ内のsource |
| --- | --- | --- |
| Logs | `console` exporter | FairLogger custom sink、有効化した場合のspdlog sink |
| Metrics | 無効 | `nestdaq::telemetry::Telemetry` counter/histogram/gauge application programming interface (API) |
| Traces | 無効 | `nestdaq::telemetry::TelemetrySpan` resource acquisition is initialization (RAII) API |

> **注意:** NestDAQのOpenTelemetry metricsおよびtrace instrumentationはexperimentalです。
> production codeでは使用しないでください。spdlog log sinkもexperimentalであり、
> 詳細は後述します。

`libnestdaq_otel.so`は、CMake configure時に`opentelemetry-cpp`が見つかった場合にのみbuildおよびinstallされます。

<a id="1-telemetry-plugin-loading-model"></a>
## 1. テレメトリープラグインのloadモデル

NestDAQは、telemetry plugin内にprocess全体で共有するOpenTelemetry providerをinstallします。
process全体で共有するcustom sinkがFairLogger logを取得します。
NestDAQ spdlog sinkを明示的に接続したloggerだけがspdlog logをexportします。
metricsとtracesは、OpenTelemetry C++ headerを直接公開しないNestDAQの薄いwrapper APIを通じて記録されます。

動的loadされるtelemetry pluginは、public C ABIを`OpenTelemetryInitializer.cxx`で定義します。
内部実装はlogs、metrics、traces、共通telemetry helperというsignal領域別に構成されています。
applicationは内部実装fileへ依存せず、`TelemetryLibrary`、`Telemetry`、`Counter`、`Histogram`、`Gauge`、`TelemetrySpan`、`GetTelemetry()`を使用してください。

各signalはcomma区切りのprotocol listを受け取ります。
対応protocolは`console`、`otlp-http`、`otlp-grpc`です。
OTLPはOpenTelemetry Protocol、HTTPはHypertext Transfer Protocol、gRPCはGoogle remote procedure callの略です。
pluginはaliasの`http`、`otlp_http`、`grpc`、`otlp_grpc`も受け付けます。
空のprotocolはsignalを無効にします。

<a id="2-resource-attributes"></a>
## 2. リソース属性

logs、metrics、tracesは1つのOpenTelemetry resourceを共有します。
NestDAQは値を利用できる場合に、以下のresource attributeを設定します。
`service.*`と`host.*`はOpenTelemetry semantic convention attributeです。
以下ではOpenTelemetryの一般的な略称として`OTel`を使用します。
`nestdaq.*`と`fairmq.*`はNestDAQ固有のattributeです。

| Attribute | 由来 | 値 |
| --- | --- | --- |
| `service.name` | OTel semantic convention | 設定されたtelemetry service name。未設定時は`nestdaq`。 |
| `service.version` | OTel semantic convention | `NESTDAQ_VERSION`。 |
| `service.namespace` | OTel semantic convention | 設定されたtelemetry service namespace。 |
| `service.instance.id` | OTel semantic convention | 設定されたtelemetry service instance id。 |
| `host.name` | OTel semantic convention | telemetry option parse時に検出したhost name。 |
| `nestdaq.instance.id` | NestDAQ custom | 判明後のFairMQ device id。 |
| `nestdaq.instance.id.status` | NestDAQ custom | FairMQ device id判明前は`unresolved`、判明後は`resolved`。 |
| `fairmq.id` | NestDAQ/FairMQ custom | FairMQ device id。 |
| `fairmq.device` | NestDAQ/FairMQ custom | FairMQ device name。 |
| `fairmq.session` | NestDAQ/FairMQ custom | FairMQ session。 |
| `fairmq.transport` | NestDAQ/FairMQ custom | FairMQ transport。 |

詳細なNestDAQおよびFairMQのbuildとGit metadataは、resource attributeではなく構造化したstartup log bodyとして出力されます。
OpenTelemetry software development kit (SDK) は、独自のresource attributeを別途追加する場合があります。
この表はNestDAQが明示的に設定するattributeだけを示します。

<a id="3-fairlogger-log-records"></a>
## 3. FairLoggerログレコード

FairLogger custom sinkは、FairLogger severityが`--otel-log-severity`以上の場合、出力された各FairLogger messageをOpenTelemetry LogRecordへ変換します。

| LogRecord fieldまたはattribute | 由来 | Source |
| --- | --- | --- |
| Body | OTel LogRecord field | FairLogger message text。 |
| Timestamp | OTel LogRecord field | FairLogger `metadata.timestamp + metadata.us`。 |
| Observed timestamp | OTel LogRecord field | custom sinkがLogRecordを作成した時刻。 |
| SeverityNumber | OTel LogRecord field | FairLogger severityから対応付けたOpenTelemetry severity。 |
| SeverityText | OTel LogRecord field | 対応付けたseverityに対するOpenTelemetry定義のtext。 |
| `code.file.path` | OTel semantic convention | FairLogger source file metadata。 |
| `code.line.number` | OTel semantic convention | FairLogger source line metadata。 |
| `code.function.name` | OTel semantic convention | FairLogger function metadata。 |
| `thread.id` | OTel semantic convention | Linux native thread id。他platformではhash化したC++ thread id。 |
| `fairlogger.severity.number` | NestDAQ/FairLogger custom | 元のFairLogger severity number。 |
| `fairlogger.severity.text` | NestDAQ/FairLogger custom | 元のFairLogger severity name。 |
| `nestdaq.instance.id` | NestDAQ custom | FairMQ device id判明後にtelemetry loader経由で設定するrecord単位のinstance id。 |
| `nestdaq.instance.name` | NestDAQ custom | `-<number>`で終わるinstance idからparseしたprefix。 |
| `nestdaq.instance.index` | NestDAQ custom | `-<number>`で終わるinstance idからparseした数値suffix。 |
| `process.name` | NestDAQ/FairLogger custom | FairLogger process name metadata。OTelの`process.executable.name` resource attributeではありません。 |

instrumentation scopeはloggerおよびlibrary nameに`FairLogger`を使用し、library versionに`FAIRLOGGER_VERSION`を使用します。
`SeverityText`が標準OpenTelemetry LogRecord fieldであるため、NestDAQはcustom `log.severity.text` attributeを追加しません。

FairMQ throughput log lineは、log severity filterを適用する前にframework metrics用にparseされます。
そのため、元のlog messageがexport対象severity未満でも、throughput sampleがframework metricsを更新する場合があります。
metricsとtracesはresourceへ`nestdaq.instance.id`を含めるため、FairMQ device id判明後にのみ初期化されます。
logsはprocess起動時に`nestdaq.instance.id.status=unresolved`で初期化され、idを利用可能になると`nestdaq.instance.id.status=resolved`で再初期化されます。

<a id="4-spdlog-log-records"></a>
## 4. spdlogログレコード

spdlog OpenTelemetry sinkはexperimentalであり、まだ十分に検証されていません。

NestDAQのbuild時に`opentelemetry-cpp`とspdlogの両方を利用できる場合、`nestdaq/telemetry/SpdlogOpenTelemetrySink.h`がinstallされます。
spdlog instrumentationはFairLogger instrumentationから独立しています。
NestDAQはspdlogのdefault logger、registry、log levelを変更しません。
applicationは、OpenTelemetry recordをexportする各spdlog loggerへ返されたsinkを接続します。

```cpp
#include <nestdaq/telemetry/SpdlogOpenTelemetrySink.h>

#include <spdlog/spdlog.h>

auto logger = spdlog::logger{
    "sampler",
    {nestdaq::telemetry::CreateSpdlogOpenTelemetrySink()},
};
logger.info("event accepted");
```

`logger.info(...)`や`logger.warn(...)`などの通常のspdlog member functionは、source location metadataを自動では付加しません。
OpenTelemetry recordへfile path、line number、function nameを含める場合は、標準spdlog macroを使用します。

```cpp
SPDLOG_LOGGER_INFO(&logger, "accepted event {}", eventId);
SPDLOG_LOGGER_WARN(&logger, "queue depth is {}", depth);
```

default spdlog loggerでは、対応するdefault-logger macroを使用します。

```cpp
SPDLOG_INFO("accepted event {}", eventId);
SPDLOG_WARN("queue depth is {}", depth);
```

spdlog sinkは以下のOpenTelemetry fieldとattributeを記録します。

| LogRecord fieldまたはattribute | 由来 | Source |
| --- | --- | --- |
| Body | OTel LogRecord field | spdlog message payload。 |
| Timestamp | OTel LogRecord field | spdlog message timestamp。 |
| Observed timestamp | OTel LogRecord field | sinkがLogRecordを作成した時刻。 |
| SeverityNumber | OTel LogRecord field | spdlog levelから対応付けたOpenTelemetry severity。 |
| SeverityText | OTel LogRecord field | 対応付けたseverityに対するOpenTelemetry定義のtext。 |
| `code.file.path` | OTel semantic convention | 存在する場合のspdlog source file metadata。 |
| `code.line.number` | OTel semantic convention | 存在する場合のspdlog source line metadata。 |
| `code.function.name` | OTel semantic convention | 存在する場合のspdlog function metadata。 |
| `thread.id` | OTel semantic convention | spdlog thread id metadata。 |
| `spdlog.logger.name` | NestDAQ/spdlog custom | spdlog logger name。 |
| `spdlog.level` | NestDAQ/spdlog custom | 元のspdlog level text。 |

<a id="5-log-severity-mapping"></a>
## 5. Log severityの対応

OpenTelemetryは正規化したlog levelをLogRecordの`SeverityNumber`および`SeverityText` fieldへ保存します。
元のlogging library levelは、FairLogger recordでは`fairlogger.severity.*`、spdlog recordでは`spdlog.level`として別に保持されます。
logging libraryのenum整数はOpenTelemetry `SeverityNumber`値ではありません。
正規化したseverityのqueryにはOpenTelemetry fieldを使用してください。

`--otel-log-severity`はFairLogger sink filterです。
OpenTelemetry logsへexportするFairLoggerの最低severityを制御します。
有効化したspdlog sinkから出力されるrecordはfilterしません。
spdlogのfilteringは、引き続きspdlog loggerおよびsink levelで制御します。

<a id="51-fairlogger-severity-mapping"></a>
### 5.1. FairLogger severityの対応

| FairLogger level | `fair::Severity` int | OTel SeverityNumber | OTel SeverityText | 元のlevel attribute |
| --- | --- | --- | --- | --- |
| `nolog` | `0` | `0` | invalid / unspecified | `fairlogger.severity.*` |
| `trace` | `1` | `1` | `TRACE` | `fairlogger.severity.*` |
| `debug4` | `2` | `2` | `TRACE2` | `fairlogger.severity.*` |
| `debug3` | `3` | `2` | `TRACE2` | `fairlogger.severity.*` |
| `debug2` | `4` | `3` | `TRACE3` | `fairlogger.severity.*` |
| `debug1` | `5` | `4` | `TRACE4` | `fairlogger.severity.*` |
| `debug` | `6` | `5` | `DEBUG` | `fairlogger.severity.*` |
| `detail` | `7` | `6` | `DEBUG2` | `fairlogger.severity.*` |
| `info` | `8` | `9` | `INFO` | `fairlogger.severity.*` |
| `state` | `9` | `10` | `INFO2` | `fairlogger.severity.*` |
| `warn` | `10` | `13` | `WARN` | `fairlogger.severity.*` |
| `important` | `11` | `14` | `WARN2` | `fairlogger.severity.*` |
| `alarm` | `12` | `15` | `WARN3` | `fairlogger.severity.*` |
| `error` | `13` | `17` | `ERROR` | `fairlogger.severity.*` |
| `critical` | `14` | `18` | `ERROR2` | `fairlogger.severity.*` |
| `fatal` | `15` | `21` | `FATAL` | `fairlogger.severity.*` |

`warning`は`warn`の`--otel-log-severity` aliasとして使用できますが、FairLogger record自体はFairLogger level nameを使用します。
このaliasの`fair::Severity`値は`warn`と同じ`10`です。

<a id="52-spdlog-severity-mapping"></a>
### 5.2. spdlog severityの対応

| spdlog level | `spdlog::level::level_enum` int | OTel SeverityNumber | OTel SeverityText | 元のlevel attribute |
| --- | --- | --- | --- | --- |
| `trace` | `0` | `1` | `TRACE` | `spdlog.level` |
| `debug` | `1` | `5` | `DEBUG` | `spdlog.level` |
| `info` | `2` | `9` | `INFO` | `spdlog.level` |
| `warn` | `3` | `13` | `WARN` | `spdlog.level` |
| `err` | `4` | `17` | `ERROR` | `spdlog.level` |
| `critical` | `5` | `21` | `FATAL` | `spdlog.level` |
| `off` | `6` | `0` | invalid / unspecified | `spdlog.level` |
| `n_levels` | `7` | `0` | invalid / unspecified | `spdlog.level` |

<a id="6-command-line-options"></a>
## 6. コマンドラインオプション

| Option | 環境変数 | デフォルト | 意味 |
| --- | --- | --- | --- |
| `--otel-library` | `NESTDAQ_OTEL_LIBRARY` | `libnestdaq_otel.so` | `dlopen()`で読み込むshared library pathまたはsoname。 |
| `--otel-log-protocol` | `NESTDAQ_OTEL_LOG_PROTOCOL` | `console` | comma区切りのlog exporter。空ならlogsを無効化。 |
| `--otel-metric-protocol` | `NESTDAQ_OTEL_METRIC_PROTOCOL` | 空 | comma区切りのmetric exporter。空ならmetricsを無効化。 |
| `--otel-trace-protocol` | `NESTDAQ_OTEL_TRACE_PROTOCOL` | 空 | comma区切りのtrace exporter。空ならtracesを無効化。 |
| `--otel-log-endpoint-http` | `NESTDAQ_OTEL_LOG_ENDPOINT_HTTP` | `http://localhost:4318/v1/logs` | OTLP HTTP logs endpoint。 |
| `--otel-log-endpoint-grpc` | `NESTDAQ_OTEL_LOG_ENDPOINT_GRPC` | `localhost:4317` | OTLP gRPC logs endpoint。 |
| `--otel-metric-endpoint-http` | `NESTDAQ_OTEL_METRIC_ENDPOINT_HTTP` | `http://localhost:4318/v1/metrics` | OTLP HTTP metrics endpoint。 |
| `--otel-metric-endpoint-grpc` | `NESTDAQ_OTEL_METRIC_ENDPOINT_GRPC` | `localhost:4317` | OTLP gRPC metrics endpoint。 |
| `--otel-trace-endpoint-http` | `NESTDAQ_OTEL_TRACE_ENDPOINT_HTTP` | `http://localhost:4318/v1/traces` | OTLP HTTP traces endpoint。 |
| `--otel-trace-endpoint-grpc` | `NESTDAQ_OTEL_TRACE_ENDPOINT_GRPC` | `localhost:4317` | OTLP gRPC traces endpoint。 |
| `--otel-log-headers` | `NESTDAQ_OTEL_LOG_HEADERS` | 空 | comma区切りの`key=value` log exporter header。 |
| `--otel-metric-headers` | `NESTDAQ_OTEL_METRIC_HEADERS` | 空 | comma区切りの`key=value` metric exporter header。 |
| `--otel-trace-headers` | `NESTDAQ_OTEL_TRACE_HEADERS` | 空 | comma区切りの`key=value` trace exporter header。 |
| `--otel-log-severity` | `NESTDAQ_OTEL_LOG_SEVERITY` | `info` | exportするFairLoggerの最低severity。 |
| `--otel-log-required` | `NESTDAQ_OTEL_LOG_REQUIRED` | `false` | telemetryをloadまたは初期化できない場合に起動失敗とする。 |
| `--otel-timeout-ms` | なし | `5000` | force-flush、shutdown、exporter timeout (milliseconds)。 |
| `--spdlog-console-pattern` | `NESTDAQ_SPDLOG_CONSOLE_PATTERN` | `[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v` | spdlog native console sink pattern。 |
| `--spdlog-native-console` | `NESTDAQ_SPDLOG_NATIVE_CONSOLE` | `true` | OTel spdlog sinkとは独立してspdlog native console outputを有効化。 |
| `--spdlog-async` | `NESTDAQ_SPDLOG_ASYNC` | `false` | NestDAQ helper loggerに`spdlog::async_logger`を使用。 |
| `--spdlog-async-queue-size` | `NESTDAQ_SPDLOG_ASYNC_QUEUE_SIZE` | `8192` | async spdlog helper loggerのqueue size。 |
| `--spdlog-async-thread-count` | `NESTDAQ_SPDLOG_ASYNC_THREAD_COUNT` | `1` | async spdlog helper loggerのworker thread count。 |
| `--spdlog-async-overflow-policy` | `NESTDAQ_SPDLOG_ASYNC_OVERFLOW_POLICY` | `block` | queue overflow policy：`block`、`overrun_oldest`、`discard_new`。 |
| `--otel-metric-export-interval-ms` | なし | `1000` | 定期metric export interval (milliseconds)。 |
| `--otel-log-http-json` | なし | `true` | OTLP HTTP logsでJavaScript Object Notation (JSON) content typeを使用。 |
| `--otel-metric-http-json` | なし | `true` | OTLP HTTP metricsでJSON content typeを使用。 |
| `--otel-trace-http-json` | なし | `true` | OTLP HTTP tracesでJSON content typeを使用。 |
| `--otel-service-name` | なし | caller default | `service.name` resource attribute。FairMQ device wrapperは`--service-name`をdefaultとし、`--service-name`未設定時はexecutable basenameを使用します。collector pipelineがOpenSearch index nameにこの値を使用する場合があるため、NestDAQはASCII uppercase letterをlowercaseへ変換します。 |
| `--otel-service-namespace` | なし | `nestdaq` | `service.namespace` resource attribute。 |
| `--otel-service-instance-id` | なし | 生成したuniversally unique identifier (UUID) | `service.instance.id` resource attribute。FairMQ device wrapperは、このoption未設定時に`--uuid`を使用し、それ以外の場合はUUIDを生成します。 |
| `--otel-fairmq-id` | なし | 空 | `fairmq.id` resource attribute。 |
| `--otel-fairmq-device` | なし | 空 | `fairmq.device` resource attribute。 |
| `--otel-fairmq-session` | なし | 空 | `fairmq.session` resource attribute。 |
| `--otel-fairmq-transport` | なし | 空 | `fairmq.transport` resource attribute。 |

severity nameは`nolog`、`trace`、`debug4`、`debug3`、`debug2`、`debug1`、`debug`、`detail`、`info`、`state`、`warn`、`warning`、`important`、`alarm`、`error`、`critical`、`fatal`です。

<a id="7-examples"></a>
## 7. 使用例

デフォルト動作ではlogsをconsole exporterへexportし、metricsとtracesは無効です。

shell command例の中で`#`から始まる行は読者向けのcommentであり、shellでは実行されません。

```sh
# defaultのtelemetry exporterを使用してdeviceを起動します。
my-device
```

logs、metrics、tracesをOTLP HTTP collectorへ送信します。

```sh
# deviceのlogs、metrics、tracesをOTLP HTTPでcollectorへexportします。
my-device \
  --otel-log-protocol=otlp-http \
  --otel-metric-protocol=otlp-http \
  --otel-trace-protocol=otlp-http \
  --otel-log-endpoint-http=http://collector:4318/v1/logs \
  --otel-metric-endpoint-http=http://collector:4318/v1/metrics \
  --otel-trace-endpoint-http=http://collector:4318/v1/traces
```

protocol optionを値なしで渡してlogsを明示的に無効化します。

```sh
# log exportを明示的に無効化してdeviceを起動します。
my-device --otel-log-protocol
```

spdlog patternを設定し、custom patternのspdlog native console sinkを使用します。
native console sinkはデフォルトで有効で、OTel spdlog sinkと同時に動作できます。

```sh
# OTLP gRPCでlogをexportし、native console formatを変更します。
my-device \
  --otel-log-protocol=otlp-grpc \
  --spdlog-console-pattern '[%n] [%l] %v'
```

OTel spdlog exportを有効に保ったまま、native spdlog console outputだけを無効化します。

```sh
# OTLP gRPC log exportを維持し、native console outputを無効化します。
my-device \
  --otel-log-protocol=otlp-grpc \
  --spdlog-native-console=false
```

NestDAQ helper loggerはデフォルトで同期動作し、spdlogのmulti-thread-safe sinkを使用します。
logging frequencyが高く、caller threadからbackground workerへrecordを渡したい場合はasync modeを有効にします。

```sh
# helper loggerの処理を、上限付きqueueを使用する2つのbackground workerへ移します。
my-device \
  --spdlog-async=true \
  --spdlog-async-queue-size=16384 \
  --spdlog-async-thread-count=2 \
  --spdlog-async-overflow-policy=block
```

`block` overflow policyはlog recordの消失を防ぎますが、queue満杯時にcaller threadを待たせることがあります。
`overrun_oldest`はqueue内の古いrecordを破棄し、`discard_new`はqueue満杯時に新しく送信されたrecordを破棄します。
async queue sizeとworker countはasync helper logger作成時に適用されます。
後から設定を変更しても、既存loggerは変更されません。

telemetryを明示的に管理するapplicationからC++ thin APIを使用します。

```cpp
auto library = nestdaq::telemetry::TelemetryLibrary{};
if (!library.Load("libnestdaq_otel.so")) {
    std::cerr << library.GetLastError() << '\n';
}

auto options = nestdaq::telemetry::TelemetryOptions{};
options.log_protocol = "console";
options.metric_protocol = "otlp-http";

const auto config = nestdaq::telemetry::MakeConfig(options);
if (!library.InitializeWith(config)) {
    std::cerr << library.GetLastError() << '\n';
}

auto telemetry = nestdaq::telemetry::Telemetry{library};
telemetry.AddCounter("events.total", 1, "1", "Total processed events");
telemetry.RecordHistogram("event.size", 4096, "By", "Input event size");
telemetry.RecordGauge("queue.depth", 12, "{message}", "Latest queue depth");

auto events = telemetry.Counter("events.total", "1", "Total processed events");
events.Add(1, {{"channel", "data"}});

auto queueDepth = telemetry.Gauge("queue.depth", "{message}", "Latest queue depth");
queueDepth.Record(12, {{"channel", "data"}});

auto span = telemetry.StartSpan("process-event");
span.SetAttribute({
    .key = "component",
    .type = NESTDAQ_OTEL_ATTRIBUTE_STRING,
    .string_value = "sampler",
    .int_value = 0,
    .uint_value = 0,
    .double_value = 0.0,
    .bool_value = 0,
});
```

application向けに推奨する形式は、`events.Add(...)`、`queueDepth.Record(...)`、`StartSpan(..., { ... })`で使用する`Attribute` wrapperです。
このwrapperは、NestDAQがattributeをC ABI形式へ変換する間、string storageを有効に保ちます。
通常はexampleでもこの形式を使用してください。

高度なcodeでは、あらかじめ構築したC ABI attributeを直接渡せます。
temporary `Attribute` wrapper変換を避けられるため、hot pathや、すでに`nestdaq_otel_attribute` bufferを所有するcodeで有用です。

```cpp
std::array<nestdaq_otel_attribute, 2> attributes{{
    {
        .key = "channel",
        .type = NESTDAQ_OTEL_ATTRIBUTE_STRING,
        .string_value = "data",
        .int_value = 0,
        .uint_value = 0,
        .double_value = 0.0,
        .bool_value = 0,
    },
    {
        .key = "slot",
        .type = NESTDAQ_OTEL_ATTRIBUTE_UINT64,
        .string_value = "",
        .int_value = 0,
        .uint_value = 2,
        .double_value = 0.0,
        .bool_value = 0,
    },
}};

telemetry.AddCounter(
    "events.total", 1, "1", "Total processed events", attributes.data(), attributes.size());
```

low-level attribute arrayとそのstring storageはcallerが所有します。
NestDAQはtelemetry call中にのみarrayを読み取ります。
C++20 buildでは、同等のoverloadが`std::span<const nestdaq_otel_attribute>`も受け取り、同じlow-level implementationへforwardします。

<a id="8-collector-compose-setup"></a>
## 8. Collector Compose構成 (`docker compose`または`podman compose`)

OpenTelemetry Collector、OpenSearch、OpenSearch Dashboardsを使用するlocal環境については、[OpenTelemetry Collector Compose setup](../../share/otel-collector-compose/README.ja.md)を参照してください。

<a id="9-troubleshooting"></a>
## 9. トラブルシューティング

- `--otel-library`をloadできない場合は、`LD_LIBRARY_PATH`を確認するか、rpathを設定するか、absolute pathを指定してください。
- libraryをloadできても初期化に失敗する場合は、`TelemetryLibrary::GetLastError()`を確認してください。
- 未対応のprotocol name、不正なconfig size、不正なseverity value、空のmetric/span nameはpluginのlast-error stringを通じて報告されます。
- metric signalが無効の場合、metric記録は成功するno-opになります。
  trace signalが無効の場合、inactive spanを返します。
