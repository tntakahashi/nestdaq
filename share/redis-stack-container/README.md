# Redis Stack Container Helpers

[English](README.md) | [日本語](README.ja.md)

This directory contains small helper scripts for starting Redis Stack containers
for local NestDAQ validation. They publish ports on the host and do not enable
Redis authentication by default, so do not expose them on a public or shared
network. Use the RedisInsight-enabled Redis Stack image for development and
local inspection. Prefer Redis Stack Server for production deployments.

The scripts use pinned image tags instead of `latest`:

- `docker.io/redis/redis-stack:7.4.0-v8` for development Redis Stack with RedisInsight.
- `docker.io/redis/redis-stack-server:7.4.0-v8` for production-oriented Redis Stack Server only.
- `docker.io/library/redis:8.2.7` for the official Redis 8.2.7 image.
- `docker.io/redis/redis-stack:7.2.0-v20` for Redis Stack 7.2 with RedisInsight.
- `docker.io/redis/redis-stack-server:7.2.0-v20` for Redis Stack 7.2 Server only.

## 1. Choose an Image

| Script | Image | RedisInsight | Notes |
| :-- | :-- | :-- | :-- |
| `run-redis-8.2.7.sh` | `docker.io/library/redis:8.2.7` | no | Official Redis image. The Redis 8 package is expected to include Redis Stack modules; verify with `MODULE LIST` after startup. |
| `run-redis-7.2-stack.sh` | `docker.io/redis/redis-stack:7.2.0-v20` | yes | Redis Stack 7.2 image line for development and local inspection. |
| `run-redis-7.2-stack-server.sh` | `docker.io/redis/redis-stack-server:7.2.0-v20` | no | Redis Stack 7.2 server-only image line. |
| `run-redis-stack.sh` | `docker.io/redis/redis-stack:7.4.0-v8` | yes | Default Redis Stack development helper. |
| `run-redis-stack-server.sh` | `docker.io/redis/redis-stack-server:7.4.0-v8` | no | Default Redis Stack server-only helper. |

Check the running Redis version and loaded modules with:

```sh
redis-cli -p 6379 INFO server
redis-cli -p 6379 MODULE LIST
```

The Redis Stack 7.2 image tags are Stack release tags, not exact Redis server
patch-version tags. Use the commands above after startup when the precise Redis
server patch version matters.

## 2. Start Redis 8.2.7

Run:

```sh
./run-redis-8.2.7.sh
```

Default endpoint:

- Redis: `localhost:6379`

Data is bind-mounted from `redis-8.2.7-data` next to the script to `/data` in
the container. This helper uses the official Redis image, so extra Redis server
arguments in `REDIS_ARGS` are passed as container command arguments.

## 3. Start Redis Stack 7.2

Run Redis Stack with RedisInsight:

```sh
./run-redis-7.2-stack.sh
```

Run Redis Stack Server only:

```sh
./run-redis-7.2-stack-server.sh
```

Default endpoints:

- Redis: `localhost:6379`
- RedisInsight: `http://localhost:8001` when using `run-redis-7.2-stack.sh`

## 4. Start Redis Stack with RedisInsight

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

## 5. Start Redis Stack Server Only

Run:

```sh
./run-redis-stack-server.sh
```

Default endpoint:

- Redis: `localhost:6379`

Data is bind-mounted from `redis-stack-server-data` next to the script to
`/data` in the container.

## 6. Rerun Behavior

By default, each script removes any existing container with the configured
container name before starting a new one. This makes repeated invocations safe
after a previous terminal was interrupted or a same-name container was left
behind. Persistent Redis data remains in the configured bind-mounted data directory
or named volume.

Set `REDIS_CONTAINER_REPLACE=0` to make the script fail instead when a
same-name container already exists.

## 7. Security-Enhanced Linux (SELinux)

SELinux label options are only used with `REDIS_VOLUME_MODE=bind`. Bind mounts
use the `:Z` label option by default so the container can write to the data
directory on SELinux-enabled hosts. Set `REDIS_VOLUME_LABEL=z` when the same
data directory must be shared by multiple containers. Set `REDIS_VOLUME_LABEL=`
to omit the label option entirely. In RedisInsight-enabled helpers, the same
label option is applied to both Redis and RedisInsight bind mounts.

