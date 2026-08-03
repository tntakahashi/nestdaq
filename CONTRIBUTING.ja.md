# コントリビューションガイドライン

[English](CONTRIBUTING.md) | [日本語](CONTRIBUTING.ja.md)

このドキュメントでは、NestDAQへのコントリビューションにおける推奨事項と禁止事項を説明します。

<a id="forking-workflow"></a>
## フォークを使用した開発手順

- `main`ブランチには、NestDAQの最新リリース版が含まれます。
- `develop`ブランチには、NestDAQの最新開発版が含まれます。
- 開発を始める前に、upstreamの`spadi-alliance/nestdaq`リポジトリを自身のGitHub accountへforkしてください。
- 自身のforkをupstreamの`develop`ブランチと同期し、forkの`develop`を基点として、使用したい任意のbranch nameでlocal working treeを準備してください。既存のworking treeを使用する場合は`git switch`、別の作業directoryを作成する場合は`git worktree add`を使用します。local branch nameには`develop`も使用できます。
- 自身のfork内では作業ブランチを自由に作成できます。upstreamリポジトリには作業ブランチを作成しないでください。
- upstreamの`main`および`develop`ブランチは保護されており、直接pushできません。
- commitを自身のforkへpushしてください。push先にはworking branchまたはforkの`develop`を使用できます。
- 自身のfork内のそのbranchから、upstreamの`develop`ブランチを対象としてPull RequestまたはDraft Pull Requestを作成してください。
- upstreamの`main`ブランチへ変更を反映できるのは、権限を持つmaintainerだけです。upstreamの`main`を対象とするPull Requestはupstreamの`develop`ブランチから作成するものだけを許可し、forkやその他のbranchからupstreamの`main`へのPull Requestは受け付けません。
- 変更が最終レビューの準備段階にない場合でも、早期のフィードバックが有用であればDraft Pull Requestを使用してください。

upstream repositoryはNestDAQの基準となるrepositoryです。forkは自身のGitHub accountに
置くcopyであり、自身がpushするbranchを保持します。local PC上のcloneは、branchを
選択してfileの編集、build、checkを行うworking treeを保持します。

```mermaid
flowchart BT
  subgraph Upstream["Upstream repository<br/>spadi-alliance/nestdaq"]
    direction LR
    UpstreamDevelop["develop branch"]
    UpstreamMain["main branch"]
  end

  subgraph Fork["自身のGitHub fork<br/>your-account/nestdaq"]
    direction LR
    ForkDevelop["develop branch"]
    ForkWorking["PR source branch<br/>developまたはworking branch"]
  end

  subgraph Local["Local PC<br/>作業用clone"]
    direction LR
    LocalClone["自身のforkのclone"]
    LocalWorking["working tree<br/>developが基点のbranch"]
  end

  UpstreamDevelop -->|forkまたは同期| ForkDevelop
  ForkDevelop -->|git clone| LocalClone
  LocalClone -->|git switchまたはgit worktree add| LocalWorking
  LocalWorking -->|git push| ForkWorking
  ForkWorking -->|Pull Request| UpstreamDevelop
  UpstreamDevelop -->|権限を持つmaintainerのPull Request| UpstreamMain
```

例えば、自身のforkをcloneし、`git switch`を使用して既存のworking treeに別名の
branchを作成します。

```sh
git clone https://github.com/<your-account>/nestdaq.git
cd nestdaq

# forkのdevelopを基点として、別名のworking branchを使用する場合
git switch -c <working-branch> origin/develop
```

別の作業directoryを使用する場合は、`git worktree add`でbranchとworking treeを
作成します。

```sh
git worktree add -b <working-branch> ../nestdaq-<working-branch> origin/develop
```

forkの`develop`で直接作業する場合は、代わりに次を実行します。

```sh
git switch develop
git pull --ff-only origin develop
```

fileを編集してcommitした後、選択したbranchを自身のforkへpushします。branch nameには
上で選択した名前を指定し、`develop`も使用できます。

```sh
git add <changed-files>
git commit -m "<commit-message>"
git push -u origin <branch-name>
```

<a id="commits-and-pull-requests"></a>
## コミットとPull Request

- 関連のない複数の変更を1つの巨大なコミットにまとめることは避けてください。
- 個別にレビューできる変更は、意図ごとにコミットを分けてください。
- Pull Requestは、慎重にレビューできる規模に保ってください。
- 関連のない変更が多数蓄積するまで待たず、こまめにPull Requestを作成してください。

<a id="formatting"></a>
## フォーマット

- Pull Requestを作成する前、またはDraft Pull Requestをレビュー可能な状態に変更する前に、フォーマッターを適用してください。
- C/C++ファイルには`astyle`を適用してください。
- 変更で触れたファイルだけをフォーマットしてください。
- 関係のないファイルを再フォーマットしないでください。

<a id="static-analysis"></a>
## 静的解析

- Pull Requestを作成する前に`clang-tidy`を実行してください。
- リポジトリ内の .clang-tidy 設定を使用してください。
- Pull Request自体がclang-tidyの方針に関するものでない限り、プロジェクトのコードに追加のチェックを有効にしないでください。
- CMakeを介して`clang-tidy`を実行するには、`-DNESTDAQ_ENABLE_CLANG_TIDY=ON`を指定して構成してください。

<a id="code-style-and-naming"></a>
## コードスタイルと命名規則

<a id="c"></a>
### C++

- 4個の空白でインデントしてください。

<a id="naming"></a>
### 命名規則

- `PascalCase`と`UpperCamelCase`は同じ命名形式を意味します。
- `class`名およびtype名: `PascalCase` / `UpperCamelCase`。
- `namespace`名: `snake_case`。
- functionおよびmember function: lower camel case / `camelCase`を優先します。既存スタイルとの一貫性を保つ場合は、`PascalCase` / `UpperCamelCase`も許容します。
- variable: `snake_case`を優先します。既存スタイルとの一貫性を保つ場合は、lower camel case / `camelCase`も許容します。
- `using` aliasは命名規則の対象外です。局所的な可読性、外部libraryの規約、または一般的な短縮形に従って構いません。
- `public struct`のdata field: `snake_case`。
- `private`および`protected`の`class` data member: `fPascalCase`。
- `static` data member: `fg`で開始します(例: `fgPascalCase`)。
- `static` variable: `g`で開始します(例: `gPascalCase`)。
- constant: `k`で開始する`kPascalCase`、または`SCREAMING_SNAKE_CASE`を使用します。
- macro名: `SCREAMING_SNAKE_CASE`。
- enum constant: `kPascalCase`、`PascalCase` / `UpperCamelCase`、または`SCREAMING_SNAKE_CASE`。
- NestDAQ codeのbase `namespace`: `nestdaq`。

<a id="file-naming"></a>
### ファイル名

- 推奨するファイル拡張子: `.cpp`, `.hpp`。
- 許容するファイル拡張子: `.cxx`, `.h`, `.hh`, `.hxx`。
