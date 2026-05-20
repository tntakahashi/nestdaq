# Redis Stack Container Helpers

This directory contains small helper scripts for starting Redis Stack containers
for local NestDAQ validation. They publish ports on the host and do not enable
Redis authentication by default, so do not expose them on a public or shared
network. Use the RedisInsight-enabled Redis Stack image for development and
local inspection. Prefer Redis Stack Server for production deployments.

The scripts use pinned Redis Stack image tags instead of `latest`:

- `redis/redis-stack:7.4.0-v8` for development Redis Stack with RedisInsight.
- `redis/redis-stack-server:7.4.0-v8` for production-oriented Redis Stack Server only.

## Start Redis Stack with RedisInsight

Run:

```sh
./run-redis-stack.sh
```

Default endpoints:

- Redis: `localhost:6379`
- RedisInsight: `http://localhost:8001`

Redis server data is bind-mounted from `redis-stack-data` next to the script to
`/data` in the container. RedisInsight data is bind-mounted from
`redisinsight-data` to `/redisinsight`, so RedisInsight can create its internal
subdirectories under that mounted directory.

## Start Redis Stack Server Only

Run:

```sh
./run-redis-stack-server.sh
```

Default endpoint:

- Redis: `localhost:6379`

Data is bind-mounted from `redis-stack-server-data` next to the script to
`/data` in the container.

## Rerun Behavior

By default, each script removes any existing container with the configured
container name before starting a new one. This makes repeated invocations safe
after a previous terminal was interrupted or a same-name container was left
behind. Persistent Redis data remains in the configured bind-mounted data directory
or named volume.

Set `REDIS_CONTAINER_REPLACE=0` to make the script fail instead when a
same-name container already exists.

## SELinux

SELinux label options are only used with `REDIS_VOLUME_MODE=bind`. Bind mounts
use the `:Z` label option by default so the container can write to the data
directory on SELinux-enabled hosts. Set `REDIS_VOLUME_LABEL=z` when the same
data directory must be shared by multiple containers. Set `REDIS_VOLUME_LABEL=`
to omit the label option entirely. In `run-redis-stack.sh`, the same label
option is applied to both Redis and RedisInsight bind mounts.

## Directory Permissions

By default, the scripts create bind-mounted data directories as the host user
running the script and do not change directory permissions.
On rootless Podman, container root normally maps to the host user running the
container, so the created directories are usually writable without extra
permission changes.

SELinux labeling and Unix permissions are separate. The `:Z` mount label lets
the container access the directory on SELinux-enabled hosts, but it does not
fix uid/gid permission mismatches. Rootful containers may create files owned by
host root in the bind-mounted directories. If a bind-mounted directory is not
writable, adjust host-side ownership or permissions explicitly outside these
helper scripts.

## Named Volumes

Named volumes are optional. Use `REDIS_VOLUME_MODE=volume` when you want Docker
or Podman to manage Redis data outside the helper script directory.

Inspect volumes with:

```sh
docker volume ls
podman volume ls
```

Remove named volumes when you want to discard local Redis data:

```sh
docker volume rm nestdaq-redis-stack-data nestdaq-redis-stack-redisinsight
docker volume rm nestdaq-redis-stack-server-data
```

or:

```sh
podman volume rm nestdaq-redis-stack-data nestdaq-redis-stack-redisinsight
podman volume rm nestdaq-redis-stack-server-data
```

Use named volumes when you do not want Redis data directories next to the
helper scripts:

```sh
REDIS_VOLUME_MODE=volume ./run-redis-stack.sh
```

## Runtime Options

Both scripts use the directory containing the script as `THIS_SCRIPT_DIR`.
Bind-mount data directories are relative to that directory, so copied installed
scripts keep bind-mounted data next to the copied scripts.

| Variable | Default | Description |
| -------- | ------- | ----------- |
| `CONTAINER_RUNTIME` | `docker` | Container runtime command. Set to `podman` to use Podman. |
| `REDIS_CONTAINER_NAME` | `nestdaq-redis-stack` or `nestdaq-redis-stack-server` | Container name. |
| `REDIS_CONTAINER_REPLACE` | `1` | Remove an existing same-name container before starting. Set to `0` to fail instead. |
| `REDIS_IMAGE` | `redis/redis-stack:7.4.0-v8` or `redis/redis-stack-server:7.4.0-v8` | Container image. |
| `REDIS_PORT` | `6379` | Host port mapped to Redis port `6379`. |
| `REDIS_INSIGHT_PORT` | `8001` | Host port mapped to RedisInsight port `8001`; used only by `run-redis-stack.sh`. |
| `REDIS_VOLUME_MODE` | `bind` | Storage mode. Use `bind` for host bind mounts or `volume` for named volumes. |
| `REDIS_DATA_VOLUME` | Container-name-based volume | Named volume mounted to `/data`; used only in `volume` mode. |
| `REDIS_INSIGHT_VOLUME` | Container-name-based volume | Named volume mounted to `/redisinsight`; used only by `run-redis-stack.sh` in `volume` mode. |
| `REDIS_DATA_DIR` | Data directory next to the script | Host directory bind-mounted to `/data`; used only in `bind` mode. |
| `REDIS_INSIGHT_DATA_DIR` | `redisinsight-data` next to the script | Host directory bind-mounted to `/redisinsight`; used only by `run-redis-stack.sh` in `bind` mode. |
| `REDIS_VOLUME_LABEL` | `Z` | SELinux bind-mount label option; used only in `bind` mode. Use `z` for shared labeling or an empty value to disable. |
| `REDIS_ARGS` | empty | Extra Redis server arguments passed through the image `REDIS_ARGS` environment variable. |

Example:

```sh
REDIS_PORT=16379 \
REDIS_INSIGHT_PORT=18001 \
REDIS_ARGS="--requirepass nestdaq" \
./run-redis-stack.sh
```

For Podman:

```sh
CONTAINER_RUNTIME=podman ./run-redis-stack-server.sh
```

Stop the foreground container with Ctrl-C. The container is removed on exit, but
the bind-mounted data directory or named volume is kept.
