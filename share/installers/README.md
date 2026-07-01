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
| `install-redis-stack.sh` | Redis server and Redis Stack modules from the Redis package repository. |
| `install-otelcol-contrib.sh` | OpenTelemetry Collector Contrib from the upstream release package. |
| `install-opensearch.sh` | OpenSearch from the OpenSearch 2.x package repository. |
| `install-opensearch-dashboards.sh` | OpenSearch Dashboards from the OpenSearch 2.x package repository. |

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

## Redis

The Redis script registers `packages.redis.io` and installs Redis `8.2.7` by
default. For Redis 8 packages, the default package name is `redis`. It installs
Redis server and Redis Stack modules, but it does not include RedisInsight.
The Redis 8.2.7 package includes modules such as:

```text
/usr/lib/redis/modules/redisbloom.so
/usr/lib/redis/modules/redisearch.so
/usr/lib/redis/modules/redistimeseries.so
/usr/lib/redis/modules/rejson.so
```

Use `REDIS_VERSION=latest` when you want the package manager to install or
upgrade to the latest version currently published by the Redis repository:

```sh
REDIS_VERSION=latest ./install-redis-stack.sh install
```

Use `REDIS_PACKAGE=redis-stack` only when the Redis repository for your
distribution provides that package and you want the RedisInsight-inclusive
Redis Stack package:

```sh
REDIS_PACKAGE=redis-stack ./install-redis-stack.sh install
```

Version pinning with `REDIS_VERSION=8.2.7` is supported for the default
`REDIS_PACKAGE=redis` package. On Debian and Ubuntu, pinned installs follow the
official Redis APT package set and install `redis`, `redis-server`,
`redis-sentinel`, and `redis-tools` with the same package version. Set
`REDIS_VERSION=latest` when using a legacy package name such as
`redis-stack-server` or `redis-stack`.

RedisInsight is not installed by the default `redis` package. Use a separate
RedisInsight package or the Redis Stack container helper in
[`../redis-stack-container/`](../redis-stack-container/README.md) when
RedisInsight is needed.

Redis publishes packages per distribution codename or RPM repository. If the
configured Redis repository does not publish `REDIS_VERSION`, the installer
fails before installing a different Redis version.

For Debian and Ubuntu systems, the Redis official APT repository publishes
packages per distribution codename. Debian 12 (`bookworm`), Debian 13
(`trixie`), Ubuntu 22.04 (`jammy`), and Ubuntu 24.04 (`noble`) can install
Redis `7.2.14`, `7.4.9`, and `8.2.7` with the pinned package set. Ubuntu 26.04
(`resolute`) currently does not provide those versions; only newer Redis
packages such as `8.8.0` are available, so pinned installs for `7.2.14`,
`7.4.9`, and `8.2.7` fail there.

For AlmaLinux/RHEL-family systems, the installer uses the Redis official RPM
repository for the matching Rocky Linux major version. The Redis official
Rocky Linux repositories do not provide Redis 7.x packages. AlmaLinux 9
standard AppStream provides Redis `7.2.14` through the `redis:7` module, but
that package is not used by this installer because the installer targets the
Redis official repository. AlmaLinux 8 and 9 can install Redis `8.2.7` from the
Redis official RPM repository. AlmaLinux 10 currently does not provide Redis
`8.2.7` there; only newer Redis packages such as `8.8.0` are available, so the
default `REDIS_VERSION=8.2.7` install fails on AlmaLinux 10.

This installer does not install Redis from AlmaLinux AppStream modules. On
RHEL-family systems it always configures the Redis official RPM repository and
disables the distribution Redis module so package resolution uses
`packages.redis.io`. The AppStream row below is informational only.

Verified Redis package availability:

| Distribution | Repository key | Redis 7.2.14 | Redis 7.4.9 | Redis 8.2.7 |
| --- | --- | --- | --- | --- |
| AlmaLinux 8 | `rockylinux8` RPM repo | No | No | Yes |
| AlmaLinux 9 | `rockylinux9` RPM repo | No | No | Yes |
| AlmaLinux 9 | AppStream `redis:7` module (not used by this installer) | Yes | No | No |
| AlmaLinux 10 | `rockylinux10` RPM repo | No | No | No (`8.8.0` available) |
| Debian 12 | `bookworm` APT repo | Yes | Yes | Yes |
| Debian 13 | `trixie` APT repo | Yes | Yes | Yes |
| Ubuntu 22.04 | `jammy` APT repo | Yes | Yes | Yes |
| Ubuntu 24.04 | `noble` APT repo | Yes | Yes | Yes |
| Ubuntu 26.04 | `resolute` APT repo | No | No | No (`8.8.0` available) |

Official install instructions:

- https://redis.io/docs/latest/operate/oss_and_stack/install/install-stack/apt/
- https://redis.io/docs/latest/operate/oss_and_stack/install/install-stack/rpm/

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

The OpenSearch scripts register the OpenSearch 2.x package repositories and
install OpenSearch `2.19.5` and OpenSearch Dashboards `2.19.5` by default. Use
`OPENSEARCH_VERSION=latest` or `OPENSEARCH_DASHBOARDS_VERSION=latest` when you
want the package manager to install or upgrade to the latest version currently
published by the repository.

By default, `install-opensearch.sh` passes
`DISABLE_INSTALL_DEMO_CONFIG=true` and `DISABLE_SECURITY_PLUGIN=true` during
installation so the package can be installed without a demo admin password. Set
`OPENSEARCH_INSTALL_SECURITY=demo` and provide
`OPENSEARCH_INITIAL_ADMIN_PASSWORD` if you want the package installer to set up
the demo security configuration.

```sh
OPENSEARCH_VERSION=2.19.5 ./install-opensearch.sh install
OPENSEARCH_DASHBOARDS_VERSION=2.19.5 ./install-opensearch-dashboards.sh install

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
| Redis | commonly `redis-server`; legacy Redis Stack packages may use `redis-stack-server`. |
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
sudo systemctl enable --now redis-server
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
sudo systemctl stop redis-server
sudo systemctl disable redis-server
./install-redis-stack.sh uninstall
sudo systemctl daemon-reload
systemctl list-unit-files 'redis*'
```
