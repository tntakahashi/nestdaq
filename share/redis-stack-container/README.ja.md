# Redis Stackコンテナー補助スクリプト

[English](README.md) | [日本語](README.ja.md)

このディレクトリには、NestDAQのローカル検証用にRedis Stack containerを起動する小さなhelper scriptが含まれています。これらはhost上にportを公開し、デフォルトではRedis authenticationを有効にしないため、公開ネットワークや共有ネットワークには公開しないでください。開発やローカルでの確認にはRedisInsightを含むRedis Stack imageを使用してください。production deploymentではRedis Stack Serverを推奨します。

scriptは`latest`ではなく固定されたimage tagを使用します。

- 開発用Redis Stack（RedisInsightを含む）: `docker.io/redis/redis-stack:7.4.0-v8`
- production向けRedis Stack Serverのみ: `docker.io/redis/redis-stack-server:7.4.0-v8`
- 公式Redis 8.2.7 image: `docker.io/library/redis:8.2.7`
- RedisInsightを含むRedis Stack 7.2: `docker.io/redis/redis-stack:7.2.0-v20`
- Redis Stack 7.2 Serverのみ: `docker.io/redis/redis-stack-server:7.2.0-v20`

<a id="1-choose-an-image"></a>
## 1. イメージの選択

| スクリプト | image | RedisInsight | 備考 |
| :-- | :-- | :-- | :-- |
| `run-redis-8.2.7.sh` | `docker.io/library/redis:8.2.7` | なし | 公式Redis image。Redis 8 packageにはRedis Stack moduleが含まれる想定です。起動後に`MODULE LIST`で確認してください。 |
| `run-redis-7.2-stack.sh` | `docker.io/redis/redis-stack:7.2.0-v20` | あり | 開発およびローカルでの確認用のRedis Stack 7.2 image系列。 |
| `run-redis-7.2-stack-server.sh` | `docker.io/redis/redis-stack-server:7.2.0-v20` | なし | Redis Stack 7.2のserver-only image系列。 |
| `run-redis-stack.sh` | `docker.io/redis/redis-stack:7.4.0-v8` | あり | デフォルトのRedis Stack開発用helper。 |
| `run-redis-stack-server.sh` | `docker.io/redis/redis-stack-server:7.4.0-v8` | なし | デフォルトのRedis Stack server-only helper。 |

実行中のRedis versionと読み込まれたmoduleは、次のコマンドで確認できます。

```sh
redis-cli -p 6379 INFO server
redis-cli -p 6379 MODULE LIST
```

Redis Stack 7.2のimage tagはStack release tagであり、Redis serverの正確なpatch version tagではありません。Redis serverの正確なpatch versionが重要な場合は、起動後に上記のコマンドを使用してください。

<a id="2-start-redis-827"></a>
## 2. Redis 8.2.7の起動

実行:

```sh
./run-redis-8.2.7.sh
```

デフォルトのendpoint:

- Redis: `localhost:6379`

dataはscriptの隣にある`redis-8.2.7-data`からcontainer内の`/data`へbind mountされます。このhelperは公式Redis imageを使用するため、`REDIS_ARGS`に指定した追加のRedis server argumentはcontainer commandのargumentとして渡されます。

<a id="3-start-redis-stack-72"></a>
## 3. Redis Stack 7.2の起動

RedisInsightを含むRedis Stackを実行します。

```sh
./run-redis-7.2-stack.sh
```

Redis Stack Serverだけを実行します。

```sh
./run-redis-7.2-stack-server.sh
```

デフォルトのendpoint:

- Redis: `localhost:6379`
- RedisInsight: `run-redis-7.2-stack.sh`を使用する場合は`http://localhost:8001`

<a id="4-start-redis-stack-with-redisinsight"></a>
## 4. RedisInsightを含むRedis Stackの起動

実行:

```sh
./run-redis-stack.sh
```

デフォルトのendpoint:

- Redis: `localhost:6379`
- RedisInsight: `http://localhost:8001`

Redis server dataはscriptの隣にある`redis-stack-data`からcontainer内の`/data`へbind mountされます。RedisInsight dataは`redisinsight-data`から`/redisinsight`へbind mountされるため、RedisInsightはmountされたdirectory内に内部subdirectoryを作成できます。

<a id="5-start-redis-stack-server-only"></a>
## 5. Redis Stack Serverのみの起動

実行:

```sh
./run-redis-stack-server.sh
```

デフォルトのendpoint:

- Redis: `localhost:6379`

dataはscriptの隣にある`redis-stack-server-data`からcontainer内の`/data`へbind mountされます。

<a id="6-rerun-behavior"></a>
## 6. 再実行時の動作

デフォルトでは、各scriptは新しいcontainerを起動する前に、設定されたcontainer nameと同じ名前の既存containerを削除します。このため、以前のterminalが中断された場合や、同名のcontainerが残っていた場合でも安全に再実行できます。永続化されたRedis dataは、設定されたbind mount用data directoryまたはnamed volumeに残ります。

同名のcontainerがすでに存在する場合にscriptを失敗させるには、`REDIS_CONTAINER_REPLACE=0`を設定します。

<a id="7-security-enhanced-linux-selinux"></a>
## 7. Security-Enhanced Linux（SELinux）

SELinux label optionは`REDIS_VOLUME_MODE=bind`の場合に限り使用されます。SELinuxが有効なhostでcontainerがdata directoryへ書き込めるよう、bind mountではデフォルトで`:Z` label optionを使用します。同じdata directoryを複数のcontainerで共有する必要がある場合は、`REDIS_VOLUME_LABEL=z`を設定します。label optionを完全に省略するには、`REDIS_VOLUME_LABEL=`を設定します。RedisInsightを含むhelperでは、RedisとRedisInsightの両方のbind mountに同じlabel optionが適用されます。

