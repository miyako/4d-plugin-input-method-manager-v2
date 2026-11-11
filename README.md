# Input Method Manager

English | [日本語](README.ja.md)

Controls the input method (IME / FEP) of the focused 4D window: turn it on or off, switch between hiragana, katakana, half-width katakana and alphanumeric input, disable it for a field, read or end the text being composed, list and select input methods, capture the reading (furigana) the user types, and get the phonetic reading of Japanese text.

On Windows the plugin uses IMM32 (`imm.h`) for the input context of the focused window and the Text Services Framework (`msctf.h`) for the open/close and conversion state of text input processors and for input method profiles. On macOS it uses Text Input Sources and `NSTextInputContext`.

Version 2 is a rewrite of [4d-plugin-input-method-manager](https://github.com/miyako/4d-plugin-input-method-manager). The four v1 commands and all v1 constants keep their names, syntax and values.

## Requirements

- 4D v21.1 or later (the `compatibilityVersion` of the test project)
- Windows 64-bit, or macOS 11 or later (Apple silicon and Intel)
- For the Japanese input modes: a Japanese input method (Microsoft IME, Google Japanese Input, ATOK, or the macOS Japanese input method)

## Installation

Download the latest release from the [Releases](../../releases) page.

### macOS & Windows (single download)

1. Download the `.zip` from the release
2. Extract to get the `input-method-manager.bundle` folder
3. Copy the `.bundle` into your 4D application's **Plugins** folder (or your database's **Plugins** folder)
4. Restart 4D

### macOS only (notarized DMG)

1. Download the `.dmg` from the release
2. Mount it and copy the `.bundle` into your **Plugins** folder
3. Restart 4D

### Upgrading from v1

The plugin is still named `Input Method Manager`. Remove `Input Method Manager.bundle` (v1) from the **Plugins** folder before you add v2: 4D loads neither plugin when two have the same name.

| | v1 | v2 |
|---|---|---|
| Platforms | Windows | Windows and macOS |
| Window | `IM Get mode` and `IM Set mode` used the foreground window | Every command uses the window with keyboard focus |
| `IM Set mode(CMODE_DIRECT_INPUT; ...)` | Always returned `0` | Returns `1` when the input method was turned off |
| Preemptive processes | Declared thread-safe, but returned `0`: a preemptive process has no window | Commands that act on the window are not thread-safe; the compiler reports their use in preemptive methods. `IM Get phonetic` and `IM Convert kana` are thread-safe |

## Concepts

All commands except `IM Get phonetic` and `IM Convert kana` act on the window that has keyboard focus. A 4D form object is not a separate system window, so a setting stays until the next call: call the command in the `On getting focus` event of each field that needs a specific input mode.

```4d
// Object method of a field for a name in kanji
If (Form event code=On getting focus)
	$result:=IM Set input mode(IM mode hiragana)
End if
```

