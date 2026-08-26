cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}")
    message(FATAL_ERROR "ZZ_SOURCE_DIR must name the source directory")
endif()

set(required_tokens
    "scripts/ci/run-linux-gates.sh|linux-gcc-debug"
    "scripts/ci/run-linux-gates.sh|linux-clang-tidy-release"
    "scripts/ci/run-linux-gates.sh|linux-clang-tidy-static"
    "scripts/ci/run-linux-gates.sh|linux-clang-asan"
    "scripts/ci/run-linux-gates.sh|linux-gcc-release"
    "scripts/ci/run-linux-gates.sh|linux-static-release"
    "scripts/ci/run-linux-gates.sh|linux-gcc-release-lto"
    "scripts/ci/run-linux-gates.sh|linux-static-release-lto"
    "scripts/ci/run-linux-gates.sh|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/run-linux-gates.sh|cmake --preset \"$preset\" -DZZ_BUILD_EXAMPLES=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF"
    "scripts/ci/run-linux-gates.sh|linux-gcc-benchmarks"
    "scripts/ci/run-linux-gates.sh|cmake --preset linux-gcc-benchmarks -DZZ_PERFORMANCE_REFERENCE:BOOL=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF"
    "scripts/ci/run-linux-gates.sh|linux-clang-asan-benchmarks"
    "scripts/ci/run-linux-gates.sh|cmake --preset linux-clang-asan-benchmarks -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF"
    "scripts/ci/run-linux-gates.sh|run-linux-performance-gates.sh"
    "scripts/ci/run-linux-gates.sh|-LE benchmark"
    "scripts/ci/run-linux-gates.sh|-DZZ_PERFORMANCE_REFERENCE:BOOL=ON"
    "scripts/ci/run-linux-gates.sh|cmake --preset linux-gcc-benchmarks -DZZ_PERFORMANCE_REFERENCE:BOOL=ON"
    "scripts/ci/run-linux-performance-gates.sh|for round in 1 2 3"
    "scripts/ci/run-linux-performance-gates.sh|-L benchmark"
    "scripts/ci/run-linux-performance-gates.sh|ZZ_PERFORMANCE_REFERENCE:BOOL=ON"
    "scripts/ci/run-linux-performance-gates.sh|grep -Fx 'ZZ_PERFORMANCE_REFERENCE:BOOL=ON'"
    "scripts/ci/run-linux-performance-gates.sh|release-rounds/round-\${round}"
    "scripts/ci/run-linux-performance-gates.sh|ZzComparePerformanceReport.cmake"
    "scripts/ci/run-linux-performance-gates.sh|regression-thresholds.json"
    "scripts/ci/run-linux-performance-gates.sh|-DZZ_ABSOLUTE_GATES_VERIFIED=TRUE"
    "scripts/ci/run-linux-performance-gates.sh|commands.log"
    "scripts/ci/run-linux-startup-stability-probe.sh|EUID"
    "scripts/ci/run-linux-startup-stability-probe.sh|for round in $(seq 1 10)"
    "scripts/ci/run-linux-startup-stability-probe.sh|-displayfd 3"
    "scripts/ci/run-linux-startup-stability-probe.sh|taskset -c"
    "scripts/ci/run-linux-startup-stability-probe.sh|ZzStartupBenchmark"
    "scripts/ci/run-linux-startup-stability-probe.sh|ZzComparePerformanceReport.cmake"
    "scripts/ci/run-linux-startup-stability-probe.sh|regression-thresholds.json"
    "scripts/ci/run-linux-startup-stability-probe.sh|governor-experiment/\${phase}"
    "scripts/ci/run-linux-startup-stability-probe.sh|summary.json"
    "scripts/ci/run-linux-startup-stability-probe.sh|INVALID "
    "scripts/ci/run-linux-startup-stability-probe.sh|trap cleanup_xvfb EXIT"
    "scripts/ci/run-linux-startup-governor-experiment.sh|EUID -ne 0"
    "scripts/ci/run-linux-startup-governor-experiment.sh|SUDO_USER"
    "scripts/ci/run-linux-startup-governor-experiment.sh|SUDO_UID"
    "scripts/ci/run-linux-startup-governor-experiment.sh|SUDO_GID"
    "scripts/ci/run-linux-startup-governor-experiment.sh|mktemp -d"
    "scripts/ci/run-linux-startup-governor-experiment.sh|trap finish EXIT"
    "scripts/ci/run-linux-startup-governor-experiment.sh|trap 'exit 130' INT"
    "scripts/ci/run-linux-startup-governor-experiment.sh|trap 'exit 143' TERM"
    "scripts/ci/run-linux-startup-governor-experiment.sh|trap 'exit 129' HUP"
    "scripts/ci/run-linux-startup-governor-experiment.sh|zz_governor_snapshot"
    "scripts/ci/run-linux-startup-governor-experiment.sh|zz_governor_apply"
    "scripts/ci/run-linux-startup-governor-experiment.sh|zz_governor_restore"
    "scripts/ci/run-linux-startup-governor-experiment.sh|zz_governor_verify"
    "scripts/ci/run-linux-startup-governor-experiment.sh|setpriv --reuid"
    "scripts/ci/run-linux-startup-governor-experiment.sh|--regid"
    "scripts/ci/run-linux-startup-governor-experiment.sh|--init-groups"
    "scripts/ci/run-linux-startup-governor-experiment.sh|clear_phase_evidence"
    "scripts/ci/run-linux-startup-governor-experiment.sh|--phase control"
    "scripts/ci/run-linux-startup-governor-experiment.sh|--phase performance"
    "scripts/ci/run-linux-startup-governor-experiment.sh|host-state-before.json"
    "scripts/ci/run-linux-startup-governor-experiment.sh|host-state-applied.json"
    "scripts/ci/run-linux-startup-governor-experiment.sh|host-state-restored.json"
    "scripts/ci/run-linux-startup-governor-experiment.sh|manual restore"
    "scripts/ci/run-linux-gates.sh|sha256:\${profile_digest}"
    "scripts/ci/run-linux-gates.sh|ZZ_UBUNTU2204_BUILD_IMAGE"
    "scripts/ci/run-linux-gates.sh|pending-user-validation"
    "scripts/ci/run-ubuntu2204-release-gates.sh|VERSION_ID"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-gcc-release"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-static-release"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-gcc-release-lto"
    "scripts/ci/run-ubuntu2204-release-gates.sh|linux-static-release-lto"
    "scripts/ci/run-ubuntu2204-release-gates.sh|ZZ_BUNDLE_GNU_RUNTIME=ON"
    "scripts/ci/run-ubuntu2204-release-gates.sh|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/run-windows-gates.ps1|windows-msvc2022-release"
    "scripts/ci/run-windows-gates.ps1|windows-msvc2022-static"
    "scripts/ci/run-windows-gates.ps1|windows-mingw-release"
    "scripts/ci/run-windows-gates.ps1|windows-mingw-static"
    "scripts/ci/run-windows-gates.ps1|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/Assert-QtMinGWKit.ps1|TrimEnd([char[]]@('\\', '/'))"
    "scripts/ci/run-macos-gates.sh|macos-clang-release-arm64"
    "scripts/ci/run-macos-gates.sh|macos-clang-release-x86_64"
    "scripts/ci/run-macos-gates.sh|macos-clang-static-arm64"
    "scripts/ci/run-macos-gates.sh|macos-clang-static-x86_64"
    "scripts/ci/run-macos-gates.sh|-DZZ_BUILD_EXAMPLES=ON"
    "scripts/ci/check-ubuntu2204-runtime.sh|GLIBCXX_"
    "scripts/ci/check-ubuntu2204-runtime.sh|libstdc++.so.6 =>"
    "scripts/ci/check-ubuntu2204-runtime.sh|not found"
    "cmake/ZzCompilerWarnings.cmake|/analyze:external-"
    "ZzThirdParty/ZzLog/CMakeLists.txt|$<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>"
    "ZzThirdParty/ZzLog/CMakeLists.txt|$<$<CXX_COMPILER_ID:MSVC>:/utf-8>"
    "tests/Platform/ZzBinaryDependencyCheck.cmake|LC_ALL=C"
    "tests/Platform/ZzBinaryDependencyCheck.cmake|LANG=C"
    "tests/Platform/ZzBinaryDependencyCheck.cmake|-E chdir \"\${binary_directory}\""
    "tests/Platform/ZzBinaryDependencyCheck.cmake|[A-Za-z0-9_.+-]+"
    "tests/Platform/ZzBinaryDependencyCheck.cmake|msvcrt"
    "tests/Platform/ZzBinaryDependencyCheck.cmake|^(lib)?zz"
    "tests/Platform/ZzBinaryDependencyCheck.cmake|\"\${ZZ_DUMPBIN}\" /nologo /dependents \"\${binary_name}\""
    "tests/Platform/ZzBinaryDependencyCheck.cmake|\"\${ZZ_OTOOL}\" -L \"\${binary_name}\""
    "cmake/ZzExpectConfigureFailure.cmake|-DCMAKE_OSX_ARCHITECTURES:STRING="
    "cmake/ZzExpectConfigureFailure.cmake|-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING="
    "cmake/ZzExpectConfigureFailure.cmake|-DCMAKE_OSX_SYSROOT:PATH="
    "cmake/ZzExpectConfigureFailure.cmake|-DCMAKE_C_COMPILER:FILEPATH="
    "tests/Release/CMakeLists.txt|-DZZ_OSX_ARCHITECTURES="
    "tests/Release/CMakeLists.txt|-DZZ_OSX_DEPLOYMENT_TARGET="
    "tests/Release/CMakeLists.txt|-DZZ_OSX_SYSROOT="
    "ZzWindowKit/src/private/ZzQWindowKitBackend.cpp|requires the cocoa Qt platform"
    "ZzWindowKit/tests/CMakeLists.txt|windowkit.offscreen-rejected"
    "ZzWindowKit/tests/CMakeLists.txt|QT_QPA_PLATFORM=cocoa"
    "ZzPureTools/tests/CMakeLists.txt|QT_QPA_PLATFORM=cocoa"
    "docs/third-party/THIRD_PARTY_NOTICES.md|GCC Runtime Library Exception")