## 8. Directory Permissions

By default, the scripts create bind-mounted data directories as the host user
running the script and do not change directory permissions.
On rootless Podman, container root normally maps to the host user running the
container, so the created directories are usually writable without extra
permission changes.

SELinux labeling and Unix permissions are separate. The `:Z` mount label lets
the container access the directory on SELinux-enabled hosts, but it does not
fix user identifier/group identifier (uid/gid) permission mismatches. Rootful
containers may create files owned by host root in the bind-mounted directories.
If a bind-mounted directory is not writable, adjust host-side ownership or
permissions explicitly outside these helper scripts.

## 9. Named Volumes

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
docker volume rm nestdaq-redis-8.2.7-data
docker volume rm nestdaq-redis-7.2-stack-data nestdaq-redis-7.2-stack-redisinsight
docker volume rm nestdaq-redis-7.2-stack-server-data
```

or:

```sh
podman volume rm nestdaq-redis-stack-data nestdaq-redis-stack-redisinsight
podman volume rm nestdaq-redis-stack-server-data
podman volume rm nestdaq-redis-8.2.7-data
podman volume rm nestdaq-redis-7.2-stack-data nestdaq-redis-7.2-stack-redisinsight
podman volume rm nestdaq-redis-7.2-stack-server-data
```

Use named volumes when you do not want Redis data directories next to the
helper scripts:

```sh
REDIS_VOLUME_MODE=volume ./run-redis-stack.sh
```

<a id="10-runtime-options"></a>
## 10. Environment Variables

Both scripts use the directory containing the script as `THIS_SCRIPT_DIR`.
Bind-mount data directories are relative to that directory, so copied installed
scripts keep bind-mounted data next to the copied scripts.

| Variable | Default | Description |
| -------- | ------- | ----------- |
| `CONTAINER_RUNTIME` | `docker` | Container engine command. Set to `podman` to use Podman. |
| `REDIS_CONTAINER_NAME` | Script-specific name | Container name. |
| `REDIS_CONTAINER_REPLACE` | `1` | Remove an existing same-name container before starting. Set to `0` to fail instead. |
| `REDIS_IMAGE` | Script-specific pinned image | Container image. |
| `REDIS_PORT` | `6379` | Host port mapped to Redis port `6379`. |
| `REDIS_INSIGHT_PORT` | `8001` | Host port mapped to RedisInsight port `8001`; used only by RedisInsight-enabled helpers. |
| `REDIS_CONTAINER_RUN_FLAGS` | `--rm -it` | Flags passed to `docker run` or `podman run`. Use `-d --rm` for non-interactive validation. |
| `REDIS_VOLUME_MODE` | `bind` | Storage mode. Use `bind` for host bind mounts or `volume` for named volumes. |
| `REDIS_DATA_VOLUME` | Container-name-based volume | Named volume mounted to `/data`; used only in `volume` mode. |
| `REDIS_INSIGHT_VOLUME` | Container-name-based volume | Named volume mounted to `/redisinsight`; used only by RedisInsight-enabled helpers in `volume` mode. |
| `REDIS_DATA_DIR` | Data directory next to the script | Host directory bind-mounted to `/data`; used only in `bind` mode. |
| `REDIS_INSIGHT_DATA_DIR` | Script-specific RedisInsight data directory | Host directory bind-mounted to `/redisinsight`; used only by RedisInsight-enabled helpers in `bind` mode. |
| `REDIS_VOLUME_LABEL` | `Z` | SELinux bind-mount label option; used only in `bind` mode. Use `z` for shared labeling or an empty value to disable. |
| `REDIS_ARGS` | empty | Extra Redis server arguments. Redis Stack images receive this through the image `REDIS_ARGS` environment variable; the official Redis 8.2.7 helper passes it as command arguments. |
| `REDIS_ARGS_MODE` | `env` or `argv` | Argument passing mode used by `run-redis-stack-server.sh`. Use `env` for Redis Stack images and `argv` for official Redis images. |

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
