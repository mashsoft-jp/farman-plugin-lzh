#pragma once

#include "core/IArchivePlugin.h"
#include <QObject>

namespace Farman {

// LZH (LHA) アーカイブの読み取り (エントリ列挙 + 展開) を提供するプラグイン。
// farman コアのアーカイブ機構 (zip/tar) と同じ libarchive を、プラグイン内で
// 独自にリンクして使う。lzh 圧縮 (作成) は libarchive に write ハンドラが無い
// ため非対応 (読み取り専用)。
//
// 「ビュアー以外の外部プラグイン」の第一実装。IArchivePlugin (IID
// com.farman.IArchivePlugin/1.0) を実装する。farman 本体には同梱せず外部配布し、
// ユーザーが自分の外部プラグインディレクトリ (<AppData>/plugins/archives/) に
// 置くと farman が起動時に動的ロードする。
class LzhArchivePlugin : public QObject, public IArchivePlugin {
  Q_OBJECT
  Q_PLUGIN_METADATA(IID FarmanIArchivePlugin_iid)
  Q_INTERFACES(Farman::IArchivePlugin)

public:
  LzhArchivePlugin() = default;
  ~LzhArchivePlugin() override = default;

  QString pluginId()   const override { return QStringLiteral("lzh_archive"); }
  QString pluginName() const override { return QStringLiteral("LZH Archive"); }
  QString author()     const override { return QStringLiteral("Mashsoft Inc."); }
  QString authorUrl()  const override {
    return QStringLiteral("https://www.mashsoft.co.jp");
  }
  // 配布バージョン。CMake の project() VERSION を LZH_PLUGIN_VERSION として
  // コンパイル時に埋め込む。未定義ビルド (手元の素朴なコンパイル等) では "dev"。
  QString version()    const override {
#ifdef LZH_PLUGIN_VERSION
    return QStringLiteral(LZH_PLUGIN_VERSION);
#else
    return QStringLiteral("dev");
#endif
  }
  // farman 本体には同梱せず、外部プラグインとして別途配布する。外部プラグインは
  // priority 0〜9999 の範囲を使う (10000 以上は同梱公式の予約域で、外部ロード時に
  // 拒否される)。
  int     priority()   const override { return 100; }

  QStringList supportedExtensions() const override {
    return {QStringLiteral("lzh"), QStringLiteral("lha")};
  }

  ArchiveListResult listEntries(const QString&     archivePath,
                                std::atomic<bool>* cancelFlag,
                                std::atomic<int>*  entriesRead) override;

  bool extractEntry(const QString& archivePath,
                    const QString& entryPath,
                    const QString& destPath,
                    const QString& password) override;
};

} // namespace Farman