The commands added in v2 return a status: `0` (`IM error none`) on success, otherwise one of the [IM Error](#im-error) constants. Failure is never reported as a 4D error. The four v1 commands keep their v1 results: `1` on success and `0` on failure.

### Platform behaviour

| | Windows | macOS |
|---|---|---|
| Input state | Per input context. The plugin writes both IMM32 and, when the active input method is a text input processor, the TSF keyboard compartments. Windows 8 and later share the input method across windows unless **Let me set a different input method for each app window** is selected in the language settings | Per user. Each input mode is an input source; the plugin selects the input source of the requested mode, preferring the input method that is already selected |
| `IM Disassociate` | Removes the input context from the window: the input method cannot be turned on | Limits the field to Roman input sources (`allowedInputSourceLocales`) and selects direct input |
| Sentence mode (`SMODE_*`) | Read and written | Not available: read as `0`, ignored when written |
| `IM mode halfwidth alphanumeric` | Input method on, half-width alphanumeric | Same as direct input (the input method's Roman mode) |
| Reading capture | Supported | Not available (`IM error not supported`) |
| `IM Get phonetic` | `MSIME.Japan` (`IFELanguage`), installed with the Japanese Microsoft IME | `CFStringTokenizer`, always available |

## Commands

### `IM Associate`

```4d
$result:=IM Associate
```

| Parameter | Type | Description |
|---|---|---|
| `$result` | Longint | `1` on success, `0` on failure or when no window has focus |

Restores the default input context of the focused window, after `IM Disassociate`.

### `IM Disassociate`

```4d
$result:=IM Disassociate
```

| Parameter | Type | Description |
|---|---|---|
| `$result` | Longint | `1` on success, `0` on failure or when no window has focus |

Disables the input method for the focused window, for fields that only accept codes or numbers. Call `IM Associate` when focus leaves the field.

### `IM Get mode`

```4d
$result:=IM Get mode($conversion; $sentence)
```

| Parameter | Type | | Description |
|---|---|---|---|
| `$conversion` | Longint | ← | Conversion mode: a sum of `CMODE_*` constants, or `CMODE_DIRECT_INPUT` (`-1`) when the input method is off |
| `$sentence` | Longint | ← | Sentence mode: a sum of `SMODE_*` constants |
| `$result` | Longint | ← | `1` on success, `0` on failure |

### `IM Set mode`

```4d
$result:=IM Set mode($conversion; $sentence)
```

| Parameter | Type | | Description |
|---|---|---|---|
| `$conversion` | Longint | → | Conversion mode, or `CMODE_DIRECT_INPUT` to turn the input method off |
| `$sentence` | Longint | → | Sentence mode |
| `$result` | Longint | ← | `1` on success, `0` on failure |

Any other conversion mode turns the input method on. The v1 examples still apply:

```4d
// half-width alphanumeric
$result:=IM Set mode(CMODE_ROMAN; SMODE_AUTOMATIC)
// full-width hiragana
$result:=IM Set mode(CMODE_ROMAN+CMODE_FULLSHAPE+CMODE_NATIVE; SMODE_AUTOMATIC)
```

`IM Set input mode` is simpler for the usual Japanese modes: it keeps the other bits, such as romaji or kana input.

### `IM Get open status`

```4d
$status:=IM Get open status
```

| Parameter | Type | Description |
|---|---|---|
| `$status` | Longint | `1` when the input method is on, `0` when it is off, or a negative [IM Error](#im-error) |

### `IM Set open status`

```4d
$result:=IM Set open status($open)
```

| Parameter | Type | Description |
|---|---|---|
| `$open` | Longint | `1` to turn the input method on (the Kanji key), `0` to turn it off |
| `$result` | Longint | Status |

On macOS, turning the input method on selects the hiragana mode of the Japanese input method.

### `IM Get input mode`

```4d
$mode:=IM Get input mode
```

| Parameter | Type | Description |
|---|---|---|
| `$mode` | Longint | One of the [IM Input Mode](#im-input-mode) constants; `IM mode unknown` when no window has focus or the mode is not a Japanese one |

### `IM Set input mode`

```4d
$result:=IM Set input mode($mode)
```

| Parameter | Type | Description |
|---|---|---|
| `$mode` | Longint | One of the [IM Input Mode](#im-input-mode) constants except `IM mode unknown` |
| `$result` | Longint | Status; `IM error invalid argument` for another value |

| Mode | Input method | Conversion mode on Windows |
|---|---|---|
| `IM mode direct input` | Off | unchanged |
| `IM mode hiragana` | On | `CMODE_NATIVE+CMODE_FULLSHAPE` |
| `IM mode katakana` | On | `CMODE_NATIVE+CMODE_KATAKANA+CMODE_FULLSHAPE` |
| `IM mode halfwidth katakana` | On | `CMODE_NATIVE+CMODE_KATAKANA` |
| `IM mode fullwidth alphanumeric` | On | `CMODE_FULLSHAPE` |
| `IM mode halfwidth alphanumeric` | On (Windows), direct input (macOS) | `CMODE_ALPHANUMERIC` |

### `IM Get composition`

```4d
$composition:=IM Get composition
```

| Parameter | Type | Description |
|---|---|---|
| `$composition` | Object | The text being composed in the focused window |

| Property | Type | Description |
|---|---|---|
| `status` | Number | Status |
| `active` | Boolean | `True` while the user is composing text |
| `text` | Text | Composition text |
| `reading` | Text | Reading of the composition, when the input method gives one (Windows) |
| `cursor` | Number | Caret position in `text` |

### `IM Complete composition`

```4d
$result:=IM Complete composition
```

Commits the text being composed, as if the user pressed Enter. Call it before your code reads or saves the field, for example in a button that saves the record, so that text still in the input method is not lost. Returns a status; `0` also when nothing was being composed.

### `IM Cancel composition`

```4d
$result:=IM Cancel composition
```

Discards the text being composed. Returns a status.

### `IM Get input methods`

```4d
$methods:=IM Get input methods
```

| Parameter | Type | Description |
|---|---|---|
| `$methods` | Collection | The enabled input methods and keyboard layouts |

Each element:

| Property | Type | Description |
|---|---|---|
| `id` | Text | Identifier for `IM Set input method`. Windows: `tip:0411:{clsid}:{profile}` for an input method, `hkl:04090409` for a keyboard layout. macOS: the input source identifier, e.g. `com.apple.inputmethod.Kotoeri.RomajiTyping.Japanese` |
| `name` | Text | Display name |
| `language` | Text | Language tag, e.g. `ja-JP` (Windows) or `ja` (macOS) |
| `type` | Text | `ime` (input method), `keyboard` (keyboard layout) or `mode` (an input mode of an input method, macOS) |
| `selected` | Boolean | `True` for the current input method |

### `IM Get input method`

```4d
$method:=IM Get input method
```

Returns the current input method, with the properties above and `status`.

### `IM Set input method`

```4d
$result:=IM Set input method($id)
```

| Parameter | Type | Description |
|---|---|---|
| `$id` | Text | `id` from `IM Get input methods` |
| `$result` | Longint | Status; `IM error not found` when no enabled input method has this `id` |

```4d
// Select the Japanese input method, whichever one the user has
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

| Parameter | Type | Description |
|---|---|---|
| `$enable` | Longint | `1` to start capturing (and clear the captured reading), `0` to stop |
| `$result` | Longint | Status; `IM error not supported` on macOS |

While capture is on, the plugin records the reading of every string the user commits in the input method: the reading the input method reports, or else the kana typed before conversion. This fills a furigana field with the reading the user actually typed (`東海林` typed as `しょうじ`), which a dictionary cannot guess. Text the user deletes afterwards stays in the captured reading: start capture again to clear it.

The plugin reads the input method messages (`WM_IME_COMPOSITION`) of 4D's windows. Check it with the input methods your users have: an input method that sends no reading and converts before showing kana leaves the kanji in the captured reading.

```4d
// Object method of the name field
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

| Parameter | Type | Description |
|---|---|---|
| `$form` | Longint | One of the [IM Kana](#im-kana) constants; hiragana when omitted |
| `$reading` | Text | The captured reading, `""` on macOS or for an unknown form |

### `IM Get phonetic`

```4d
$reading:=IM Get phonetic($text; $form)
```

| Parameter | Type | Description |
|---|---|---|
| `$text` | Text | Japanese text |
| `$form` | Longint | One of the [IM Kana](#im-kana) constants; hiragana when omitted |
| `$reading` | Text | Phonetic reading, `""` when no reading service is available |

The reading comes from a dictionary, so names can be read differently from how their owner reads them; use reading capture when the user types the text. Latin letters, digits and kana are kept as written. Thread-safe.

```4d
$reading:=IM Get phonetic("山田太郎"; IM kana katakana)  // ヤマダタロウ
```

### `IM Convert kana`

```4d
$result:=IM Convert kana($text; $form)
```

| Parameter | Type | Description |
|---|---|---|
| `$text` | Text | Text to convert |
| `$form` | Longint | One of the [IM Kana](#im-kana) constants; hiragana when omitted |
| `$result` | Text | Converted text, `""` for an unknown form |

Converts between hiragana, katakana and half-width katakana. Half-width voiced kana (`ｶﾞ`) become one full-width character (`ガ`) and the other way round. `IM kana halfwidth katakana` also narrows full-width ASCII and the ideographic space, as Japanese bank transfer (Zengin) data requires. Kanji and other characters are kept. Thread-safe.

```4d
$result:=IM Convert kana("ヤマダ　タロウ"; IM kana halfwidth katakana)  // ﾔﾏﾀﾞ ﾀﾛｳ
```

4D compares text without regard to kana type or width (`"か"="カ"` is `True`). Use `Compare strings($a; $b; sk char codes)` to test the result of a conversion.

## Constants

### IME Conversion Mode

Unchanged from v1. The values are the Windows `IME_CMODE_*` flags.

| Constant | Value |
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

Unchanged from v1.

| Constant | Value |
|---|---|
| `SMODE_NONE` | 0 |
| `SMODE_PLURALCLAUSE` | 1 |
| `SMODE_SINGLECONVERT` | 2 |
| `SMODE_AUTOMATIC` | 4 |
| `SMODE_PHRASEPREDICT` | 8 |
| `SMODE_CONVERSATION` | 16 |

### IM Input Mode

| Constant | Value |
|---|---|
| `IM mode unknown` | -1 |
| `IM mode direct input` | 0 |
| `IM mode hiragana` | 1 |
| `IM mode katakana` | 2 |
| `IM mode halfwidth katakana` | 3 |
| `IM mode fullwidth alphanumeric` | 4 |
| `IM mode halfwidth alphanumeric` | 5 |

### IM Kana

| Constant | Value |
|---|---|
| `IM kana hiragana` | 0 |
| `IM kana katakana` | 1 |
| `IM kana halfwidth katakana` | 2 |

### IM Error

| Constant | Value | Meaning |
|---|---|---|
| `IM error none` | 0 | Success |
| `IM error no window` | -1 | No window or text field has keyboard focus |
| `IM error no input method` | -2 | The window has no input context (for example after `IM Disassociate`), or no matching input method is installed |
| `IM error not supported` | -3 | Not available on this platform or system |
| `IM error invalid argument` | -4 | A parameter is out of range or empty |
| `IM error not found` | -5 | No enabled input method has this identifier |
| `IM error failed` | -6 | The system or the input method rejected the request |

## Building from Source

### Prerequisites

- CMake 3.20+
- Xcode (macOS) or Visual Studio 2022 or later (Windows)

### Clone

```bash
git clone --recurse-submodules https://github.com/miyako/4d-plugin-input-method-manager-v2.git
cd 4d-plugin-input-method-manager-v2
```

### Build (macOS)

```bash
cd input-method-manager
mkdir -p cmake-build && cd cmake-build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build .
```

### Build (Windows)

```pwsh
cd input-method-manager
mkdir cmake-build; cd cmake-build
cmake .. -A x64
cmake --build . --config Release
```

The plugin is written to `input-method-manager/input-method-manager-test/Plugins`.

### Run Tests

Requires [tool4d](https://developer.4d.com/docs/Admin/cli/) (free, no license needed):

```bash
/path/to/tool4d --dataless --startup-method=test_all --project=$(pwd)/input-method-manager/input-method-manager-test/Project/input-method-manager.4DProject
```

tool4d has no window, so the tests cover kana conversion, phonetic readings, the input method list and the results without a focused window. They never change the machine's input method. Check the input mode commands in a form of a 4D application.

## CI/CD

This project uses the canonical generated three-workflow native-plugin pipeline:

| Workflow | Trigger | Purpose |
|---|---|---|
| `test.yml` | Push / PR / manual / reusable | Build, test and validate same-revision artifacts |
| `bump-version.yml` | Manual, default branch only | Bump `VERSION` and commit; no tag |
| `release.yml` | Manual, default branch only; no inputs | Reuse tests, sign, notarize, verify, draft and publish `vVERSION` |

### Required Secrets (for `release.yml` only)

Configure these in **Settings → Secrets and variables → Actions**:

| Secret | Description |
|---|---|
| `APPLE_DEVELOPER_ID_CERTIFICATE` | Base64-encoded Developer ID Application `.p12` including its private key |
| `APPLE_DEVELOPER_ID_CERTIFICATE_PASSWORD` | Password for the `.p12` export |
| `KEYCHAIN_PASSWORD` | Independent password for the CI runner's temporary keychain |
| `NOTARYTOOL_APPLE_ID` | Apple ID email for notarization |
| `NOTARYTOOL_TEAM_ID` | Apple Developer Team ID (10-char alphanumeric) |
| `NOTARYTOOL_PASSWORD` | Apple app-specific password, not the account password |

## License

[MIT](LICENSE)
