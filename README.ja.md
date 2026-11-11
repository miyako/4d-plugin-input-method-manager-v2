# Input Method Manager

[English](README.md) | 日本語

フォーカスのある4Dウインドウのインプットメソッド（IME / FEP）を制御するプラグインです。オン・オフの切り替え、ひらがな・カタカナ・半角カタカナ・英数入力の切り替え、フィールド単位での無効化、変換中の文字列の取得と確定・取消、インプットメソッドの一覧と選択、ユーザーが入力した読み（ふりがな）の取得、日本語テキストの読みの取得ができます。

Windowsでは、フォーカスのあるウインドウの入力コンテキストにIMM32（`imm.h`）を、テキスト入力プロセッサーのオン・オフと変換モード、およびインプットメソッドのプロファイルにText Services Framework（`msctf.h`）を使用します。macOSではText Input Sourcesと`NSTextInputContext`を使用します。

バージョン2は[4d-plugin-input-method-manager](https://github.com/miyako/4d-plugin-input-method-manager)を書き直したものです。v1の4つのコマンドとすべての定数は、名前・シンタックス・値がそのままです。

## 動作環境

- 4D v21.1以降（テストプロジェクトの`compatibilityVersion`）
- Windows 64-bit、またはmacOS 11以降（Appleシリコン・Intel）
- 日本語の入力モードには日本語インプットメソッド（Microsoft IME、Google日本語入力、ATOK、macOSの日本語入力）

## インストール

[Releases](../../releases)ページから最新のリリースをダウンロードします。

### macOS & Windows（共通のダウンロード）

1. リリースの`.zip`をダウンロードします
2. 展開して`input-method-manager.bundle`フォルダーを取り出します
3. `.bundle`を4Dアプリケーションの**Plugins**フォルダー（またはデータベースの**Plugins**フォルダー）にコピーします
4. 4Dを再起動します

### macOSのみ（公証済みDMG）

1. リリースの`.dmg`をダウンロードします
2. マウントして`.bundle`を**Plugins**フォルダーにコピーします
3. 4Dを再起動します

### v1からのアップグレード

プラグイン名は引き続き`Input Method Manager`です。v2を追加する前に、**Plugins**フォルダーから`Input Method Manager.bundle`（v1）を削除してください。同じ名前のプラグインが2つあると、4Dはどちらも読み込みません。

| | v1 | v2 |
|---|---|---|
| プラットフォーム | Windows | WindowsとmacOS |
| 対象ウインドウ | `IM Get mode`と`IM Set mode`は最前面のウインドウ | すべてのコマンドがキーボードフォーカスのあるウインドウ |
| `IM Set mode(CMODE_DIRECT_INPUT; ...)` | 常に`0`を返す | インプットメソッドをオフにできれば`1`を返す |
| プリエンプティブプロセス | スレッドセーフと宣言されていたが、ウインドウがないため`0`を返していた | ウインドウを操作するコマンドはスレッドセーフではありません。プリエンプティブなメソッドで使用するとコンパイラーが報告します。`IM Get phonetic`と`IM Convert kana`はスレッドセーフです |

## 概要

`IM Get phonetic`と`IM Convert kana`以外のコマンドは、キーボードフォーカスのあるウインドウを対象とします。4Dのフォームオブジェクトはシステムのウインドウではないため、設定は次の呼び出しまで続きます。特定の入力モードが必要なフィールドごとに、`On getting focus`イベントでコマンドを呼び出してください。

```4d
// 漢字の氏名フィールドのオブジェクトメソッド
If (Form event code=On getting focus)
	$result:=IM Set input mode(IM mode hiragana)
End if
```

v2で追加したコマンドはステータスを返します。成功は`0`（`IM error none`）、それ以外は[IM Error](#im-error)定数のいずれかです。失敗が4Dのエラーとして報告されることはありません。v1の4つのコマンドはv1と同じく、成功で`1`、失敗で`0`を返します。

### プラットフォームによる違い

| | Windows | macOS |
|---|---|---|
| 入力状態 | 入力コンテキストごと。IMM32と、アクティブなインプットメソッドがテキスト入力プロセッサーの場合はTSFのキーボードコンパートメントの両方に書き込みます。Windows 8以降は、言語設定で**アプリウィンドウごとに異なる入力方式を設定する**を選択していない限り、インプットメソッドはウインドウ間で共有されます | ユーザーごと。入力モードはそれぞれ入力ソースです。指定したモードの入力ソースを、選択中のインプットメソッドを優先して選択します |
| `IM Disassociate` | ウインドウから入力コンテキストを外します。インプットメソッドはオンにできなくなります | フィールドをローマ字の入力ソースに限定し（`allowedInputSourceLocales`）、直接入力を選択します |
| 文章モード（`SMODE_*`） | 読み書きできます | ありません。読むと`0`、書き込みは無視されます |
| `IM mode halfwidth alphanumeric` | インプットメソッドがオンで半角英数 | 直接入力と同じ（インプットメソッドの英字モード） |
| 読みの取得 | 対応 | 非対応（`IM error not supported`） |
| `IM Get phonetic` | `MSIME.Japan`（`IFELanguage`）。日本語Microsoft IMEとともにインストールされます | `CFStringTokenizer`。常に使用できます |

## コマンド

### `IM Associate`

```4d
$result:=IM Associate
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$result` | Longint | 成功は`1`、失敗またはフォーカスのあるウインドウがない場合は`0` |

`IM Disassociate`の後で、フォーカスのあるウインドウの既定の入力コンテキストを元に戻します。

### `IM Disassociate`

```4d
$result:=IM Disassociate
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$result` | Longint | 成功は`1`、失敗またはフォーカスのあるウインドウがない場合は`0` |

コードや数値だけを入力するフィールドのために、フォーカスのあるウインドウのインプットメソッドを無効にします。フィールドからフォーカスが外れたら`IM Associate`を呼び出してください。

### `IM Get mode`

```4d
$result:=IM Get mode($conversion; $sentence)
```

| 引数 | 型 | | 説明 |
|---|---|---|---|
| `$conversion` | Longint | ← | 変換モード。`CMODE_*`定数の和。インプットメソッドがオフの場合は`CMODE_DIRECT_INPUT`（`-1`） |
| `$sentence` | Longint | ← | 文章モード。`SMODE_*`定数の和 |
| `$result` | Longint | ← | 成功は`1`、失敗は`0` |

### `IM Set mode`

```4d
$result:=IM Set mode($conversion; $sentence)
```

| 引数 | 型 | | 説明 |
|---|---|---|---|
| `$conversion` | Longint | → | 変換モード。`CMODE_DIRECT_INPUT`でインプットメソッドをオフにします |
| `$sentence` | Longint | → | 文章モード |
| `$result` | Longint | ← | 成功は`1`、失敗は`0` |

ほかの変換モードではインプットメソッドがオンになります。v1の例はそのまま使えます。

```4d
// 半角英数
$result:=IM Set mode(CMODE_ROMAN; SMODE_AUTOMATIC)
// 全角ひらがな
$result:=IM Set mode(CMODE_ROMAN+CMODE_FULLSHAPE+CMODE_NATIVE; SMODE_AUTOMATIC)
```

日本語の通常の入力モードには`IM Set input mode`のほうが簡単です。ローマ字入力・かな入力などほかのビットは保持されます。

### `IM Get open status`

```4d
$status:=IM Get open status
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$status` | Longint | インプットメソッドがオンなら`1`、オフなら`0`、または負の[IM Error](#im-error) |

### `IM Set open status`

```4d
$result:=IM Set open status($open)
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$open` | Longint | `1`でインプットメソッドをオン（漢字キー）、`0`でオフ |
| `$result` | Longint | ステータス |

macOSでは、オンにすると日本語インプットメソッドのひらがなモードを選択します。

### `IM Get input mode`

```4d
$mode:=IM Get input mode
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$mode` | Longint | [IM Input Mode](#im-input-mode)定数のいずれか。フォーカスのあるウインドウがない場合や日本語以外のモードでは`IM mode unknown` |

### `IM Set input mode`

```4d
$result:=IM Set input mode($mode)
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$mode` | Longint | `IM mode unknown`以外の[IM Input Mode](#im-input-mode)定数 |
| `$result` | Longint | ステータス。それ以外の値では`IM error invalid argument` |

| モード | インプットメソッド | Windowsの変換モード |
|---|---|---|
| `IM mode direct input` | オフ | 変更しない |
| `IM mode hiragana` | オン | `CMODE_NATIVE+CMODE_FULLSHAPE` |
| `IM mode katakana` | オン | `CMODE_NATIVE+CMODE_KATAKANA+CMODE_FULLSHAPE` |
| `IM mode halfwidth katakana` | オン | `CMODE_NATIVE+CMODE_KATAKANA` |
| `IM mode fullwidth alphanumeric` | オン | `CMODE_FULLSHAPE` |
| `IM mode halfwidth alphanumeric` | オン（Windows）、直接入力（macOS） | `CMODE_ALPHANUMERIC` |

### `IM Get composition`

```4d
$composition:=IM Get composition
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$composition` | Object | フォーカスのあるウインドウで変換中の文字列 |

| プロパティ | 型 | 説明 |
|---|---|---|
| `status` | Number | ステータス |
| `active` | Boolean | 変換中は`True` |
| `text` | Text | 変換中の文字列 |
| `reading` | Text | インプットメソッドが提供する場合、変換中の文字列の読み（Windows） |
| `cursor` | Number | `text`内のキャレット位置 |

### `IM Complete composition`

```4d
$result:=IM Complete composition
```

変換中の文字列を、Enterキーを押したときと同じように確定します。レコードを保存するボタンなど、コードがフィールドを読み取る・保存する前に呼び出せば、インプットメソッドに残った文字列が失われません。ステータスを返します。変換中の文字列がない場合も`0`です。

### `IM Cancel composition`

```4d
$result:=IM Cancel composition
```

変換中の文字列を破棄します。ステータスを返します。

### `IM Get input methods`

```4d
$methods:=IM Get input methods
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$methods` | Collection | 有効なインプットメソッドとキーボードレイアウト |

各要素:

| プロパティ | 型 | 説明 |
|---|---|---|
| `id` | Text | `IM Set input method`に渡す識別子。Windows: インプットメソッドは`tip:0411:{clsid}:{profile}`、キーボードレイアウトは`hkl:04090409`。macOS: 入力ソースの識別子（例: `com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese`） |
| `name` | Text | 表示名 |
| `language` | Text | 言語タグ。例: `ja-JP`（Windows）、`ja`（macOS） |
| `type` | Text | `ime`（インプットメソッド）、`keyboard`（キーボードレイアウト）、`mode`（インプットメソッドの入力モード、macOS） |
| `selected` | Boolean | 現在のインプットメソッドなら`True` |

### `IM Get input method`

```4d
$method:=IM Get input method
```

現在のインプットメソッドを、上記のプロパティと`status`で返します。

### `IM Set input method`

```4d
$result:=IM Set input method($id)
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$id` | Text | `IM Get input methods`の`id` |
| `$result` | Longint | ステータス。この`id`の有効なインプットメソッドがない場合は`IM error not found` |

```4d
// ユーザーが使っている日本語インプットメソッドを選択する
var $methods : Collection
$methods:=IM Get input methods
$methods:=$methods.query("language = :1 and type # :2"; "ja@"; "keyboard")
If ($methods.length>0)
	$result:=IM Set input method($methods[0].id)
End if
```

### `IM Set reading capture`

```4d
$result:=IM Set reading capture($enable)
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$enable` | Longint | `1`で取得を開始（取得済みの読みをクリア）、`0`で停止 |
| `$result` | Longint | ステータス。macOSでは`IM error not supported` |

取得中は、ユーザーがインプットメソッドで確定した文字列ごとに読みを記録します。インプットメソッドが報告する読み、それがなければ変換前に入力したかなを使います。辞書では推測できない、ユーザーが実際に入力した読み（`しょうじ`と入力した`東海林`など）でふりがなフィールドを埋められます。後でユーザーが削除した文字列の読みは残るため、クリアするには取得を開始し直してください。

プラグインは4Dのウインドウが受け取るインプットメソッドのメッセージ（`WM_IME_COMPOSITION`）を読み取ります。ユーザーが使うインプットメソッドで確認してください。読みを送らず、かなを表示せずに変換するインプットメソッドでは、取得した読みに漢字が残ります。

```4d
// 氏名フィールドのオブジェクトメソッド
Case of
	: (Form event code=On getting focus)
		$result:=IM Set input mode(IM mode hiragana)
		$result:=IM Set reading capture(1)
	: (Form event code=On losing focus)
		[Customer]NameKana:=IM Get reading(IM kana katakana)
		$result:=IM Set reading capture(0)
End case
```

### `IM Get reading`

```4d
$reading:=IM Get reading($form)
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$form` | Longint | [IM Kana](#im-kana)定数のいずれか。省略時はひらがな |
| `$reading` | Text | 取得した読み。macOSや不明な形式では`""` |

### `IM Get phonetic`

```4d
$reading:=IM Get phonetic($text; $form)
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$text` | Text | 日本語のテキスト |
| `$form` | Longint | [IM Kana](#im-kana)定数のいずれか。省略時はひらがな |
| `$reading` | Text | 読み。読みのサービスがない場合は`""` |

読みは辞書によるため、人名などは本人の読みと異なる場合があります。ユーザーが入力するテキストには読みの取得を使用してください。英字・数字・かなはそのまま残ります。スレッドセーフです。

```4d
$reading:=IM Get phonetic("山田太郎"; IM kana katakana)  // ヤマダタロウ
```

### `IM Convert kana`

```4d
$result:=IM Convert kana($text; $form)
```

| 引数 | 型 | 説明 |
|---|---|---|
| `$text` | Text | 変換するテキスト |
| `$form` | Longint | [IM Kana](#im-kana)定数のいずれか。省略時はひらがな |
| `$result` | Text | 変換後のテキスト。不明な形式では`""` |

ひらがな・カタカナ・半角カタカナを相互に変換します。半角の濁音（`ｶﾞ`）は全角の1文字（`ガ`）に、またその逆に変換します。`IM kana halfwidth katakana`は、全銀フォーマットのデータが求めるとおり、全角のASCIIと全角スペースも半角にします。漢字などほかの文字はそのまま残ります。スレッドセーフです。

```4d
$result:=IM Convert kana("ヤマダ　タロウ"; IM kana halfwidth katakana)  // ﾔﾏﾀﾞ ﾀﾛｳ
```

4Dはかなの種類や全角・半角を区別せずにテキストを比較します（`"か"="カ"`は`True`）。変換結果を検証するには`Compare strings($a; $b; sk char codes)`を使用してください。

## 定数

### IME Conversion Mode

v1から変更ありません。値はWindowsの`IME_CMODE_*`フラグです。

| 定数 | 値 |
|---|---|
| `CMODE_DIRECT_INPUT` | -1 |
| `CMODE_ALPHANUMERIC` | 0 |
| `CMODE_NATIVE` | 1 |
| `CMODE_KATAKANA` | 2 |
| `CMODE_LANGUAGE` | 3 |
| `CMODE_FULLSHAPE` | 8 |
| `CMODE_ROMAN` | 16 |
| `CMODE_CHARCODE` | 32 |
| `CMODE_HANJACONVERT` | 64 |
| `CMODE_SOFTKBD` | 128 |
| `CMODE_NOCONVERSION` | 256 |
| `CMODE_EUDC` | 512 |
| `CMODE_SYMBOL` | 1024 |
| `CMODE_FIXED` | 2048 |

### IME Sentence Mode

v1から変更ありません。

| 定数 | 値 |
|---|---|
| `SMODE_NONE` | 0 |
| `SMODE_PLURALCLAUSE` | 1 |
| `SMODE_SINGLECONVERT` | 2 |
| `SMODE_AUTOMATIC` | 4 |
| `SMODE_PHRASEPREDICT` | 8 |
| `SMODE_CONVERSATION` | 16 |

### IM Input Mode

| 定数 | 値 |
|---|---|
| `IM mode unknown` | -1 |
| `IM mode direct input` | 0 |
| `IM mode hiragana` | 1 |
| `IM mode katakana` | 2 |
| `IM mode halfwidth katakana` | 3 |
| `IM mode fullwidth alphanumeric` | 4 |
| `IM mode halfwidth alphanumeric` | 5 |

### IM Kana

| 定数 | 値 |
|---|---|
| `IM kana hiragana` | 0 |
| `IM kana katakana` | 1 |
| `IM kana halfwidth katakana` | 2 |

### IM Error

| 定数 | 値 | 意味 |
|---|---|---|
| `IM error none` | 0 | 成功 |
| `IM error no window` | -1 | キーボードフォーカスのあるウインドウやテキストフィールドがない |
| `IM error no input method` | -2 | ウインドウに入力コンテキストがない（`IM Disassociate`の後など）、または該当するインプットメソッドがインストールされていない |
| `IM error not supported` | -3 | このプラットフォームまたはシステムでは使用できない |
| `IM error invalid argument` | -4 | 引数が範囲外または空 |
| `IM error not found` | -5 | この識別子の有効なインプットメソッドがない |
| `IM error failed` | -6 | システムまたはインプットメソッドが要求を拒否した |

## ソースからのビルド

### 必要なもの

- CMake 3.20以降
- Xcode（macOS）またはVisual Studio 2022以降（Windows）

### クローン

```bash
git clone --recurse-submodules https://github.com/miyako/4d-plugin-input-method-manager-v2.git
cd 4d-plugin-input-method-manager-v2
```

### ビルド（macOS）

```bash
cd input-method-manager
mkdir -p cmake-build && cd cmake-build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build .
```

### ビルド（Windows）

```pwsh
cd input-method-manager
mkdir cmake-build; cd cmake-build
cmake .. -A x64
cmake --build . --config Release
```

プラグインは`input-method-manager/input-method-manager-test/Plugins`に出力されます。

### テストの実行

[tool4d](https://developer.4d.com/docs/Admin/cli/)（無償、ライセンス不要）が必要です。

```bash
/path/to/tool4d --dataless --startup-method=test_all --project=$(pwd)/input-method-manager/input-method-manager-test/Project/input-method-manager.4DProject
```

tool4dにはウインドウがないため、テストの対象はかな変換、読み、インプットメソッドの一覧、およびフォーカスのあるウインドウがない場合の結果です。テストがマシンのインプットメソッドを変更することはありません。入力モードのコマンドは4Dアプリケーションのフォームで確認してください。

## CI/CD

このプロジェクトは、生成された正規の3ワークフロー構成のネイティブプラグインパイプラインを使用します。

| ワークフロー | トリガー | 目的 |
|---|---|---|
| `test.yml` | Push / PR / 手動 / 再利用 | 同一リビジョンの成果物のビルド、テスト、検証 |
| `bump-version.yml` | 手動、デフォルトブランチのみ | `VERSION`を更新してコミット。タグなし |
| `release.yml` | 手動、デフォルトブランチのみ。入力なし | テストを再利用し、署名、公証、検証、ドラフト作成、`vVERSION`の公開 |

### 必要なシークレット（`release.yml`のみ）

**Settings → Secrets and variables → Actions**で設定します。

| シークレット | 説明 |
|---|---|
| `APPLE_DEVELOPER_ID_CERTIFICATE` | 秘密鍵を含むDeveloper ID Applicationの`.p12`をBase64エンコードしたもの |
| `APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD` | `.p12`書き出し時のパスワード |
| `KEYCHAIN_PASSWORD` | CIランナーの一時キーチェーン用の独立したパスワード |
| `NOTARYTOOL_APPLE_ID` | 公証に使うApple IDのメールアドレス |
| `NOTARYTOOL_TEAM_ID` | Apple DeveloperのチームID（英数字10文字） |
| `NOTARYTOOL_PASSWORD` | Appleのアプリ用パスワード（アカウントのパスワードではありません） |

## ライセンス

[MIT](LICENSE)
