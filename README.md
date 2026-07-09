# farman-plugin-lzh

[farman](https://github.com/) 用の **LZH (LHA) アーカイブ外部プラグイン**です。
farman 本体には同梱されていません。lzh/lha を扱いたいユーザーがこのプラグインを
ダウンロード（またはビルド）して導入します。

- **できること**: `.lzh` / `.lha` アーカイブの **内部ブラウジング**（`x.lzh!/inner`
  形式でディレクトリを辿る）と **展開**（zip などと同じ操作）。日本語（Shift-JIS /
  CP932）ファイル名にも対応。
- **できないこと**: lzh 形式での **圧縮（作成）** は非対応（読み取り専用）。
  内部で libarchive の lha リーダを利用しており、libarchive に lzh の書き込み
  ハンドラが無いためです。

farman の `IArchivePlugin`（IID `com.farman.IArchivePlugin/1.0`）を実装する、
「ビュアー以外の外部プラグイン」の参照実装でもあります。

## 導入方法（ビルド済みバイナリを使う場合）

1. お使いの OS 用の `LzhArchivePlugin`（`.dylib` / `.so` / `.dll`）を入手します。
2. farman の**外部プラグインディレクトリの `archives/` サブフォルダ**に置きます。
   既定の場所は OS ごとに以下です（farman の設定でプラグインディレクトリを
   変更している場合はそちら配下の `archives/`）:

   | OS | 既定の配置先 |
   |----|-------------|
   | macOS | `~/Library/Application Support/Farman/farman/plugins/archives/` |
   | Linux | `~/.local/share/Farman/farman/plugins/archives/` |
   | Windows | `%APPDATA%\Farman\farman\plugins\archives\` |

   `archives` フォルダが無ければ作成してください。
3. farman を再起動します。以後、`.lzh` / `.lha` に Enter でアーカイブ内へ入り、
   展開もできます。

> 注意: プラグインは farman 本体と **同じ Qt / libarchive** でビルドされたものを
> 使ってください（ABI が一致している必要があります）。自分でビルドするのが確実です。

## 自分でビルドする

### 前提
- CMake 3.21 以上、C++20 対応コンパイラ
- Qt6（Core / Core5Compat）
- libarchive（lha リーダ有効。Homebrew / 主要 Linux ディストロの既定で有効）

### 手順（macOS / Homebrew の例）
```bash
cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
cmake --build build
# → build/dist/LzhArchivePlugin.dylib が生成される
```

生成物を上表の配置先へコピーして farman を再起動します。

## リポジトリ構成

```
src/                     プラグイン本体 (LzhArchivePlugin.{h,cpp})
farman-sdk/core/         farman 本体から vendoring した共有ファイル
                         (IArchivePlugin.h = ABI、その既定実装、Shift-JIS
                          対応エントリ名デコード)。詳細は farman-sdk/README.md
CMakeLists.txt           スタンドアロンのビルド定義
```

## ライセンス

farman 本体に準じます（`LICENSE` 参照）。
