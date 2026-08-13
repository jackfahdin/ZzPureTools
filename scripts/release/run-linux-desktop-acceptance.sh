#!/usr/bin/env bash
set -euo pipefail

require_x11_output() {
  command -v xrandr >/dev/null || {
    echo "xrandr is required to verify a physical X11 output" >&2
    return 69
  }
  local monitor_summary
  monitor_summary=$(xrandr --listmonitors 2>&1) || {
    echo "xrandr could not query the active X11 outputs" >&2
    return 65
  }
  local monitor_count
  monitor_count=$(sed -n 's/^Monitors:[[:space:]]*//p' <<<"$monitor_summary")
  [[ "$monitor_count" =~ ^[1-9][0-9]*$ ]] || {
    echo "the X11 session exposes no active physical output" >&2
    return 65
  }
}

require_wayland_output() {
  command -v wayland-info >/dev/null || {
    echo "wayland-info is required to verify a physical Wayland output" >&2
    return 69
  }
  local registry
  registry=$(wayland-info 2>&1) || {
    echo "wayland-info could not query the active Wayland compositor" >&2
    return 65
  }
  [[ "$registry" == *"interface: 'wl_output'"* ]] || {
    echo "the Wayland compositor exposes no wl_output" >&2
    return 65
  }
}

require_local_desktop_session() {
  [[ -n "${XDG_SESSION_ID:-}" ]] || {
    echo "XDG_SESSION_ID is required to identify the physical desktop session" >&2
    return 65
  }

  local login_type login_remote login_active login_desktop
  login_type=$(loginctl show-session "$XDG_SESSION_ID" -p Type --value) || {
    echo "loginctl could not query session $XDG_SESSION_ID" >&2
    return 65
  }
  login_remote=$(loginctl show-session "$XDG_SESSION_ID" -p Remote --value)
  login_active=$(loginctl show-session "$XDG_SESSION_ID" -p Active --value)
  login_desktop=$(loginctl show-session "$XDG_SESSION_ID" -p Desktop --value)

  [[ "$login_remote" == no && "$login_active" == yes ]] || {
    echo "desktop acceptance requires a local active login session" >&2
    return 65
  }
  [[ "$login_type" == "$session_type" ]] || {
    echo "XDG session type does not match loginctl: $session_type != $login_type" >&2
    return 65
  }

  local login_desktop_lower=${login_desktop,,}
  [[ -n "$login_desktop_lower"
    && ("$desktop_name_lower" == *"$login_desktop_lower"*
      || "$login_desktop_lower" == *"$desktop_name_lower"*) ]] || {
    echo "XDG desktop does not match loginctl: $desktop_name != $login_desktop" >&2
    return 65
  }
}

usage() {
  cat <<'EOF'
usage: run-linux-desktop-acceptance.sh --session <id> --build-dir <path>

session ids:
  linux-x11-kde
  linux-x11-gnome
  linux-wayland-kde
  linux-wayland-gnome
  linux-qt-fallback

The build directory must be below this repository's build directory and must
already contain ZzPureToolsExample. The fallback session additionally requires
ZZ_WINDOWKIT_FORCE_QT_CONTEXT:BOOL=ON in CMakeCache.txt.

The tracked source tree must be clean. The only permitted untracked input is
the exact top-level temp_image/ directory; the script records but never reads,
copies, hashes, or commits that directory.
EOF
}

session_id=
build_dir_argument=
while [[ $# -gt 0 ]]; do
  case "$1" in
    --session)
      [[ $# -ge 2 ]] || {
        usage >&2
        exit 64
      }
      session_id=$2
      shift 2
      ;;
    --build-dir)
      [[ $# -ge 2 ]] || {
        usage >&2
        exit 64
      }
      build_dir_argument=$2
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 64
      ;;
  esac
done

case "$session_id" in
  linux-x11-kde|linux-x11-gnome|linux-wayland-kde|linux-wayland-gnome|linux-qt-fallback)
    ;;
  *)
    echo "unsupported or missing session id: $session_id" >&2
    usage >&2
    exit 64
    ;;
esac
[[ -n "$build_dir_argument" ]] || {
  echo "--build-dir is required" >&2
  exit 64
}

for tool in bash cmake file git ldd loginctl realpath sed sha256sum tee; do
  command -v "$tool" >/dev/null || {
    echo "required tool is unavailable: $tool" >&2
    exit 69
  }
done
[[ -n "${QT_ROOT:-}" && -d "$QT_ROOT" ]] || {
  echo "QT_ROOT must identify the Qt SDK used by the build" >&2
  exit 64
}

source_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd -P)
cd "$source_dir"
status_output=$(git status --porcelain --untracked-files=normal)
local_untracked_inputs=none
unexpected_status=
while IFS= read -r status_line; do
  [[ -n "$status_line" ]] || continue
  if [[ "$status_line" == "?? temp_image/" ]]; then
    local_untracked_inputs=temp_image/
    continue
  fi
  unexpected_status+="${status_line}"$'\n'
done <<<"$status_output"
[[ -z "$unexpected_status" ]] || {
  echo "desktop acceptance requires a clean worktree" >&2
  printf '%s' "$unexpected_status" >&2
  exit 65
}

