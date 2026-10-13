#!/usr/bin/env bash
# A reproducible recipe, not a promise of bit-identical binaries across hosts.
set -euo pipefail
fail() { printf 'SDK build: %s\n' "$*" >&2; exit 1; }
usage() {
    printf '%s\n' \
        'Usage: build-qt-precision-sdk.sh --work-dir DIR --prefix DIR [options]' \
        'Build patched Qt 6.11.1 on Linux into a NEW prefix outside the repository.' \
        '  --jobs N                 parallel jobs (default: 8)' \
        '  --cc PATH --cxx PATH     C/C++ compiler (defaults: gcc, g++)' \
        '  --dependency-prefix DIR  additional development dependency prefix' \
        '  --with-multimedia        require FFmpeg and PulseAudio; build QtMultimedia' \
        '  --prepare-only          verify/download sources and apply patches only' \
        'Same recipe can resume; a different recipe requires fresh directories.'
}
work= prefix= deps= cc=gcc cxx=g++ jobs=8 multimedia=0 prepare=0
while (($#)); do
    case "$1" in
        --help|-h) usage; exit 0 ;;
        --with-multimedia) multimedia=1; shift ;;
        --prepare-only) prepare=1; shift ;;
        --work-dir|--prefix|--dependency-prefix|--cc|--cxx|--jobs)
            (($# >= 2)) && [[ -n "$2" && "$2" != --* ]] || fail "$1 requires a value"
            case "$1" in
                --work-dir) work=$2 ;; --prefix) prefix=$2 ;;
                --dependency-prefix) deps=$2 ;; --cc) cc=$2 ;;
                --cxx) cxx=$2 ;; --jobs) jobs=$2 ;;
            esac
            shift 2 ;;
        *) fail "unknown option: $1" ;;
    esac
done
[[ -n "$work" && -n "$prefix" ]] || fail '--work-dir and --prefix are required'
[[ "$jobs" =~ ^[1-9][0-9]*$ ]] || fail '--jobs must be a positive integer'
[[ $(uname -s) == Linux ]] || fail 'only Linux is supported'
for tool in realpath sha256sum curl tar git cmake ninja pkg-config flock find sort xargs cmp; do
    command -v "$tool" >/dev/null || fail "missing tool: $tool"
done
cc=$(command -v "$cc") || fail 'C compiler not found'
cxx=$(command -v "$cxx") || fail 'C++ compiler not found'
cc=$(realpath "$cc"); cxx=$(realpath "$cxx")
repo=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)
# In a linked worktree, also protect the main checkout and its existing SDK builds.
common=$(git -C "$repo" rev-parse --path-format=absolute --git-common-dir)
main_repo=$(dirname "$common")
work=$(realpath -m -- "$work"); prefix=$(realpath -m -- "$prefix")
if [[ -n "$deps" ]]; then
    deps=$(realpath -e -- "$deps") || fail 'dependency prefix does not exist'
fi
for path in "$work" "$prefix" "$deps"; do
    [[ "$path" != *$'\n'* && "$path" != *';'* ]] || fail 'paths must not contain newlines or semicolons'
done
within() { [[ "$1" == "$2" || "$1" == "$2/"* ]]; }
for source_root in "$repo" "$main_repo"; do
    if within "$prefix" "$source_root" || within "$source_root" "$prefix"; then
        fail 'installation prefix must be outside the repository and its ancestors'
    fi
    within "$source_root" "$work" && fail 'work directory must not contain the repository'
done
[[ "$work" != / && "$prefix" != / ]] || fail 'root directories are not allowed'
if within "$work" "$prefix" || within "$prefix" "$work"; then
    fail 'work and installation directories must not overlap'
fi
if [[ -n "$deps" ]] && { within "$deps" "$work" || within "$deps" "$prefix"; }; then
    fail 'dependency prefix must not be inside work or installation directories'
