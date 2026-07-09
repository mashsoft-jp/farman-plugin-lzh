#include "LzhArchivePlugin.h"
#include "core/ArchiveEntryName.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <archive.h>
#include <archive_entry.h>

#ifndef Q_OS_WIN
#include <locale.h>
#ifdef Q_OS_MAC
#include <xlocale.h>
#endif
#endif

namespace Farman {

namespace {

#ifdef Q_OS_WIN
inline const wchar_t* asWChar(const QString& s) {
  return reinterpret_cast<const wchar_t*>(s.utf16());
}
#endif

#ifndef Q_OS_WIN
// libarchive の LHA リーダは、プロセスのロケールが UTF-8 のとき CP932 (Shift-JIS)
// ファイル名を含むエントリのヘッダ解析に失敗し ARCHIVE_FATAL を返す。Qt アプリは
// QApplication 構築時に UTF-8 ロケールを設定するため、日本語ファイル名の lzh で
// これを踏む (ASCII 名だけの lzh では顕在化しない)。
//
// 対策: libarchive 呼び出しの間だけ、このスレッドのロケール (LC_CTYPE) を "C" に
// 切り替える。バイト列をそのまま取得できるので、文字コード判定は自前の
// decodeArchiveEntryName (UTF-8 か Shift-JIS を判定) に委ねられる。uselocale は
// スレッドローカルなので GUI スレッドや他ワーカーには影響しない (listEntries /
// extractEntry はワーカースレッドで実行される)。
class ScopedCLocale {
public:
  ScopedCLocale() {
    m_c = newlocale(LC_CTYPE_MASK, "C", static_cast<locale_t>(0));
    if (m_c) m_prev = uselocale(m_c);
  }
  ~ScopedCLocale() {
    if (m_c) {
      uselocale(m_prev);
      freelocale(m_c);
    }
  }
  ScopedCLocale(const ScopedCLocale&) = delete;
  ScopedCLocale& operator=(const ScopedCLocale&) = delete;
private:
  locale_t m_c    = static_cast<locale_t>(0);
  locale_t m_prev = static_cast<locale_t>(0);
};
#else
// Windows は _w パス経由で ACP を使い、この POSIX ロケール依存の問題は該当しない。
class ScopedCLocale { public: ScopedCLocale() {} };
#endif

// libarchive のエントリパスを「先頭/末尾 '/' なし」のキー文字列に整える。
// CP932 (Shift-JIS) の lzh でも文字化けしないよう共通ヘルパで判定する。
QString readEntryPath(struct archive_entry* entry) {
  QString path = decodeArchiveEntryName(entry);
  while (path.size() > 0 && path.endsWith(QLatin1Char('/')))   path.chop(1);
  while (path.size() > 0 && path.startsWith(QLatin1Char('/'))) path.remove(0, 1);
  return path;
}

// lzh (lha) 用に libarchive の read ハンドラを構成して archivePath を開く。
// 成功時は開いた archive* を返す (呼び出し側で close/free)。失敗時 nullptr。
struct archive* openLzh(const QString& archivePath) {
  struct archive* a = archive_read_new();
  // lzh/lha フォーマットのみ有効化 (このプラグインの責務範囲)。フィルタは
  // 念のため全て許可しておく (lzh は通常フィルタ無しだが害はない)。
  archive_read_support_format_lha(a);
  archive_read_support_filter_all(a);

#ifdef Q_OS_WIN
  const int r = archive_read_open_filename_w(a, asWChar(archivePath), 64 * 1024);
#else
  const int r = archive_read_open_filename(
    a, archivePath.toUtf8().constData(), 64 * 1024);
#endif
  if (r != ARCHIVE_OK) {
    archive_read_free(a);
    return nullptr;
  }
  return a;
}

} // namespace

ArchiveListResult LzhArchivePlugin::listEntries(const QString&     archivePath,
                                                std::atomic<bool>* cancelFlag,
                                                std::atomic<int>*  entriesRead) {
  ArchiveListResult result;
  // libarchive の LHA ヘッダ解析を UTF-8 ロケール依存の失敗から守る (下記 class 参照)。
  ScopedCLocale cLocale;

  struct archive* a = openLzh(archivePath);
  if (!a) {
    result.error = tr("Failed to open LZH archive: %1").arg(archivePath);
    return result;
  }

  struct archive_entry* entry = nullptr;
  while (true) {
    if (cancelFlag && cancelFlag->load()) {
      result.error = tr("Archive load cancelled.");
      archive_read_close(a);
      archive_read_free(a);
      return result;  // ok == false
    }
    const int r = archive_read_next_header(a, &entry);
    if (r == ARCHIVE_EOF) break;
    if (r < ARCHIVE_WARN) {
      result.error = tr("LZH read error (rc=%1 errno=%2): %3")
                       .arg(r).arg(archive_errno(a))
                       .arg(QString::fromUtf8(archive_error_string(a)));
      archive_read_close(a);
      archive_read_free(a);
      return result;
    }
    if (r < ARCHIVE_OK) continue;  // 警告は流す

    const QString path = readEntryPath(entry);
    if (path.isEmpty()) continue;

    // 安全性フィルタ・合成ディレクトリ生成はホスト側 (ArchiveContext) が行う。
    // ここでは raw な一覧をそのまま返す。
    ArchiveEntry e;
    e.pathInArchive  = path;
    const int slash  = path.lastIndexOf(QLatin1Char('/'));
    e.name           = (slash < 0) ? path : path.mid(slash + 1);
    e.isDir          = (archive_entry_filetype(entry) == AE_IFDIR);
    e.size           = e.isDir ? -1
                               : static_cast<qint64>(archive_entry_size(entry));
    e.compressedSize = -1;
    const time_t mt  = archive_entry_mtime(entry);
    e.mtime          = (mt > 0) ? QDateTime::fromSecsSinceEpoch(mt) : QDateTime();
#ifdef Q_OS_WIN
    if (const wchar_t* wlink = archive_entry_symlink_w(entry)) {
      e.linkTarget = QString::fromWCharArray(wlink);
    } else if (const char* link = archive_entry_symlink_utf8(entry)) {
      e.linkTarget = QString::fromUtf8(link);
    }
#else
    if (const char* link = archive_entry_symlink_utf8(entry)) {
      e.linkTarget = QString::fromUtf8(link);
    } else if (const char* link2 = archive_entry_symlink(entry)) {
      e.linkTarget = QString::fromUtf8(link2);
    }
#endif
    result.entries.append(e);
    if (entriesRead) entriesRead->fetch_add(1, std::memory_order_relaxed);
  }

  archive_read_close(a);
  archive_read_free(a);
  result.ok = true;
  return result;
}

bool LzhArchivePlugin::extractEntry(const QString& archivePath,
                                    const QString& entryPath,
                                    const QString& destPath,
                                    const QString& /*password*/) {
  // listEntries と同様、libarchive 呼び出し区間はスレッドローカル C ロケールで守る。
  ScopedCLocale cLocale;

  // 親ディレクトリを確保 (ホスト側でも mkpath するが二重でも無害)。
  QFileInfo destInfo(destPath);
  if (!QDir().mkpath(destInfo.absolutePath())) return false;

  struct archive* a = openLzh(archivePath);
  if (!a) return false;

  bool found = false;
  bool ok    = false;
  struct archive_entry* entry = nullptr;
  while (true) {
    const int r = archive_read_next_header(a, &entry);
    if (r == ARCHIVE_EOF) break;
    if (r < ARCHIVE_WARN) break;
    if (r < ARCHIVE_OK)  continue;

    if (readEntryPath(entry) != entryPath) continue;

    found = true;
    QFile out(destPath);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Truncate)) break;

    bool writeOk = true;
    const void* buf = nullptr;
    size_t      sz  = 0;
    la_int64_t  off = 0;
    while (true) {
      const int rr = archive_read_data_block(a, &buf, &sz, &off);
      if (rr == ARCHIVE_EOF) break;
      if (rr < ARCHIVE_OK)  { writeOk = false; break; }
      const qint64 written = out.write(reinterpret_cast<const char*>(buf), sz);
      if (written < 0 || static_cast<size_t>(written) != sz) {
        writeOk = false; break;
      }
    }
    out.close();
    ok = writeOk;
    break;
  }

  archive_read_close(a);
  archive_read_free(a);
  if (!found || !ok) {
    QFile::remove(destPath);
    return false;
  }
  return true;
}

} // namespace Farman
