# farman-sdk（vendoring）

このディレクトリのファイルは **farman 本体リポジトリからコピーしたもの**です。
外部プラグインを farman 本体のソースツリー無しで単体ビルドできるように同梱しています。

| ファイル | 役割 | farman 側の出所 |
|----------|------|-----------------|
| `core/IArchivePlugin.h` | **ABI（プラグインインターフェース）**。IID `com.farman.IArchivePlugin/<major>.<minor>` を含む | `src/core/IArchivePlugin.h` |
| `core/IArchivePlugin.cpp` | `IArchivePlugin::canHandle()` の既定実装 | `src/core/IArchivePlugin.cpp` |
| `core/ArchiveEntry.h` | エントリのメタデータ構造体（`IArchivePlugin.h` が参照） | `src/core/ArchiveEntry.h` |
| `core/ArchiveEntryName.{h,cpp}` | libarchive のエントリ名を UTF-8/Shift-JIS 判定で QString 化 | `src/core/ArchiveEntryName.{h,cpp}` |

## 同期について（重要）

`IArchivePlugin.h` の **IID が farman 本体と一致していないと、プラグインはロード時に
`qobject_cast` で拒否されます**（バージョン不一致は安全に弾かれる設計）。farman 本体が
インターフェースを破壊的変更して IID のメジャー番号を上げたら、ここのファイルも
更新してプラグインを再ビルドしてください。

対応する farman 本体のバージョン: **farman v1.0.0（IID 1.0）**。
