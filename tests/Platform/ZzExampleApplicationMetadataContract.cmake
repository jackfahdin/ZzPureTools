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
    "install(TARGETS ZzPureToolsExample")
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

message(STATUS "PASS ZzPureToolsExample application metadata contract")