<a id="8-directory-permissions"></a>
## 8. ディレクトリ権限

デフォルトでは、scriptを実行したhost userとしてbind mount用data directoryを作成し、directory permissionは変更しません。rootless Podmanでは通常、container rootがcontainerを実行するhost userに対応付けられるため、作成されたdirectoryは追加のpermission変更なしで書き込み可能です。

SELinux labelingとUnix permissionは別のものです。`:Z` mount labelはSELinuxが有効なhostでcontainerからdirectoryへのaccessを許可しますが、user identifier/group identifier（uid/gid）のpermission不一致は解消しません。rootful containerは、bind mountしたdirectoryにhost root所有のfileを作成する場合があります。bind mountしたdirectoryに書き込めない場合は、このhelper scriptの外部でhost側のownershipまたはpermissionを明示的に調整してください。

<a id="9-named-volumes"></a>
## 9. 名前付きボリューム

named volumeの使用は必須ではありません。helper scriptのdirectory外でDockerまたはPodmanにRedis dataを管理させる場合は、`REDIS_VOLUME_MODE=volume`を使用します。

volumeを確認します。

```sh
docker volume ls
podman volume ls
```

ローカルのRedis dataを破棄する場合はnamed volumeを削除します。

```sh
docker volume rm nestdaq-redis-stack-data nestdaq-redis-stack-redisinsight
docker volume rm nestdaq-redis-stack-server-data
docker volume rm nestdaq-redis-8.2.7-data
docker volume rm nestdaq-redis-7.2-stack-data nestdaq-redis-7.2-stack-redisinsight
docker volume rm nestdaq-redis-7.2-stack-server-data
```

または:

```sh
podman volume rm nestdaq-redis-stack-data nestdaq-redis-stack-redisinsight
podman volume rm nestdaq-redis-stack-server-data
podman volume rm nestdaq-redis-8.2.7-data
podman volume rm nestdaq-redis-7.2-stack-data nestdaq-redis-7.2-stack-redisinsight
podman volume rm nestdaq-redis-7.2-stack-server-data
```

helper scriptの隣にRedis data directoryを置かない場合は、named volumeを使用します。

```sh
REDIS_VOLUME_MODE=volume ./run-redis-stack.sh
```

<a id="10-environment-variables"></a>
## 10. 環境変数

すべてのscriptは、scriptが置かれているdirectoryを`THIS_SCRIPT_DIR`として使用します。bind mount用data directoryはこのdirectoryからの相対pathであるため、インストール済みscriptをコピーしても、bind mountされたdataはコピー先のscriptの隣に保持されます。

| 変数 | デフォルト | 説明 |
| -------- | ------- | ----------- |
| `CONTAINER_RUNTIME` | `docker` | container engineのcommand。Podmanを使用する場合は`podman`を設定します。 |
| `REDIS_CONTAINER_NAME` | script固有の名前 | Container name。 |
| `REDIS_CONTAINER_REPLACE` | `1` | 起動前に同名の既存containerを削除します。削除せずに失敗させる場合は`0`を設定します。 |
| `REDIS_IMAGE` | script固有の固定image | Container image。 |
| `REDIS_PORT` | `6379` | Redis port `6379`に割り当てるhost port。 |
| `REDIS_INSIGHT_PORT` | `8001` | RedisInsight port `8001`に割り当てるhost port。RedisInsightを含むhelperだけで使用します。 |
| `REDIS_CONTAINER_RUN_FLAGS` | `--rm -it` | `docker run`または`podman run`へ渡すflag。非対話的な検証には`-d --rm`を使用します。 |
| `REDIS_VOLUME_MODE` | `bind` | storage mode。host bind mountには`bind`、named volumeには`volume`を使用します。 |
| `REDIS_DATA_VOLUME` | container nameに基づくvolume | `/data`にmountするnamed volume。`volume` modeだけで使用します。 |
| `REDIS_INSIGHT_VOLUME` | container nameに基づくvolume | `/redisinsight`にmountするnamed volume。RedisInsightを含むhelperの`volume` modeだけで使用します。 |
| `REDIS_DATA_DIR` | scriptの隣のdata directory | `/data`にbind mountするhost directory。`bind` modeだけで使用します。 |
| `REDIS_INSIGHT_DATA_DIR` | script固有のRedisInsight data directory | `/redisinsight`にbind mountするhost directory。RedisInsightを含むhelperの`bind` modeだけで使用します。 |
| `REDIS_VOLUME_LABEL` | `Z` | SELinux bind mount label option。`bind` modeだけで使用します。共有labelingには`z`、無効にするには空の値を使用します。 |
| `REDIS_ARGS` | 空 | 追加のRedis server argument。Redis Stack imageではimageの`REDIS_ARGS` environment variableを通して渡され、公式Redis 8.2.7 helperではcommand argumentとして渡されます。 |
| `REDIS_ARGS_MODE` | `env`または`argv` | `run-redis-stack-server.sh`が使用するargumentの受け渡しmode。Redis Stack imageには`env`、公式Redis imageには`argv`を使用します。 |

例:

```sh
REDIS_PORT=16379 \
REDIS_INSIGHT_PORT=18001 \
REDIS_ARGS="--requirepass nestdaq" \
./run-redis-stack.sh
```

Podmanの場合:

```sh
CONTAINER_RUNTIME=podman ./run-redis-stack-server.sh
```

foreground containerはCtrl-Cで停止します。終了時にcontainerは削除されますが、bind mountしたdata directoryまたはnamed volumeは保持されます。
