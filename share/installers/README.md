# Package Installer Helpers

This directory contains optional helper scripts for installing and updating
runtime services with the host package manager instead of using the local
Compose examples.

The scripts are intended for administrator-controlled hosts. They use `sudo`
unless they are run as root. They install packages into system-managed
locations such as `/usr`, `/etc`, package-manager repository directories, and
systemd unit directories, so root privileges are required.

On Debian and Ubuntu systems, the scripts use `apt-get`. On RHEL-family
systems such as AlmaLinux, Rocky Linux, RHEL, CentOS, and Fedora, the scripts
prefer `dnf` and fall back to `yum` when `dnf` is not available.

## Scripts

| Script | Installs or updates |
| :-- | :-- |
| `install-redis-stack.sh` | Redis Stack Server from the Redis package repository. |
| `install-otelcol-contrib.sh` | OpenTelemetry Collector Contrib from the upstream release package. |
| `install-opensearch.sh` | OpenSearch from the OpenSearch 3.x package repository. |
| `install-opensearch-dashboards.sh` | OpenSearch Dashboards from the OpenSearch 3.x package repository. |

## Usage

Run a script with one of these actions:

```sh
./install-redis-stack.sh install
./install-redis-stack.sh upgrade
./install-redis-stack.sh uninstall
./install-redis-stack.sh --help
```

`install` is the default action. `upgrade` uses the same package source and asks
the package manager to update the installed package. `uninstall` removes the
package with the host package manager.

The uninstall action is intentionally conservative: it does not delete package
repository files, service configuration, logs, Redis persistence files, or
OpenSearch data paths. Review those files manually before deleting them. If a
service is managed by `systemd`, stop and disable it before uninstalling the
package; see [systemd Management](#systemd-management).

Set `SUDO=` when running as root or when you want to provide your own privilege
wrapper:

```sh
SUDO=doas ./install-opensearch.sh install
```

## Redis Stack

The Redis script registers `packages.redis.io` and installs
`redis-stack-server` by default. This package is the server-oriented Redis
Stack package: it includes Redis server and Redis Stack modules, but it does
not include RedisInsight.

Use `REDIS_PACKAGE=redis-stack` only when the Redis repository for your
distribution provides that package and you want the RedisInsight-inclusive
Redis Stack package:

```sh
REDIS_PACKAGE=redis-stack ./install-redis-stack.sh install
```

Do not replace the default with plain `redis` or `redis-server` unless you
intentionally want Redis Open Source without Redis Stack modules.

Official install instructions:

- https://redis.io/docs/latest/operate/oss_and_stack/install/install-stack/

## OpenTelemetry Collector Contrib

The OpenTelemetry project publishes Linux packages with each GitHub release.
There is no apt or dnf repository configured by this helper. The script
downloads the selected release package and installs it through `apt` or `dnf`.

Use `OTELCOL_CONTRIB_VERSION` to choose a version. The default follows the
version used by the local Compose examples.

```sh
OTELCOL_CONTRIB_VERSION=0.155.0 ./install-otelcol-contrib.sh install
```

After installation, place or edit the collector configuration in the package's
configured location, commonly `/etc/otelcol-contrib/config.yaml`, before
starting the service.

Official install and release instructions:

- https://opentelemetry.io/docs/collector/install/
- https://github.com/open-telemetry/opentelemetry-collector-releases/releases

## OpenSearch

The OpenSearch scripts register the OpenSearch 3.x package repositories. By
default, `install-opensearch.sh` passes `DISABLE_INSTALL_DEMO_CONFIG=true` and
`DISABLE_SECURITY_PLUGIN=true` during installation so the package can be
installed without a demo admin password. Set `OPENSEARCH_INSTALL_SECURITY=demo`
and provide `OPENSEARCH_INITIAL_ADMIN_PASSWORD` if you want the package
installer to set up the demo security configuration.

```sh
OPENSEARCH_INSTALL_SECURITY=demo \
OPENSEARCH_INITIAL_ADMIN_PASSWORD='change-this-strong-password' \
./install-opensearch.sh install
```

These scripts install packages only. Review and edit service configuration
under `/etc/opensearch/` and `/etc/opensearch-dashboards/` before exposing the
services on a network.

Official install instructions:

- https://docs.opensearch.org/latest/install-and-configure/install-opensearch/rpm/
- https://docs.opensearch.org/latest/install-and-configure/install-opensearch/debian/
- https://docs.opensearch.org/latest/install-and-configure/install-dashboards/rpm/
- https://docs.opensearch.org/latest/install-and-configure/install-dashboards/debian/

## systemd Management

The package scripts install software only. Review service configuration before
enabling or starting services with `systemd`.

Common service commands:

```sh
sudo systemctl status <service>
sudo systemctl enable --now <service>
sudo systemctl restart <service>
sudo systemctl stop <service>
sudo systemctl disable <service>
```

Likely service names:

| Service | Unit name |
| :-- | :-- |
| Redis Stack Server | `redis-stack-server`; some distributions may use `redis-server` or `redis`. |
| OpenTelemetry Collector Contrib | `otelcol-contrib`. |
| OpenSearch | `opensearch`. |
| OpenSearch Dashboards | `opensearch-dashboards`. |

Examples:

```sh
sudo systemctl enable --now otelcol-contrib
sudo systemctl enable --now opensearch
sudo systemctl enable --now opensearch-dashboards
```

For Redis, check the unit name installed by your package first:

```sh
systemctl list-unit-files 'redis*'
sudo systemctl enable --now redis-stack-server
```

If you install the RedisInsight-inclusive `redis-stack` package, check the
installed unit names before enabling services. The Redis Stack container helper
`run-redis-stack.sh` also includes RedisInsight, but it is separate from these
host package installer scripts.

Before uninstalling a package that is managed by `systemd`, stop and disable
the service explicitly. The installer scripts' `uninstall` action only removes
the package with the host package manager; it does not run `systemctl`.

```sh
sudo systemctl stop <service>
sudo systemctl disable <service>
./install-xxx.sh uninstall
sudo systemctl daemon-reload
systemctl list-unit-files '<service-pattern>'
```

For Redis, confirm the installed unit name first because it can differ between
packages and distributions:

```sh
systemctl list-unit-files 'redis*'
sudo systemctl stop redis-stack-server
sudo systemctl disable redis-stack-server
./install-redis-stack.sh uninstall
sudo systemctl daemon-reload
systemctl list-unit-files 'redis*'
```
