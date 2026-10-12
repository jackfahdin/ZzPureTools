#!/usr/bin/env bash

# Formal reports do not fingerprint these diagnostic runtime overrides.
# Refuse them instead of silently collecting a different rendering/timer setup.
zz_require_default_performance_runtime() {
  local override_name
  for override_name in QT_NO_GLIB LD_PRELOAD; do
    if [[ -n "${!override_name:-}" ]]; then
      echo "formal performance runs require the default Qt runtime; unset $override_name" >&2
      return 64
    fi
  done
  # Qt checks presence, not value, for this switch (even an empty value disables SHM).
  if [[ ${QT_XCB_NO_MITSHM+x} == x ]]; then
    echo "formal performance runs require XCB shared memory defaults; unset QT_XCB_NO_MITSHM" >&2
    return 64
  fi
  return 0
}
