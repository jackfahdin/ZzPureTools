#!/usr/bin/env bash
set -euo pipefail
fail() { printf 'SDK verification: %s\n' "$*" >&2; exit 1; }
sdk_work= prefix= framework= deps= jobs=8 multimedia=0
while (($#)); do
    case "$1" in
        --help|-h)
            printf '%s\n' \
                'Usage: verify-qt-precision-sdk.sh --sdk-work-dir DIR --prefix DIR [options]' \
                '  --dependency-prefix DIR   same development prefix used for SDK build' \
                '  --jobs N                  build parallelism (default: 8)' \
                '  --with-multimedia         also require all three real/simulated audio tests' \
                '  --framework-build DIR     configured framework build using this SDK'
            exit 0 ;;
        --with-multimedia) multimedia=1; shift ;;
        --sdk-work-dir|--prefix|--framework-build|--dependency-prefix|--jobs)
            (($# >= 2)) && [[ -n "$2" && "$2" != --* ]] || fail "$1 requires a value"
            case "$1" in
                --sdk-work-dir) sdk_work=$2 ;; --prefix) prefix=$2 ;;
                --framework-build) framework=$2 ;; --dependency-prefix) deps=$2 ;; --jobs) jobs=$2 ;;
            esac
            shift 2 ;;
        *) fail "unknown option: $1" ;;
    esac
done
[[ -n "$sdk_work" && -n "$prefix" ]] || fail '--sdk-work-dir and --prefix are required'
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail '--jobs must be a positive integer'
((multimedia == 0)) || [[ -n "$framework" ]] || fail '--with-multimedia requires --framework-build'
sdk_work=$(realpath -m "$sdk_work"); prefix=$(realpath -m "$prefix")
[[ -f "$sdk_work/.zz-sdk-recipe" && -f "$prefix/.zz-sdk-recipe" && -d "$prefix/lib/cmake/Qt6" ]] || fail 'a completed SDK build is required'
cmp -s "$sdk_work/.zz-sdk-recipe" "$prefix/.zz-sdk-recipe" || fail 'SDK work and installation recipes differ'
[[ ! -L "$sdk_work/.zz-sdk-lock" && ! -L "$prefix/.zz-sdk-lock" ]] || fail 'SDK lock must not be a symbolic link'
exec 8>>"$sdk_work/.zz-sdk-lock"
flock -n 8 || fail 'SDK build is in progress'
exec 9>>"$prefix/.zz-sdk-lock"
flock -n 9 || fail 'SDK installation is in use'
latest=$(find "$sdk_work/logs" -mindepth 1 -maxdepth 1 -type d -name 'run-*' -printf '%T@ %p\n' | sort -nr | head -1 | cut -d ' ' -f2-)
[[ -n "$latest" && -f "$latest/complete" ]] || fail 'latest attempt is not a completed SDK build'
sha256sum --check --status "$latest/libraries.sha256" || fail 'installed Qt libraries changed since build'
cache="$sdk_work/build/qtbase/CMakeCache.txt"
cxx=$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$cache")
[[ -x "$cxx" ]] || fail 'SDK compiler is unavailable'
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)
source "$repo/scripts/sdk/ZzQtSdkValidation.sh"
if ((multimedia)); then
    framework=$(realpath -e "$framework")
    grep -Fxq "ZZ_QT_PREFIX:PATH=$prefix" "$framework/CMakeCache.txt" || fail 'framework is not configured for this SDK'
    grep -Fxq "CMAKE_HOME_DIRECTORY:INTERNAL=$repo" "$framework/CMakeCache.txt" || fail 'framework build belongs to another checkout'
    for package in Qt6 Qt6Core Qt6Gui Qt6Widgets Qt6Test Qt6Multimedia; do
        package_dir=$(sed -n "s/^${package}_DIR:[^=]*=//p" "$framework/CMakeCache.txt")
        package_dir=$(realpath -e "$package_dir") || fail "missing package: $package"
        [[ "$package_dir" == "$prefix/"* ]] || fail "foreign Qt package: $package_dir"
    done
    [[ -f "$prefix/lib/cmake/Qt6Multimedia/Qt6MultimediaConfig.cmake" ]] || fail 'QtMultimedia is missing'
