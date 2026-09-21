cmake_minimum_required(VERSION 3.21)
project(BangkeSettings LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# rime.dll 使用静态 MSVC 运行库，本程序须保持一致
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 版本数与图标绝对路径在 configure 阶段烘进 rc:
# target_compile_definitions 不会到达 rc.exe,生成目录也让相对路径失锚
get_filename_component(RC_ICON "${CMAKE_CURRENT_SOURCE_DIR}/../resource/bangke.ico" ABSOLUTE)
string(REPLACE "/" "\\\\" RC_ICON "${RC_ICON}")
configure_file(BangkeSettings.rc.in BangkeSettings.rc @ONLY)

if(NOT DEFINED VERSION_MAJOR)
  set(VERSION_MAJOR 0)
  set(VERSION_MINOR 1)
  set(VERSION_PATCH 0)
endif()
if(NOT DEFINED VERSION_BUILD)
  set(VERSION_BUILD 0)
endif()

if(NOT DEFINED ENV{BOOST_ROOT})
  message(FATAL_ERROR "BOOST_ROOT not set; run from env.bat / build.bat")
endif()

find_package(Qt6 REQUIRED COMPONENTS Quick QuickControls2 Qml)
# REQUIRES 6.8 取 QTP0001/QTP0004 的新行为:模块落在引擎默认导入路径 /qt/qml 下
# (main.cpp 的 loadFromModule 与兜底 URL 都按这条写的),子目录里的 .qml 也会
# 各生成一份 redirect qmldir —— qml/pages 那几层的隐式导入靠它
qt_standard_project_setup(REQUIRES 6.8)

set(BANGKE_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(NOT EXISTS ${BANGKE_ROOT}/lib64/rime.lib)
  message(FATAL_ERROR "${BANGKE_ROOT}/lib64/rime.lib not found; run 'build.bat rime' first")
endif()

qt_add_executable(BangkeSettings WIN32
  main.cpp
  AppController.cpp AppController.h
  SystemTheme.cpp SystemTheme.h
  Configurator.cpp Configurator.h
  UIStyleSettings.cpp UIStyleSettings.h
  models/AiModel.cpp models/AiModel.h
  models/DictModel.cpp models/DictModel.h
  models/GeneralModel.cpp models/GeneralModel.h
  models/StyleModel.cpp models/StyleModel.h
  models/SwitcherModel.cpp models/SwitcherModel.h
  BangkeSettings.rc
  ${BANGKE_ROOT}/BangkeIPC/BangkeClientImpl.cpp
  ${BANGKE_ROOT}/BangkeIPC/PipeChannel.cpp
  ${BANGKE_ROOT}/BangkeRime/BangkeUtility.cpp
)

# QML 编进 qrc(不铺散文件)。用 glob 而不是逐个列:qml/ 下的文件由视图侧随意增删,
# 免得每加一个 qml 都要回来改这里;CONFIGURE_DEPENDS 让新增文件自动进构建。
# RELATIVE 保留 qml/ 前缀 —— QML_FILES 要的就是相对本目录的路径。
file(GLOB_RECURSE BANGKE_QML RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  CONFIGURE_DEPENDS qml/*.qml
)
if(NOT BANGKE_QML)
  message(WARNING "BangkeSettings: qml/ 下没有 .qml —— GUI 入口 Main.qml 缺失")
endif()

# 必须在 qt_add_qml_module 之前:生成 qmldir 时才知道 Theme 是单例
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

# 控件观感一律在 QML 里以内联 background/indicator/contentItem 覆写,Basic 只是底座
# (main.cpp 里也用 QQuickStyle::setStyle("Basic") 再钉一次)
# RESOURCE_PREFIX 即 QTP0001 新行为的默认值,不写尾斜杠 —— 模块落在
# qrc:/qt/qml/Bangke/Pages/,与 main.cpp 的 loadFromModule/兜底 URL 一致
qt_add_qml_module(BangkeSettings
  URI Bangke.Pages
  VERSION 1.0
  RESOURCE_PREFIX /qt/qml
  QML_FILES ${BANGKE_QML}
)

target_include_directories(BangkeSettings PRIVATE
  # models/ 是子目录,头文件之间按裸名互相引用要靠这一条
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${BANGKE_ROOT}/include
  ${BANGKE_ROOT}/BangkeIPC
  ${BANGKE_ROOT}/BangkeRime
  ${BANGKE_ROOT}/librime/include
  $ENV{BOOST_ROOT}
)

target_link_directories(BangkeSettings PRIVATE
  $ENV{BOOST_ROOT}/stage/lib
  ${BANGKE_ROOT}/lib64
)

target_link_libraries(BangkeSettings PRIVATE
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Qml
  rime.lib
)

target_compile_definitions(BangkeSettings PRIVATE
  VERSION_MAJOR=${VERSION_MAJOR}
  VERSION_MINOR=${VERSION_MINOR}
  VERSION_PATCH=${VERSION_PATCH}
  VERSION_BUILD=${VERSION_BUILD}
)

target_compile_options(BangkeSettings PRIVATE /utf-8 /Zc:threadSafeInit-)
