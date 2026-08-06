cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR)
    message(FATAL_ERROR "ZZ_SOURCE_DIR is required")
endif()
if(NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "source directory does not exist: ${ZZ_SOURCE_DIR}")
endif()
file(TO_CMAKE_PATH "${ZZ_SOURCE_DIR}" zz_source_root_normalized)

set(zz_public_roots
    "${ZZ_SOURCE_DIR}/ZzCore/include"
    "${ZZ_SOURCE_DIR}/ZzWindowKit/include"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/foundation/include"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets/include"
    "${ZZ_SOURCE_DIR}/ZzPureTools/appcore/include"
    "${ZZ_SOURCE_DIR}/ZzPureTools/widgets/include"
)

foreach(zz_public_root IN LISTS zz_public_roots)
    file(GLOB_RECURSE zz_public_headers
        LIST_DIRECTORIES FALSE
        "${zz_public_root}/*.h"
    )
    foreach(zz_public_header IN LISTS zz_public_headers)
        file(READ "${zz_public_header}" zz_public_content)

        if(zz_public_content MATCHES
           "#[ \\t]*include[ \\t]*[<\"][^>\"]*(QWK|qwindowkit|Qt[^>\"]*/private|spdlog|fmt/)")
            message(FATAL_ERROR
                "forbidden dependency leaked into public header: ${zz_public_header}")
        endif()
        if(zz_public_content MATCHES
           "#[ \\t]*include[ \\t]*[<\"][^>\"]*\\.\\./")
            message(FATAL_ERROR
                "relative parent include leaked into public header: ${zz_public_header}")
        endif()

        set(zz_type_content "${zz_public_content}")
        string(REGEX REPLACE
            "ZZ_[A-Z0-9_]+_EXPORT[ \\t]+" "" zz_type_content "${zz_type_content}")
        string(REGEX MATCHALL
            "(class|struct|enum[ \\t]+class)[ \\t\\r\\n]+(\\[\\[[^]]*\\]\\][ \\t\\r\\n]*)*[A-Za-z_][A-Za-z0-9_]*[^;{]*\\{"
            zz_type_definitions "${zz_type_content}")
        string(REGEX MATCHALL
            "concept[ \\t]+[A-Za-z_][A-Za-z0-9_]*[ \\t\\r\\n]*="
            zz_concept_definitions "${zz_type_content}")
        list(APPEND zz_type_definitions ${zz_concept_definitions})

        get_filename_component(zz_header_stem "${zz_public_header}" NAME_WE)
        set(zz_has_primary_type FALSE)
        foreach(zz_definition IN LISTS zz_type_definitions)
            string(REGEX MATCH
                "(class|struct|concept|enum[ \\t]+class)[ \\t\\r\\n]+(\\[\\[[^]]*\\]\\][ \\t\\r\\n]*)*[A-Za-z_][A-Za-z0-9_]*"
                zz_type_prefix "${zz_definition}")
            string(REGEX MATCH
                "[A-Za-z_][A-Za-z0-9_]*$" zz_type_name "${zz_type_prefix}")
            if(NOT zz_type_name MATCHES "^Zz")
                message(FATAL_ERROR
                    "public type lacks Zz prefix in ${zz_public_header}: ${zz_type_name}")
            endif()
            if(zz_type_name STREQUAL zz_header_stem)
                set(zz_has_primary_type TRUE)
            endif()
        endforeach()
        if(zz_type_definitions AND NOT zz_has_primary_type)
            message(FATAL_ERROR
                "public header has no primary type matching its file name: ${zz_public_header}")
        endif()

        if(zz_type_definitions
           AND (NOT zz_public_content MATCHES "/\\*\\*"
                OR NOT zz_public_content MATCHES "@brief"))
            message(FATAL_ERROR
                "public declaration lacks Chinese Doxygen structure: ${zz_public_header}")
        endif()
    endforeach()
endforeach()

