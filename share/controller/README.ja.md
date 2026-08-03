# Webコントローラーのアセット

[English](README.md) | [日本語](README.ja.md)

このディレクトリには、NestDAQ Webコントローラー`daq-webctl`用にインストールされるブラウザーアセットが含まれています。コントローラーの実装と起動後の動作については、[`controller/README.md`](../../controller/README.ja.md)を参照してください。

<a id="1-daq-webctlhtml"></a>
## 1. `daq-webctl.html`

`daq-webctl.html`は、`daq-webctl`が提供するデフォルトのブラウザー用グラフィカルユーザーインターフェース(GUI)です。コントローラーのドキュメントルートに`daq-webctl.html`としてインストールされます。

インストール処理では、このファイルを指すシンボリックリンク`index.html`も作成されるため、ユーザーインターフェース(UI)は`/daq-webctl.html`または`/`のどちらからでも開けます。

起動コマンド、Redisの要件、コマンドラインオプション、およびブラウザー利用時の注意事項については、[`controller/README.md`](../../controller/README.ja.md)を参照してください。