set(performance_helper
    "${ZZ_SOURCE_DIR}/scripts/ci/run-linux-performance-gates.sh")
if(NOT EXISTS "${performance_helper}" OR IS_DIRECTORY "${performance_helper}" OR
   IS_SYMLINK "${performance_helper}")
    message(FATAL_ERROR
        "Linux performance helper must be an existing non-empty regular file")
endif()
file(SIZE "${performance_helper}" performance_helper_size)
if(performance_helper_size EQUAL 0)
    message(FATAL_ERROR "Linux performance helper must not be empty")
endif()

set(startup_stability_probe
    "${ZZ_SOURCE_DIR}/scripts/ci/run-linux-startup-stability-probe.sh")
if(NOT EXISTS "${startup_stability_probe}"
   OR IS_DIRECTORY "${startup_stability_probe}"
   OR IS_SYMLINK "${startup_stability_probe}")
    message(FATAL_ERROR
        "Linux startup stability probe must be an existing regular non-symlink file")
endif()
file(SIZE "${startup_stability_probe}" startup_stability_probe_size)
if(startup_stability_probe_size EQUAL 0)
    message(FATAL_ERROR "Linux startup stability probe must not be empty")
endif()

set(governor_experiment
    "${ZZ_SOURCE_DIR}/scripts/ci/run-linux-startup-governor-experiment.sh")
