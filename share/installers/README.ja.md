# パッケージインストーラー補助スクリプト

[English](README.md) | [日本語](README.ja.md)

このディレクトリには、`docker compose`または`podman compose`で実行するローカルのCompose例を使用せず、hostのpackage managerで外部サービスをインストールおよび更新するときに使用できる補助スクリプトが含まれています。

これらのスクリプトは、管理者が管理するhostでの利用を想定しています。rootとして実行しない場合は`sudo`を使用します。packageは`/usr`、`/etc`、package managerのrepository directory、systemd unit directoryなど、systemが管理する場所にインストールされるため、root権限が必要です。

DebianおよびUbuntuでは`apt-get`を使用します。AlmaLinux、Rocky Linux、RHEL、CentOS、FedoraなどのRHEL系systemでは`dnf`を優先し、`dnf`が利用できない場合は`yum`を使用します。

<a id="1-scripts"></a>
## 1. スクリプト

| スクリプト | インストールまたは更新するもの |
| :-- | :-- |
| `install-redis-stack.sh` | Redis package repositoryからRedis serverとRedis Stack moduleをインストールします。 |
| `install-otelcol-contrib.sh` | upstream release packageからOpenTelemetry Collector Contribをインストールします。 |
| `install-opensearch.sh` | OpenSearch 2.x package repositoryからOpenSearchをインストールします。 |
| `install-opensearch-dashboards.sh` | OpenSearch 2.x package repositoryからOpenSearch Dashboardsをインストールします。 |

<a id="2-usage"></a>
## 2. 使用方法

次のいずれかのactionを指定してscriptを実行します。

```sh
./install-redis-stack.sh install
./install-redis-stack.sh upgrade
./install-redis-stack.sh uninstall
./install-redis-stack.sh --help
```

デフォルトのactionは`install`です。`upgrade`は同じpackage sourceを使用し、package managerにインストール済みpackageの更新を要求します。`uninstall`はhostのpackage managerでpackageを削除します。

uninstall actionは意図的に保守的な動作になっており、package repository file、service configuration、log、Redis persistence file、OpenSearch data pathを削除しません。これらを削除する前に、手動で内容を確認してください。serviceを`systemd`で管理している場合は、packageをuninstallする前にserviceを停止して無効化してください。<a href="#6-systemd-management">systemdによる管理</a>を参照してください。

rootとして実行する場合、または独自のprivilege wrapperを指定する場合は`SUDO=`を設定します。

```sh
SUDO=doas ./install-opensearch.sh install
```

<a id="3-redis"></a>
## 3. Redis

Redis scriptは`packages.redis.io`を登録し、デフォルトでRedis `8.2.7`をインストールします。Redis 8 packageでは、デフォルトのpackage nameは`redis`です。Redis serverとRedis Stack moduleがインストールされますが、RedisInsightは含まれません。Redis 8.2.7 packageには、次のようなmoduleが含まれます。

```text
/usr/lib/redis/modules/redisbloom.so
/usr/lib/redis/modules/redisearch.so
/usr/lib/redis/modules/redistimeseries.so
/usr/lib/redis/modules/rejson.so
```

package managerでRedis repositoryが現在公開している最新versionをインストールまたは更新する場合は、`REDIS_VERSION=latest`を使用します。

```sh
REDIS_VERSION=latest ./install-redis-stack.sh install
```

使用するdistributionのRedis repositoryがそのpackageを提供しており、RedisInsightを含むRedis Stack packageを使用する場合に限り、`REDIS_PACKAGE=redis-stack`を使用してください。

```sh
REDIS_PACKAGE=redis-stack ./install-redis-stack.sh install
```

デフォルトの`REDIS_PACKAGE=redis` packageでは、`REDIS_VERSION=8.2.7`によるversion固定を利用できます。DebianおよびUbuntuでversionを固定したinstallは、公式Redis APT package setに従い、`redis`、`redis-server`、`redis-sentinel`、`redis-tools`を同じpackage versionでインストールします。`redis-stack-server`や`redis-stack`などのlegacy package nameを使用する場合は、`REDIS_VERSION=latest`を設定してください。