file(GLOB_RECURSE zz_first_party_files
    LIST_DIRECTORIES FALSE
    "${ZZ_SOURCE_DIR}/ZzCore/*.h"
    "${ZZ_SOURCE_DIR}/ZzCore/*.cpp"
    "${ZZ_SOURCE_DIR}/ZzWindowKit/*.h"
    "${ZZ_SOURCE_DIR}/ZzWindowKit/*.cpp"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/*.h"
    "${ZZ_SOURCE_DIR}/ZzFluentUI/*.cpp"
    "${ZZ_SOURCE_DIR}/ZzPureTools/*.h"
    "${ZZ_SOURCE_DIR}/ZzPureTools/*.cpp"
    "${ZZ_SOURCE_DIR}/examples/ZzPureToolsExample/*.h"
    "${ZZ_SOURCE_DIR}/examples/ZzPureToolsExample/*.cpp"
    "${ZZ_SOURCE_DIR}/tests/Architecture/*.h"
    "${ZZ_SOURCE_DIR}/tests/Architecture/*.cpp"
)

foreach(zz_source IN LISTS zz_first_party_files)
    file(READ "${zz_source}" zz_source_content)
    file(TO_CMAKE_PATH "${zz_source}" zz_source_normalized)

    if(zz_source_normalized MATCHES
       "/tests/Architecture/fixtures/")
        continue()
    endif()

    if(zz_source_content MATCHES
       "namespace[ \\t\\r\\n]+[A-Za-z_][A-Za-z0-9_]*[ \\t]*::")
        message(FATAL_ERROR
            "chained namespace declaration is forbidden: ${zz_source}")
    endif()
    if(zz_source_content MATCHES
       "#[ \\t]*include[ \\t]*[<\"](Qt[^>\"]*/private/|private/q[^>\"]*_p\\.h|q[^>\"]*_p\\.h)")
        message(FATAL_ERROR "Qt Private include is forbidden: ${zz_source}")
    endif()
    if(zz_source_content MATCHES
       "#[ \\t]*include[ \\t]*[<\"][^>\"]*(QWK|qwindowkit)")
        string(FIND "${zz_source_normalized}"
            "${zz_source_root_normalized}/ZzWindowKit/src/private/"
            zz_window_private_pos)
        if(NOT zz_window_private_pos EQUAL 0)
            message(FATAL_ERROR
                "QWindowKit include escaped ZzWindowKit private: ${zz_source}")
        endif()
    endif()
endforeach()

file(GLOB_RECURSE zz_core_files
    LIST_DIRECTORIES FALSE
    "${ZZ_SOURCE_DIR}/ZzCore/*.h"
    "${ZZ_SOURCE_DIR}/ZzCore/*.cpp"
)
foreach(zz_core_file IN LISTS zz_core_files)
    file(READ "${zz_core_file}" zz_core_content)
    if(zz_core_content MATCHES
       "#[ \\t]*include[ \\t]*[<\"]Qt(Gui|Widgets|Quick)")
        message(FATAL_ERROR
            "ZzCore source includes a forbidden Qt UI module: ${zz_core_file}")
    endif()
endforeach()

file(READ "${ZZ_SOURCE_DIR}/ZzCore/CMakeLists.txt" zz_core_cmake)
if(zz_core_cmake MATCHES "Qt6::(Gui|Widgets|Quick)")
    message(FATAL_ERROR "ZzCore links a forbidden Qt UI target")
endif()

set(zz_ui_roots
    "${ZZ_SOURCE_DIR}/ZzFluentUI/widgets"
    "${ZZ_SOURCE_DIR}/ZzPureTools/widgets"
)
foreach(zz_ui_root IN LISTS zz_ui_roots)
    file(GLOB_RECURSE zz_ui_files
        LIST_DIRECTORIES FALSE
        "${zz_ui_root}/*.h"
        "${zz_ui_root}/*.cpp"
    )
    foreach(zz_ui_file IN LISTS zz_ui_files)
        file(READ "${zz_ui_file}" zz_ui_content)
        if(zz_ui_content MATCHES
           "#[ \\t]*include[ \\t]*[<\"][^>\"]*(Repository|Database|NetworkClient|DomainEntity)")
            message(FATAL_ERROR
                "UI source includes a forbidden business/storage type: ${zz_ui_file}")
        endif()
    endforeach()
endforeach()

message(STATUS "Zz architecture boundary scan passed")
