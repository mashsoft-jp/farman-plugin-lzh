cmake_minimum_required(VERSION 3.21)
project(farman-plugin-lzh VERSION 1.0.0 LANGUAGES CXX)

# farman 本体とは独立してビルドする、farman 用の外部アーカイブプラグイン。
# 成果物 LzhArchivePlugin.(dylib|so|dll) を farman の外部プラグインディレクトリ
# <AppData>/plugins/archives/ に置くと、farman が起動時にロードして lzh/lha の
# 展開・内部ブラウジングを有効化する。詳細は README.md 参照。

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# ── 依存: Qt6 と libarchive ───────────────────────────────────────
# farman と同じ Qt を指すよう、macOS の Homebrew では
#   cmake -B build -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
# のように渡す。
find_package(Qt6 REQUIRED COMPONENTS Core Core5Compat)

# Homebrew の libarchive は keg-only なので明示的にパスを通す。
if(APPLE)
  if(EXISTS /opt/homebrew/opt/libarchive)
    list(APPEND CMAKE_PREFIX_PATH /opt/homebrew/opt/libarchive)
  elseif(EXISTS /usr/local/opt/libarchive)
    list(APPEND CMAKE_PREFIX_PATH /usr/local/opt/libarchive)
  endif()
endif()
find_package(LibArchive REQUIRED)

# ── プラグイン本体 ────────────────────────────────────────────────
# farman-sdk/ は farman 本体から vendoring した共有ファイル (ABI ヘッダ
# IArchivePlugin.h と、その既定実装・Shift-JIS 対応エントリ名デコード)。
qt_add_plugin(LzhArchivePlugin
  CLASS_NAME LzhArchivePlugin
  src/LzhArchivePlugin.cpp
  farman-sdk/core/IArchivePlugin.cpp
  farman-sdk/core/ArchiveEntryName.cpp
)

set_target_properties(LzhArchivePlugin PROPERTIES
  PREFIX ""                       # libLzh... ではなく LzhArchivePlugin にする
  LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
  RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/dist"
)

target_include_directories(LzhArchivePlugin PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  ${CMAKE_SOURCE_DIR}/farman-sdk   # "core/IArchivePlugin.h" 等を解決
)

# project() の VERSION をプラグインの version() として埋め込む。
# リリース時はこの CMakeLists の project(... VERSION x.y.z) を配布タグに合わせて更新する。
target_compile_definitions(LzhArchivePlugin PRIVATE
  LZH_PLUGIN_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(LzhArchivePlugin PRIVATE
  Qt6::Core
  Qt6::Core5Compat                 # ArchiveEntryName の QTextCodec (Shift-JIS)
  LibArchive::LibArchive
)
