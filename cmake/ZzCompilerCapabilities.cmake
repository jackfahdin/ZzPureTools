include_guard(GLOBAL)

include(CheckCXXSourceCompiles)

function(zz_check_compiler_capabilities)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 13.1)
            message(FATAL_ERROR
                "ZzPureToolsPro requires GCC 13.1 or newer")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17.0)
            message(FATAL_ERROR
                "ZzPureToolsPro requires Clang 17 or newer")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
            message(FATAL_ERROR
                "ZzPureToolsPro requires Apple Clang 15 or newer")
        endif()
    elseif(MSVC)
        if(MSVC_VERSION LESS 1938)
            message(FATAL_ERROR
                "ZzPureToolsPro requires MSVC 19.38 or newer")
        endif()
    else()
        message(FATAL_ERROR
            "Unsupported C++ compiler: ${CMAKE_CXX_COMPILER_ID} "
            "${CMAKE_CXX_COMPILER_VERSION}")
    endif()

    set(zz_saved_required_flags "${CMAKE_REQUIRED_FLAGS}")
    if(MSVC)
        string(APPEND CMAKE_REQUIRED_FLAGS
            " /std:c++20 /Zc:__cplusplus")
    else()
        string(APPEND CMAKE_REQUIRED_FLAGS " -std=c++20")
    endif()

    check_cxx_source_compiles([[
        #include <format>
        #include <source_location>
        #include <stop_token>
        #include <string>

        int main()
        {
            std::stop_source source;
            const auto location = std::source_location::current();
            const std::string text =
                std::format("{}:{}", location.line(), 42);
            return source.stop_requested() || text.empty();
        }
    ]] ZZ_HAS_REQUIRED_CXX20_LIBRARY)

    set(CMAKE_REQUIRED_FLAGS "${zz_saved_required_flags}")
    if(NOT ZZ_HAS_REQUIRED_CXX20_LIBRARY)
        message(FATAL_ERROR
            "The selected C++ standard library lacks "
            "format/source_location/stop_token")
    endif()
endfunction()