fi
patches=(
    "$repo/docs/performance/evidence/qt-timer-precision/2026-10-12/qtbase-6.11.1-timer-wait.patch"
    "$repo/docs/performance/evidence/qt-glib-precision/2026-10-12/qtbase-6.11.1-glib-ready-time.patch"
)
patch_hashes=(2231fd6b0090900b0031d018ba41b6886dbc815628151123c48b3d0a69b51962 a73fb8eda0e1eb4a965d0e5575283e1ef4714d6bdcca624ce7fdab782f5d403e)
for i in "${!patches[@]}"; do
    actual=$(sha256sum "${patches[i]}"); actual=${actual%% *}
    [[ "$actual" == "${patch_hashes[i]}" ]] || fail "patch SHA-256 mismatch: ${patches[i]}"
done
recipe=$(mktemp -t zz-sdk-recipe-XXXXXXXX)
trap 'rm -f -- "$recipe"' EXIT
{
    printf 'qt=6.11.1\nwork=%q\nprefix=%q\ndependencies=%q\nmultimedia=%s\n' "$work" "$prefix" "$deps" "$multimedia"
    printf 'cc=%q\ncxx=%q\n' "$cc" "$cxx"
    "$cc" --version
    "$cxx" --version
    cmake --version
    sha256sum "${BASH_SOURCE[0]}" "$repo/scripts/sdk/qt-6.11.1.sha256" "${patches[@]}" | cut -d ' ' -f1
} >"$recipe"
validate_directory() {
    local dir=$1 unexpected
    [[ ! -L "$dir/.zz-sdk-lock" && ! -L "$dir/.zz-sdk-recipe" ]] || fail "SDK control file is a symbolic link: $dir"
    [[ ! -e "$dir/.zz-sdk-lock" || -f "$dir/.zz-sdk-lock" ]] || fail "invalid SDK lock file: $dir"
    if [[ -f "$dir/.zz-sdk-recipe" ]]; then
        cmp -s "$recipe" "$dir/.zz-sdk-recipe" || fail "recipe changed; use new directories: $dir"
    else
        # Allow preseeded archives for offline builds, but never adopt arbitrary build/install trees.
        unexpected=$(find "$dir" -mindepth 1 -maxdepth 1 ! -name .zz-sdk-lock)
        if [[ "$dir" == "$work" ]]; then
            unexpected=$(find "$dir" -mindepth 1 -maxdepth 1 ! -name .zz-sdk-lock ! -name downloads)
        fi
        [[ -z "$unexpected" ]] || fail "directory is not empty and is not owned by this recipe: $dir"
    fi
}
for dir in "$work" "$prefix"; do
    [[ ! -d "$dir" ]] || validate_directory "$dir"
done
for child in downloads src build logs; do
    [[ ! -L "$work/$child" ]] || fail "managed directory is a symbolic link: $work/$child"