if(NOT EXISTS "${governor_experiment}"
   OR IS_DIRECTORY "${governor_experiment}"
   OR IS_SYMLINK "${governor_experiment}")
    message(FATAL_ERROR
        "Linux governor experiment must be an existing regular non-symlink file")
endif()
file(SIZE "${governor_experiment}" governor_experiment_size)
if(governor_experiment_size EQUAL 0)
    message(FATAL_ERROR "Linux governor experiment must not be empty")
endif()

foreach(requirement IN LISTS required_tokens)
    if(NOT requirement MATCHES "^([^|]+)\\|(.*)$")
        message(FATAL_ERROR "Invalid runner contract entry: ${requirement}")
    endif()
    set(relative_path "${CMAKE_MATCH_1}")
    set(required_token "${CMAKE_MATCH_2}")
    set(file_path "${ZZ_SOURCE_DIR}/${relative_path}")
    if(NOT EXISTS "${file_path}" OR IS_DIRECTORY "${file_path}")
        message(FATAL_ERROR "Required runner file is absent: ${relative_path}")
    endif()
    file(SIZE "${file_path}" file_size)
    if(file_size EQUAL 0)
        message(FATAL_ERROR "Required runner file is empty: ${relative_path}")
    endif()
    file(READ "${file_path}" file_content)
    string(FIND "${file_content}" "${required_token}" token_position)
    if(token_position EQUAL -1)
        message(FATAL_ERROR
            "${relative_path} is missing required token: ${required_token}")
    endif()
endforeach()

