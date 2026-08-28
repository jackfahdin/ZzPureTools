cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR "${ZZ_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()

file(REAL_PATH "${ZZ_SOURCE_DIR}" source_dir)
set(example_cmake
    "${source_dir}/examples/ZzPureToolsExample/CMakeLists.txt")
set(example_main
    "${source_dir}/examples/ZzPureToolsExample/main.cpp")
set(desktop_template
    "${source_dir}/packaging/linux/io.github.jackfahdin.ZzPureToolsExample.desktop.in")
set(appdata_template
    "${source_dir}/packaging/linux/io.github.jackfahdin.ZzPureToolsExample.appdata.xml.in")
set(windows_resource_template
    "${source_dir}/examples/ZzPureToolsExample/resources/application/ZzPureToolsExample.rc.in")
set(application_resource_dir
    "${source_dir}/examples/ZzPureToolsExample/resources/application")

set(required_text_files
    "${example_cmake}"
    "${example_main}"
    "${desktop_template}"
    "${appdata_template}"
    "${windows_resource_template}")
foreach(file_path IN LISTS required_text_files)
    if(NOT EXISTS "${file_path}" OR IS_DIRECTORY "${file_path}")
        message(FATAL_ERROR
            "Missing required application metadata file: ${file_path}")
    endif()
endforeach()

file(READ "${example_cmake}" example_cmake_content)
set(required_cmake_tokens
    "MACOSX_BUNDLE TRUE"
    "WIN32_EXECUTABLE TRUE"
    "MACOSX_BUNDLE_GUI_IDENTIFIER"
    "io.github.jackfahdin.ZzPureToolsExample"
    [=[ZZ_EXAMPLE_VERSION="${PROJECT_VERSION}"]=]
    "install(TARGETS ZzPureToolsExample"
    "icons/hicolor/256x256/apps")
foreach(required_token IN LISTS required_cmake_tokens)
    string(FIND "${example_cmake_content}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "Example CMake metadata is missing token: ${required_token}")
    endif()
endforeach()

file(READ "${example_main}" example_main_content)
set(required_main_tokens
    "Jackfahdin"
    "ZzPureToolsExample"
    "ZZ_EXAMPLE_VERSION"
    "QIcon("
    ":/ZzPureToolsExample/application/ZzPureToolsExample.png")
foreach(required_token IN LISTS required_main_tokens)
    string(FIND "${example_main_content}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "Example runtime identity is missing token: ${required_token}")
    endif()
endforeach()

file(READ "${desktop_template}" desktop_content)
file(READ "${appdata_template}" appdata_content)
file(READ "${windows_resource_template}" windows_resource_content)
foreach(required_token IN ITEMS
    "Type=Application"
    "Exec=ZzPureToolsExample"
    "Icon=io.github.jackfahdin.ZzPureToolsExample"
    "Terminal=false")
    string(FIND "${desktop_content}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "Desktop metadata is missing token: ${required_token}")
    endif()
endforeach()
foreach(required_token IN ITEMS
    "ZzPureToolsExample"
    "<id>io.github.jackfahdin.ZzPureToolsExample</id>"
    "io.github.jackfahdin.ZzPureToolsExample.desktop"
    "Jackfahdin")
    string(FIND "${appdata_content}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "AppStream metadata is missing identity: ${required_token}")
    endif()
endforeach()
foreach(required_token IN ITEMS
    "ICON DISCARDABLE"
    "@ZZ_EXAMPLE_WINDOWS_ICON@")
    string(FIND
        "${windows_resource_content}" "${required_token}" token_index)
    if(token_index EQUAL -1)
        message(FATAL_ERROR
            "Windows resource metadata is missing token: ${required_token}")
    endif()
endforeach()

foreach(metadata_content IN ITEMS "${desktop_content}" "${appdata_content}")
    if(metadata_content MATCHES
        "(/home/|/Users/|(^|\n)[A-Za-z]:[/\\\\])")
        message(FATAL_ERROR
            "Linux application metadata contains an absolute host path")
    endif()
    if(metadata_content MATCHES "ZzPureToolsPro")
        message(FATAL_ERROR
            "Linux application metadata contains the retired product name")
    endif()
endforeach()

foreach(icon_extension IN ITEMS png ico icns)
    set(icon_path
        "${application_resource_dir}/ZzPureToolsExample.${icon_extension}")
    if(NOT EXISTS "${icon_path}" OR IS_DIRECTORY "${icon_path}")
        message(FATAL_ERROR "Missing application icon: ${icon_path}")
    endif()
    file(SIZE "${icon_path}" icon_size)
    if(icon_size EQUAL 0)
        message(FATAL_ERROR "Application icon is empty: ${icon_path}")
    endif()
endforeach()

set(application_png
    "${application_resource_dir}/ZzPureToolsExample.png")
file(READ "${application_png}" png_header_hex OFFSET 0 LIMIT 24 HEX)
string(TOLOWER "${png_header_hex}" png_header_hex)
string(SUBSTRING "${png_header_hex}" 0 32 png_signature_and_ihdr)
if(NOT png_signature_and_ihdr STREQUAL
   "89504e470d0a1a0a0000000d49484452")
    message(FATAL_ERROR
        "Linux application icon is not a canonical PNG: ${application_png}")
endif()
string(SUBSTRING "${png_header_hex}" 32 8 png_width_hex)
string(SUBSTRING "${png_header_hex}" 40 8 png_height_hex)
math(EXPR png_width "0x${png_width_hex}")
math(EXPR png_height "0x${png_height_hex}")
if(NOT png_width EQUAL 256 OR NOT png_height EQUAL 256)
    message(FATAL_ERROR
        "Linux application icon must be 256x256; found ${png_width}x${png_height}")
endif()

message(STATUS "PASS ZzPureToolsExample application metadata contract")
