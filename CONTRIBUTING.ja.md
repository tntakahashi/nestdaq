# コントリビューションガイドライン

[English](CONTRIBUTING.md) | [日本語](CONTRIBUTING.ja.md)

このドキュメントでは、NestDAQへのコントリビューションにおける推奨事項と禁止事項を説明します。

<a id="forking-workflow"></a>
## フォークを使用した開発手順

- `main`ブランチには、NestDAQの最新リリース版が含まれます。
- `develop`ブランチには、最新開発版が含まれます。開発作業は`develop`を基点にしてください。
- 保護された`main`および`develop`ブランチへの直接pushは許可されていません。
- `develop`から作業ブランチを作成し、そのブランチを自身のフォークへpushしてください。
- 作業ブランチから上流リポジトリの`develop`ブランチを対象として、Pull RequestまたはDraft Pull Requestを作成してください。
- 変更が最終レビューの準備段階にない場合でも、早期のフィードバックが有用であればDraft Pull Requestを使用してください。

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
- クラス名および型名: `PascalCase` / `UpperCamelCase`。
- 名前空間: `snake_case`。
- 関数およびメンバー関数: lower camel case / `camelCase`を優先します。既存スタイルとの一貫性を保つ場合は、`PascalCase` / `UpperCamelCase`も許容します。
- 変数: `snake_case`を優先します。既存スタイルとの一貫性を保つ場合は、lower camel case / `camelCase`も許容します。
- `using`による別名は命名規則の対象外です。局所的な可読性、外部ライブラリの規約、または一般的な短縮形に従って構いません。
- 公開`struct`のデータフィールド: `snake_case`。
- `private`および`protected`のクラスデータメンバー: `fPascalCase`。
- 静的データメンバー: `fg`で開始します（例: `fgPascalCase`）。
- 静的変数: `g`で開始します（例: `gPascalCase`）。
- 定数: `k`で開始する`kPascalCase`、または`SCREAMING_SNAKE_CASE`を使用します。
- マクロ名: `SCREAMING_SNAKE_CASE`。
- 列挙定数: `kPascalCase`、`PascalCase` / `UpperCamelCase`、または`SCREAMING_SNAKE_CASE`。
- NestDAQコードの基底名前空間: `nestdaq`。

<a id="file-naming"></a>
### ファイル名

- 推奨するファイル拡張子: `.cpp`, `.hpp`。
- 許容するファイル拡張子: `.cxx`, `.h`, `.hh`, `.hxx`。