build_dir=$(realpath -- "$build_dir_argument")
case "$build_dir/" in
  "$source_dir/build/"*) ;;
  *)
    echo "build directory must be below $source_dir/build" >&2
    exit 64
    ;;
esac
cache_file="$build_dir/CMakeCache.txt"
binary="$build_dir/examples/ZzPureToolsExample/ZzPureToolsExample"
[[ -f "$cache_file" && -x "$binary" ]] || {
  echo "build directory lacks CMakeCache.txt or ZzPureToolsExample" >&2
  exit 66
}

qt_root=$(realpath -- "$QT_ROOT")
qt_cmake_dir=$(sed -n 's/^Qt6_DIR:PATH=//p' "$cache_file")
build_type=$(sed -n 's/^CMAKE_BUILD_TYPE:STRING=//p' "$cache_file")
shared_libraries=$(sed -n 's/^BUILD_SHARED_LIBS:BOOL=//p' "$cache_file")
case "$qt_cmake_dir/" in
  "$qt_root/"*) ;;
  *)
    echo "QT_ROOT does not match the Qt SDK recorded by CMake" >&2
    exit 65
    ;;
esac
[[ "$build_type" == Release
  && "$shared_libraries" =~ ^(1|ON|TRUE|YES)$ ]] || {
  echo "desktop acceptance requires a shared Release build" >&2
  exit 65
}

fallback_value=$(sed -n \
  's/^ZZ_WINDOWKIT_FORCE_QT_CONTEXT:BOOL=//p' "$cache_file")
if [[ "$session_id" == linux-qt-fallback ]]; then
  [[ "$fallback_value" == ON ]] || {
    echo "linux-qt-fallback requires ZZ_WINDOWKIT_FORCE_QT_CONTEXT=ON" >&2
    exit 65
  }
elif [[ "$fallback_value" != OFF ]]; then
  echo "native desktop sessions require ZZ_WINDOWKIT_FORCE_QT_CONTEXT=OFF" >&2
  exit 65
fi

session_type=${XDG_SESSION_TYPE:-}
desktop_name=${XDG_CURRENT_DESKTOP:-}
desktop_name_lower=${desktop_name,,}
require_local_desktop_session
case "$session_id" in
  linux-x11-kde)
    [[ "$session_type" == x11 && "$desktop_name_lower" == *kde* ]] || {
      echo "linux-x11-kde requires an X11 KDE session" >&2
      exit 65
    }
    [[ -n "${DISPLAY:-}" ]] || {
      echo "DISPLAY is unavailable" >&2
      exit 65
    }
    require_x11_output
    export QT_QPA_PLATFORM=xcb
    ;;
  linux-x11-gnome)
    [[ "$session_type" == x11 && "$desktop_name_lower" == *gnome* ]] || {
      echo "linux-x11-gnome requires an X11 GNOME session" >&2
      exit 65
    }
    [[ -n "${DISPLAY:-}" ]] || {
      echo "DISPLAY is unavailable" >&2
      exit 65
    }
    require_x11_output
    export QT_QPA_PLATFORM=xcb
    ;;
  linux-wayland-kde)
    [[ "$session_type" == wayland && "$desktop_name_lower" == *kde* ]] || {
      echo "linux-wayland-kde requires a Wayland KDE session" >&2
      exit 65
    }
    [[ -n "${WAYLAND_DISPLAY:-}" && -n "${XDG_RUNTIME_DIR:-}" ]] || {
      echo "Wayland display variables are unavailable" >&2
      exit 65
    }
    require_wayland_output
    export QT_QPA_PLATFORM=wayland
    ;;
  linux-wayland-gnome)
    [[ "$session_type" == wayland && "$desktop_name_lower" == *gnome* ]] || {
      echo "linux-wayland-gnome requires a Wayland GNOME session" >&2
      exit 65
    }
    [[ -n "${WAYLAND_DISPLAY:-}" && -n "${XDG_RUNTIME_DIR:-}" ]] || {
      echo "Wayland display variables are unavailable" >&2
      exit 65
    }
    require_wayland_output
    export QT_QPA_PLATFORM=wayland
    ;;
  linux-qt-fallback)
    case "$session_type" in
      x11)
        [[ -n "${DISPLAY:-}" ]] || {
          echo "DISPLAY is unavailable" >&2
          exit 65
        }
        require_x11_output
        export QT_QPA_PLATFORM=xcb
        ;;
      wayland)
        [[ -n "${WAYLAND_DISPLAY:-}" && -n "${XDG_RUNTIME_DIR:-}" ]] || {
          echo "Wayland display variables are unavailable" >&2
          exit 65
        }
        require_wayland_output
        export QT_QPA_PLATFORM=wayland
        ;;
      *)
        echo "linux-qt-fallback requires a real X11 or Wayland session" >&2
        exit 65
        ;;
    esac
    ;;
esac

