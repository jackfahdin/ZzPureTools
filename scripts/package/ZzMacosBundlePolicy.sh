#!/usr/bin/env bash

zz_macos_bundle_policy_dir=$(
  cd -- "$(dirname "${BASH_SOURCE[0]}")" >/dev/null 2>&1
  pwd -P
)
if ! declare -F zz_macos_arch_list_is_exact >/dev/null; then
  # shellcheck source=ZzMacosArchitecturePolicy.sh
  source "$zz_macos_bundle_policy_dir/ZzMacosArchitecturePolicy.sh"
fi
unset zz_macos_bundle_policy_dir

zz_macos_file_mode() {
  local binary=$1
  if [[ $(uname -s) == Darwin ]]; then
    stat -f '%Lp' "$binary"
  else
    stat -c '%a' "$binary"
  fi
}

zz_macos_extract_rpaths() {
  awk '
    $1 == "cmd" && $2 == "LC_RPATH" { want_path = 1; next }
    want_path && $1 == "path" {
      line = $0
      sub(/^[[:space:]]*path[[:space:]]+/, "", line)
      sub(/[[:space:]]+\(offset[[:space:]]+[0-9]+\)[[:space:]]*$/, "", line)
      print line
      want_path = 0
    }
  '
}

zz_macos_path_is_within_roots() {
  local path=$1
  shift
  local root
  for root in "$@"; do
    [[ -n $root ]] || continue
    case $path in
      "$root"|"$root"/*) return 0 ;;
    esac
  done
  return 1
}

zz_macos_thin_bundle() {
  local bundle=$1
  local architecture=$2
  local temporary_root=$3
  local binary description archs temporary mode
  [[ -d $bundle && ! -L $bundle && -d $temporary_root && ! -L $temporary_root ]] || {
    echo "invalid bundle thinning input" >&2
    return 1
  }

  while IFS= read -r -d '' binary; do
    description=$(file -b "$binary")
    case $description in
      *Mach-O*) ;;
      *) continue ;;
    esac

    archs=$(lipo -archs "$binary")
    zz_macos_arch_list_contains "$archs" "$architecture" || {
      echo "missing $architecture architecture in $binary: $archs" >&2
      return 1
    }
    zz_macos_arch_list_is_exact "$archs" "$architecture" && continue

    temporary=$(mktemp "$temporary_root/.zz-thin.XXXXXX")
    mode=$(zz_macos_file_mode "$binary")
    lipo "$binary" -thin "$architecture" -output "$temporary"
    chmod "$mode" "$temporary"
    archs=$(lipo -archs "$temporary")
    zz_macos_arch_list_is_exact "$archs" "$architecture" || {
      echo "failed to thin $binary to $architecture: $archs" >&2
      return 1
    }
    mv -- "$temporary" "$binary"
  done < <(find "$bundle" -type f -print0)
}

zz_macos_strip_transient_rpaths() {
  local bundle=$1
  local architecture=$2
  shift 2
  local -a transient_roots=("$@")
  local binary description load_commands rpath
  [[ -d $bundle && ! -L $bundle && ${#transient_roots[@]} -gt 0 ]] || {
    echo "invalid transient RPATH cleanup input" >&2
    return 1
  }

  while IFS= read -r -d '' binary; do
    description=$(file -b "$binary")
    case $description in
      *Mach-O*) ;;
      *) continue ;;
    esac
    load_commands=$(otool -arch "$architecture" -l "$binary")
    while IFS= read -r rpath; do
      [[ -n $rpath ]] || continue
      if zz_macos_path_is_within_roots "$rpath" "${transient_roots[@]}"; then
        install_name_tool -delete_rpath "$rpath" "$binary"
      fi
    done < <(printf '%s\n' "$load_commands" | zz_macos_extract_rpaths)
  done < <(find "$bundle" -type f -print0)
}

zz_macos_invoke_app_smoke() {
  local bundle=$1
  local executable="$bundle/Contents/MacOS/ZzPureToolsExample"
  local plugin_dir="$bundle/Contents/PlugIns/platforms"
  local cocoa_plugin="$plugin_dir/libqcocoa.dylib"
  [[ -d $bundle && ! -L $bundle &&
     -x $executable && ! -L $executable &&
     -f $cocoa_plugin && ! -L $cocoa_plugin ]] || {
    echo "bundled Cocoa smoke inputs are unavailable: $bundle" >&2
    return 1
  }

  (
    unset QT_PLUGIN_PATH QT_QPA_PLATFORM_PLUGIN_PATH
    unset QML2_IMPORT_PATH QML_IMPORT_PATH
    unset DYLD_LIBRARY_PATH DYLD_FRAMEWORK_PATH
    unset DYLD_FALLBACK_LIBRARY_PATH DYLD_FALLBACK_FRAMEWORK_PATH
    unset DYLD_INSERT_LIBRARIES
    export QT_QPA_PLATFORM=cocoa
    export QT_QPA_PLATFORM_PLUGIN_PATH="$plugin_dir"
    export ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1500
    exec "$executable" --smoke-test
  )
}