done
mkdir -p -- "$work" "$prefix"
exec 8>>"$work/.zz-sdk-lock"
flock -n 8 || fail 'work directory is in use'
exec 9>>"$prefix/.zz-sdk-lock"
flock -n 9 || fail 'installation prefix is in use'
# Recheck after acquiring both locks; another invocation may have initialized them.
validate_directory "$work"
validate_directory "$prefix"
cp -- "$recipe" "$work/.zz-sdk-recipe"
cp -- "$recipe" "$prefix/.zz-sdk-recipe"
mkdir -p "$work/downloads" "$work/src" "$work/logs"
# Each attempt keeps its own logs; incomplete runs never acquire a completion stamp.
run=$(mktemp -d "$work/logs/run-XXXXXXXX")
cp "$recipe" "$run/recipe.txt"
log() {
    local name=$1; shift
    printf '%q ' "$@" >>"$run/commands.txt"
    printf '\n' >>"$run/commands.txt"
    "$@" 2>&1 | tee "$run/$name.log"
}
modules=(qtbase qtsvg qttools)
((multimedia == 0)) || modules+=(qtshadertools qtmultimedia)
tree_digest() {
    (cd "$1" && find . -type f -print0 | sort -z | xargs -0 sha256sum) | sha256sum | cut -d ' ' -f1
}
for module in "${modules[@]}"; do
    archive="$module-everywhere-src-6.11.1.tar.xz"
    expected=$(awk -v name="$archive" '$2 == name { print $1 }' "$repo/scripts/sdk/qt-6.11.1.sha256")
    [[ "$expected" =~ ^[0-9a-f]{64}$ ]] || fail "missing locked digest for $archive"
    cached="$work/downloads/$archive"
    if [[ ! -f "$cached" ]]; then
        log "download-$module" curl --fail --location --retry 3 --connect-timeout 30 \
            --proto '=https' --proto-redir '=https' \
            "https://download.qt.io/archive/qt/6.11/6.11.1/submodules/$archive" -o "$cached.part"
        actual=$(sha256sum "$cached.part"); actual=${actual%% *}
        [[ "$actual" == "$expected" ]] || fail "SHA-256 mismatch: $cached.part"
        mv "$cached.part" "$cached"
    fi
    actual=$(sha256sum "$cached"); actual=${actual%% *}
    [[ "$actual" == "$expected" ]] || fail "SHA-256 mismatch: $cached"
    printf '%s  %s\n' "$actual" "$archive" >>"$run/sources.sha256"
    source_dir="$work/src/$module"
    if [[ ! -d "$source_dir" ]]; then
        staging=$(mktemp -d "$work/src/$module-XXXXXXXX")
        log "extract-$module" tar -xJf "$cached" -C "$staging" --strip-components=1
        if [[ "$module" == qtbase ]]; then
            for patch in "${patches[@]}"; do
                log "patch-$(basename "$patch")" git -C "$staging" apply "$patch"
            done
        fi
        tree_digest "$staging" >"$work/src/$module.sha256"
        mv "$staging" "$source_dir"
    else
        [[ -f "$work/src/$module.sha256" ]] || fail "unverified source directory: $source_dir"
        [[ $(tree_digest "$source_dir") == "$(<"$work/src/$module.sha256")" ]] || fail "source tree changed: $source_dir"
    fi
done
if ((prepare)); then
    printf 'Sources prepared and verified. Evidence: %s\n' "$run"
    exit 0
fi
prefix_path=$prefix
[[ -z "$deps" ]] || prefix_path+=";$deps"
common_args=(-G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_C_COMPILER=$cc" "-DCMAKE_CXX_COMPILER=$cxx"
    "-DCMAKE_INSTALL_PREFIX=$prefix" "-DCMAKE_PREFIX_PATH=$prefix_path"
    -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF)
for module in "${modules[@]}"; do
    options=()
    case "$module" in
        qtbase) options=(-DFEATURE_glib=ON -DFEATURE_xcb=ON -DFEATURE_xkbcommon_x11=ON
            -DFEATURE_xcb_egl_plugin=ON -DFEATURE_xcb_glx_plugin=OFF) ;;
        qttools) options=(-DFEATURE_linguist=ON -DFEATURE_assistant=OFF -DFEATURE_designer=OFF
            -DFEATURE_qdoc=OFF -DFEATURE_distancefieldgenerator=OFF -DFEATURE_pixeltool=OFF) ;;
        qtmultimedia) options=(-DFEATURE_ffmpeg=ON -DFEATURE_pulseaudio=ON -DFEATURE_gstreamer=OFF) ;;
    esac
    log "configure-$module" cmake -S "$work/src/$module" -B "$work/build/$module" "${common_args[@]}" "${options[@]}"
    log "build-$module" cmake --build "$work/build/$module" --parallel "$jobs"
    log "install-$module" cmake --install "$work/build/$module"
    cp "$work/build/$module/CMakeCache.txt" "$run/$module-CMakeCache.txt"
done
{
    uname -a
    [[ ! -f /etc/os-release ]] || cat /etc/os-release
    pkg-config --modversion glib-2.0
    [[ ! -f "$deps/../packages.sha256" ]] || cat "$deps/../packages.sha256"
} >"$run/environment.txt"
find "$prefix/lib" -type f -name '*.so*' -print0 | sort -z | xargs -0 sha256sum >"$run/libraries.sha256"
printf 'SDK build completed; native and framework tests must be run separately.\n' >"$run/complete"
printf 'SDK installed at %s\nEvidence: %s\n' "$prefix" "$run"