commit=$(git rev-parse --verify HEAD)
short_commit=$(git rev-parse --short=12 HEAD)
timestamp=$(date -u +%Y%m%dT%H%M%SZ)
evidence_root="$source_dir/build/gate-evidence/linux-desktop/${timestamp}-${session_id}-${short_commit}"
[[ ! -e "$evidence_root" ]] || {
  echo "evidence directory already exists: $evidence_root" >&2
  exit 73
}
mkdir -p "$evidence_root/screenshots"
host_log="$evidence_root/host.log"
build_log="$evidence_root/build.log"
app_log="$evidence_root/application.log"
report_path="$evidence_root/RESULT_ZH.md"

echo "Updating ZzPureToolsExample from the clean source commit"
cmake --build "$build_dir" --target ZzPureToolsExample --parallel 2 \
  2>&1 | tee "$build_log"

compiler=$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "$cache_file")
{
  echo "session.id=$session_id"
  echo "session.loginId=$XDG_SESSION_ID"
  echo "session.type=$session_type"
  echo "session.desktop=$desktop_name"
  echo "session.qtQpaPlatform=$QT_QPA_PLATFORM"
  echo "session.display=${DISPLAY:-}"
  echo "session.waylandDisplay=${WAYLAND_DISPLAY:-}"
  echo "source.commit=$commit"
  echo "source.status=tracked-clean"
  echo "source.localUntrackedInputs=$local_untracked_inputs"
  echo "build.directory=$build_dir"
  echo "build.forceQtContext=$fallback_value"
  echo "build.type=$build_type"
  echo "build.sharedLibraries=$shared_libraries"
  echo "qt.root=$qt_root"
  echo "qt.cmakeDirectory=$qt_cmake_dir"
  echo "binary.path=$binary"
  sha256sum "$binary"
  file "$binary"
  uname -a
  if [[ -f /etc/os-release ]]; then
    cat /etc/os-release
  fi
  if [[ -n "$compiler" && -x "$compiler" ]]; then
    "$compiler" --version
  fi
  if [[ -x "$qt_root/bin/qtpaths" ]]; then
    "$qt_root/bin/qtpaths" --qt-version
  elif [[ -x "$qt_root/bin/qtpaths6" ]]; then
    "$qt_root/bin/qtpaths6" --qt-version
  fi
  ldd "$binary"
  if command -v loginctl >/dev/null && [[ -n "${XDG_SESSION_ID:-}" ]]; then
    loginctl show-session "$XDG_SESSION_ID" --all
  fi
  if command -v xrandr >/dev/null && [[ -n "${DISPLAY:-}" ]]; then
    xrandr --listmonitors
    xrandr --current
  fi
  if command -v kscreen-doctor >/dev/null; then
    kscreen-doctor -o
  fi
  if command -v wayland-info >/dev/null && [[ "$session_type" == wayland ]]; then
    wayland-info
  fi
} >"$host_log" 2>&1

binary_digest=$(sha256sum "$binary" | sed 's/[[:space:]].*//')
cat >"$report_path" <<EOF
# Linux 桌面会话验收记录

状态: 待人工填写
测试日期: $timestamp
测试人员:
会话 ID: $session_id
桌面/协议: $desktop_name / $session_type
源码 commit: $commit
构建目录: $build_dir
二进制 SHA-256: $binary_digest
主机日志: $host_log
构建日志: $build_log
应用日志: $app_log
截图目录: $evidence_root/screenshots
问题链接:

## 人工检查

| 检查项 | 实际结果 | 截图/日志 | 问题链接 |
|---|---|---|---|
| 应用启动、十二路由与退出 |  |  |  |
| 标题栏拖动与交互区排除 |  |  |  |
| 四边与四角 resize |  |  |  |
| 双击最大化与还原 |  |  |  |
| 系统菜单或明确降级 |  |  |  |
| 最小化与恢复 |  |  |  |
| 返回、前进与搜索导航 |  |  |  |
| 活动 Dock 与状态栏 |  |  |  |
| 新建窗口与窗口状态隔离 |  |  |  |
| Light、Dark、HighContrast |  |  |  |
| 100% 与高 DPI/跨显示器 |  |  |  |
| 键盘导航与焦点视觉 |  |  |  |
| Orca 可访问名称、角色与状态 |  |  |  |
| 关闭取消、最小化与确认 |  |  |  |
| 连续创建关闭窗口 |  |  |  |
| fallback 能力页确认不使用 native 后端 |  |  |  |

## 结论

结果:
阻断问题:
测试人员签名:
签署日期:
EOF

echo "Launching $binary"
echo "Complete the manual checks, save screenshots under:"
echo "  $evidence_root/screenshots"
echo "Exit through the application's confirmed close path when finished."
set +e
"$binary" 2>&1 | tee "$app_log"
application_status=${PIPESTATUS[0]}
set -e

{
  echo
  echo "## 自动退出记录"
  echo
  echo "应用退出码: $application_status"
} >>"$report_path"
printf '%s\t%s\t%s\t%s\n' \
  "$timestamp" "$session_id" "$commit" "$evidence_root" \
  >>"$source_dir/build/gate-evidence/linux-native.log"

echo "Evidence saved to: $evidence_root"
echo "Fill in: $report_path"
[[ $application_status -eq 0 ]] || exit "$application_status"