file(READ "${ZZ_SOURCE_DIR}/scripts/ci/run-linux-gates.sh" linux_gates_content)
if(linux_gates_content MATCHES "performance_scenarios[ \t]*\\(")
    message(FATAL_ERROR
        "run-linux-gates.sh must not retain the legacy single-round performance_scenarios block")
endif()
string(REGEX MATCHALL
    "cmake --preset \\\"\\$preset\\\" -DZZ_BUILD_EXAMPLES=ON -DCMAKE_BUILD_WITH_INSTALL_RPATH=OFF"
    linux_preset_configurations "${linux_gates_content}")
list(LENGTH linux_preset_configurations linux_preset_configuration_count)
if(NOT linux_preset_configuration_count EQUAL 2)
    message(FATAL_ERROR
        "run-linux-gates.sh must configure run_preset and clang-tidy presets with build-tree RPATH")
endif()

file(READ "${performance_helper}" performance_helper_content)
string(FIND "${performance_helper_content}"
    "grep -Fx 'ZZ_PERFORMANCE_REFERENCE:BOOL=ON'" cache_check_position)
string(FIND "${performance_helper_content}" "rm -- " report_delete_position)
string(FIND "${performance_helper_content}" "ctest --preset linux-gcc-benchmarks"
    benchmark_ctest_position)
if(cache_check_position EQUAL -1 OR report_delete_position EQUAL -1 OR
   benchmark_ctest_position EQUAL -1 OR
   cache_check_position GREATER report_delete_position OR
   cache_check_position GREATER benchmark_ctest_position)
    message(FATAL_ERROR
        "Linux performance helper must verify ZZ_PERFORMANCE_REFERENCE before report deletion and CTest")
endif()

file(READ "${startup_stability_probe}" startup_stability_probe_content)
foreach(forbidden_token IN ITEMS
    "sudo"
    "cpupower"
    "powerprofilesctl set"
    "scaling_governor")
    string(FIND "${startup_stability_probe_content}"
        "${forbidden_token}" forbidden_position)
    if(NOT forbidden_position EQUAL -1)
        message(FATAL_ERROR
            "Linux startup stability probe contains forbidden token: ${forbidden_token}")
    endif()
endforeach()

file(READ "${governor_experiment}" governor_experiment_content)
string(FIND "${governor_experiment_content}"
    "trap finish EXIT" finish_trap_position)
string(FIND "${governor_experiment_content}"
    "zz_governor_apply" governor_apply_position)
if(finish_trap_position EQUAL -1 OR governor_apply_position EQUAL -1
   OR finish_trap_position GREATER governor_apply_position)
    message(FATAL_ERROR
        "Linux governor experiment must install its EXIT trap before applying state")
endif()

string(FIND "${governor_experiment_content}" "setpriv --reuid" setpriv_position)
string(FIND "${governor_experiment_content}" "--regid" setpriv_regid_position)
string(FIND "${governor_experiment_content}" "--init-groups" setpriv_groups_position)
string(FIND "${governor_experiment_content}" "clear_phase_evidence"
    clear_phase_evidence_position)
string(FIND "${governor_experiment_content}" "runuser -u" runuser_position)
string(FIND "${governor_experiment_content}"
    "--phase control" control_phase_position)
string(FIND "${governor_experiment_content}"
    "--phase performance" performance_phase_position)
if(setpriv_position EQUAL -1 OR setpriv_regid_position EQUAL -1
   OR setpriv_groups_position EQUAL -1 OR clear_phase_evidence_position EQUAL -1
   OR runuser_position GREATER -1 OR control_phase_position EQUAL -1
   OR performance_phase_position EQUAL -1
   OR setpriv_position GREATER control_phase_position
   OR setpriv_position GREATER performance_phase_position
   OR clear_phase_evidence_position GREATER control_phase_position
   OR clear_phase_evidence_position GREATER performance_phase_position)
    message(FATAL_ERROR
        "Linux governor experiment must use setpriv ordinary-user execution before both phases")
endif()

foreach(forbidden_root_token IN ITEMS
    "ZzStartupBenchmark"
    "Xvfb"
    "powerprofilesctl set")
    string(FIND "${governor_experiment_content}"
        "${forbidden_root_token}" forbidden_root_position)
    if(NOT forbidden_root_position EQUAL -1)
        message(FATAL_ERROR
            "Linux governor experiment contains forbidden root token: ${forbidden_root_token}")
    endif()
endforeach()

message(STATUS "Native gate script contract passed")