fi
prefix_path=$prefix
if [[ -n "$deps" ]]; then
    deps=$(realpath -e "$deps")
    prefix_path+=";$deps"
fi
mkdir -p "$sdk_work/verification"
run=$(mktemp -d "$sdk_work/verification/run-XXXXXXXX")
log() {
    local name=$1; shift
    printf '%q ' "$@" >>"$run/commands.txt"
    printf '\n' >>"$run/commands.txt"
    "$@" 2>&1 | tee "$run/$name.log"
}
cp "$sdk_work/.zz-sdk-recipe" "$run/sdk-recipe.txt"
git -C "$repo" rev-parse HEAD >"$run/framework-commit.txt"
for test in qtimer qchronotimer qeventdispatcher qabstractanimation; do
    group=kernel
    [[ "$test" != qabstractanimation ]] || group=animation
    log "configure-$test" cmake -S "$sdk_work/src/qtbase/tests/auto/corelib/$group/$test" \
        -B "$sdk_work/verification/build/$test" -G Ninja -DCMAKE_BUILD_TYPE=Release \
        "-DCMAKE_CXX_COMPILER=$cxx" "-DCMAKE_PREFIX_PATH=$prefix_path"
    log "build-$test" cmake --build "$sdk_work/verification/build/$test" --parallel "$jobs"
    found_test=0
    while IFS= read -r -d '' executable; do
        found_test=1
        sdk_assert_runtime "$prefix" "$executable" "$run/ldd-$(basename "$executable").log" || fail 'native test uses an unexpected Qt runtime'
    done < <(find "$sdk_work/verification/build/$test" -type f -name 'tst_*' -executable -print0)
    ((found_test)) || fail "no native executable built for $test"
    log "test-$test" env -u LD_PRELOAD -u LD_LIBRARY_PATH -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH -u QT_NO_GLIB QT_QPA_PLATFORM=offscreen \
        ctest --test-dir "$sdk_work/verification/build/$test" --no-tests=error --output-on-failure -j1 -V
done
if ((multimedia)); then
    log framework-build cmake --build "$framework" --parallel "$jobs" --target \
        ZzPureToolsExample ZzExampleAudioMeterSourceTest_playback ZzExampleAudioMeterSourceTest_simulation ZzExampleAudioLevelsTest
    for executable in ZzExampleAudioMeterSourceTest_playback ZzExampleAudioMeterSourceTest_simulation ZzExampleAudioLevelsTest; do
        sdk_assert_runtime "$prefix" "$framework/examples/ZzPureToolsExample/tests/$executable" "$run/ldd-$executable.log" || fail 'audio test uses an unexpected Qt runtime'
    done
    sdk_assert_runtime "$prefix" "$framework/examples/ZzPureToolsExample/ZzPureToolsExample" "$run/framework-ldd.log" || fail 'Example uses an unexpected Qt runtime'
    audio_regex='^example.audio-(meter-(simulation|playback)|levels)$'
    ctest --test-dir "$framework" -N -R "$audio_regex" >"$run/audio-list.txt"
    for name in example.audio-meter-simulation example.audio-meter-playback example.audio-levels; do
        grep -Eq ": $name$" "$run/audio-list.txt" || fail "required test is missing: $name"
    done
    log audio-tests env -u LD_PRELOAD -u LD_LIBRARY_PATH -u QT_PLUGIN_PATH -u QT_QPA_PLATFORM_PLUGIN_PATH QT_QPA_PLATFORM=offscreen QT_MEDIA_BACKEND=ffmpeg ZZ_REQUIRE_AUDIO_PLAYBACK=1 \
        ctest --test-dir "$framework" -R "$audio_regex" --no-tests=error --output-on-failure -j1 -V
    sdk_require_audio_playback "$run/audio-tests.log" || fail 'audio playback was skipped or did not execute'
fi
printf 'Requested native and audio checks passed.\n' >"$run/complete"
printf 'Verification evidence: %s\n' "$run"
