cmake_minimum_required(VERSION 3.21)
project(BangkeInstaller LANGUAGES CXX RC)

if(NOT DEFINED VERSION_MAJOR)
  set(VERSION_MAJOR 0)
endif()
if(NOT DEFINED VERSION_MINOR)
  set(VERSION_MINOR 1)
endif()
if(NOT DEFINED VERSION_PATCH)
  set(VERSION_PATCH 0)
endif()
if(NOT DEFINED VERSION_BUILD)
  set(VERSION_BUILD 0)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

find_package(Qt6 6.8 REQUIRED COMPONENTS Widgets)
qt_standard_project_setup()

# 版本数与图标绝对路径在 configure 阶段烘进 rc:
# target_compile_definitions 不会到达 rc.exe,生成目录也让相对路径失锚
get_filename_component(RC_ICON "${CMAKE_CURRENT_SOURCE_DIR}/../../resource/bangke.ico" ABSOLUTE)
string(REPLACE "/" "\\\\" RC_ICON "${RC_ICON}")
configure_file(BangkeInstaller.rc.in BangkeInstaller.rc @ONLY)

qt_add_executable(BangkeInstaller WIN32
  main.cpp
  BangkeInstaller.rc
)

set_source_files_properties("../../resource/bangke-appicon.png" PROPERTIES QT_RESOURCE_ALIAS "bangke-appicon.png")
set_source_files_properties("../../LICENSE.txt" PROPERTIES QT_RESOURCE_ALIAS "LICENSE.txt")
qt_add_resources(BangkeInstaller "installer_assets"
  PREFIX "/installer"
  BASE "${CMAKE_CURRENT_SOURCE_DIR}/../.."
  FILES "../../resource/bangke-appicon.png" "../../LICENSE.txt"
)
target_link_libraries(BangkeInstaller PRIVATE Qt6::Widgets msi.lib advapi32.lib)
target_compile_definitions(BangkeInstaller PRIVATE
  VERSION_MAJOR=${VERSION_MAJOR}
  VERSION_MINOR=${VERSION_MINOR}
  VERSION_PATCH=${VERSION_PATCH}
  VERSION_BUILD=${VERSION_BUILD}
)
target_compile_options(BangkeInstaller PRIVATE /utf-8)
# 提权声明走链接器，避免与默认 manifest 合并冲突
set_target_properties(BangkeInstaller PROPERTIES
  LINK_FLAGS "/MANIFESTUAC:\"level='requireAdministrator'\"")

# Qt 运行库与 BangkeSettings 同目录共存（output\），无需单独 windeployqt
add_custom_command(TARGET BangkeInstaller POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:BangkeInstaller> $ENV{BANGKE_OUT})