デフォルトの`redis` packageはRedisInsightをインストールしません。RedisInsightが必要な場合は、個別のRedisInsight packageまたは[`../redis-stack-container/`](../redis-stack-container/README.ja.md)のRedis Stack container helperを使用してください。

Redisはdistribution codenameまたはRPM repositoryごとにpackageを公開しています。設定されたRedis repositoryが`REDIS_VERSION`を提供していない場合、installerは別のRedis versionをインストールせずに失敗します。

DebianおよびUbuntuでは、Redis公式APT repositoryがdistribution codenameごとにpackageを公開しています。Debian 12(`bookworm`)、Debian 13(`trixie`)、Ubuntu 22.04(`jammy`)、Ubuntu 24.04(`noble`)では、固定されたpackage setを使用してRedis `7.2.14`、`7.4.9`、`8.2.7`をインストールできます。現在、Ubuntu 26.04(`resolute`)にはこれらのversionが提供されておらず、`8.8.0`などの新しいRedis packageのみが利用できます。そのため、`7.2.14`、`7.4.9`、`8.2.7`を固定したinstallは失敗します。

AlmaLinux/RHEL系systemでは、対応するRocky Linux major version向けのRedis公式RPM repositoryを使用します。Redis公式Rocky Linux repositoryはRedis 7.x packageを提供していません。AlmaLinux 9のstandard AppStreamは`redis:7` moduleを通してRedis `7.2.14`を提供しますが、このinstallerはRedis公式repositoryを対象とするため、そのpackageを使用しません。AlmaLinux 8および9ではRedis公式RPM repositoryからRedis `8.2.7`をインストールできます。現在、AlmaLinux 10ではRedis `8.2.7`が提供されておらず、`8.8.0`などの新しいRedis packageのみが利用できます。そのため、デフォルトの`REDIS_VERSION=8.2.7`によるinstallはAlmaLinux 10で失敗します。

このinstallerはAlmaLinux AppStream moduleからRedisをインストールしません。RHEL系systemでは常にRedis公式RPM repositoryを設定し、distributionのRedis moduleを無効にして、package解決に`packages.redis.io`が使用されるようにします。次のAppStreamの行は参考情報です。

確認済みのRedis package提供状況:

| Distribution | Repository key | Redis 7.2.14 | Redis 7.4.9 | Redis 8.2.7 |
| --- | --- | --- | --- | --- |
| AlmaLinux 8 | `rockylinux8` RPM repo | なし | なし | あり |
| AlmaLinux 9 | `rockylinux9` RPM repo | なし | なし | あり |
| AlmaLinux 9 | AppStream `redis:7` module(このinstallerでは不使用) | あり | なし | なし |
| AlmaLinux 10 | `rockylinux10` RPM repo | なし | なし | なし(`8.8.0`を利用可能) |
| Debian 12 | `bookworm` APT repo | あり | あり | あり |
| Debian 13 | `trixie` APT repo | あり | あり | あり |
| Ubuntu 22.04 | `jammy` APT repo | あり | あり | あり |
| Ubuntu 24.04 | `noble` APT repo | あり | あり | あり |
| Ubuntu 26.04 | `resolute` APT repo | なし | なし | なし(`8.8.0`を利用可能) |

公式インストール手順:

- https://redis.io/docs/latest/operate/oss_and_stack/install/install-stack/apt/
- https://redis.io/docs/latest/operate/oss_and_stack/install/install-stack/rpm/

<a id="4-opentelemetry-collector-contrib"></a>
## 4. OpenTelemetry Collector Contrib

OpenTelemetry projectは、各GitHub releaseでLinux packageを公開しています。このhelperはaptまたはdnf repositoryを設定しません。選択したrelease packageをdownloadし、`apt`または`dnf`を通してインストールします。

versionの選択には`OTELCOL_CONTRIB_VERSION`を使用します。デフォルトはローカルCompose例で使用するversionに合わせています。

```sh
OTELCOL_CONTRIB_VERSION=0.155.0 ./install-otelcol-contrib.sh install
```

インストール後、serviceを起動する前に、packageで設定された場所(通常は`/etc/otelcol-contrib/config.yaml`)へCollector configurationを配置するか、既存の設定を編集してください。

公式のインストールおよびrelease手順:

- https://opentelemetry.io/docs/collector/install/
- https://github.com/open-telemetry/opentelemetry-collector-releases/releases

<a id="5-opensearch"></a>
## 5. OpenSearch

OpenSearch scriptはOpenSearch 2.x package repositoryを登録し、デフォルトでOpenSearch `2.19.5`とOpenSearch Dashboards `2.19.5`をインストールします。repositoryで現在公開されている最新versionをpackage managerでインストールまたは更新する場合は、`OPENSEARCH_VERSION=latest`または`OPENSEARCH_DASHBOARDS_VERSION=latest`を使用します。

デフォルトでは、`install-opensearch.sh`はインストール時に`DISABLE_INSTALL_DEMO_CONFIG=true`と`DISABLE_SECURITY_PLUGIN=true`を渡し、demo admin passwordなしでpackageをインストールできるようにします。package installerでdemo security configurationを設定する場合は、`OPENSEARCH_INSTALL_SECURITY=demo`を設定して`OPENSEARCH_INITIAL_ADMIN_PASSWORD`を指定してください。

```sh
OPENSEARCH_VERSION=2.19.5 ./install-opensearch.sh install
OPENSEARCH_DASHBOARDS_VERSION=2.19.5 ./install-opensearch-dashboards.sh install

OPENSEARCH_INSTALL_SECURITY=demo \
OPENSEARCH_INITIAL_ADMIN_PASSWORD='change-this-strong-password' \
./install-opensearch.sh install
```

これらのscriptはpackageのみをインストールします。serviceをnetworkに公開する前に、`/etc/opensearch/`および`/etc/opensearch-dashboards/`内のservice configurationを確認して編集してください。

公式インストール手順:

- https://docs.opensearch.org/latest/install-and-configure/install-opensearch/rpm/
- https://docs.opensearch.org/latest/install-and-configure/install-opensearch/debian/
- https://docs.opensearch.org/latest/install-and-configure/install-dashboards/rpm/
- https://docs.opensearch.org/latest/install-and-configure/install-dashboards/debian/

<a id="6-systemd-management"></a>
## 6. systemdによる管理

package scriptはsoftwareのインストールだけを行います。`systemd`でserviceを有効化または起動する前に、service configurationを確認してください。

一般的なservice command:

```sh
sudo systemctl status <service>
sudo systemctl enable --now <service>
sudo systemctl restart <service>
sudo systemctl stop <service>
sudo systemctl disable <service>
```

想定されるservice name:

| Service | Unit name |
| :-- | :-- |
| Redis | 通常は`redis-server`。legacy Redis Stack packageでは`redis-stack-server`の場合があります。 |
| OpenTelemetry Collector Contrib | `otelcol-contrib`。 |
| OpenSearch | `opensearch`。 |
| OpenSearch Dashboards | `opensearch-dashboards`。 |

例:

```sh
sudo systemctl enable --now otelcol-contrib
sudo systemctl enable --now opensearch
sudo systemctl enable --now opensearch-dashboards
```

Redisでは、まずpackageによってインストールされたunit nameを確認してください。

```sh
systemctl list-unit-files 'redis*'
sudo systemctl enable --now redis-server
```

RedisInsightを含む`redis-stack` packageをインストールする場合は、serviceを有効化する前にインストールされたunit nameを確認してください。Redis Stack container helperの`run-redis-stack.sh`にもRedisInsightが含まれますが、host package installer scriptとは別のものです。

`systemd`で管理されているpackageをuninstallする前に、serviceを明示的に停止して無効化してください。installer scriptの`uninstall` actionはhostのpackage managerでpackageを削除するだけで、`systemctl`は実行しません。

```sh
sudo systemctl stop <service>
sudo systemctl disable <service>
./install-xxx.sh uninstall
sudo systemctl daemon-reload
systemctl list-unit-files '<service-pattern>'
```

Redisではpackageやdistributionによってunit nameが異なる可能性があるため、最初にインストール済みのunit nameを確認してください。

```sh
systemctl list-unit-files 'redis*'
sudo systemctl stop redis-server
sudo systemctl disable redis-server
./install-redis-stack.sh uninstall
sudo systemctl daemon-reload
systemctl list-unit-files 'redis*'
```
