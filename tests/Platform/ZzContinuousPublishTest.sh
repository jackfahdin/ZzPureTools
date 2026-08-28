#!/usr/bin/env bash
set -euo pipefail

if [[ $# -ne 2 ]]; then
  echo "usage: $0 <source-dir> <test-root>" >&2
  exit 64
fi

source_dir=$(realpath "$1")
test_root_input=$2
publish_script="$source_dir/scripts/release/publish-continuous-build.sh"
[[ -f $publish_script && ! -L $publish_script ]] || {
  echo "continuous publish script is missing: $publish_script" >&2
  exit 1
}

mkdir -p "$source_dir/build"
build_root=$(realpath "$source_dir/build")
test_parent=$(dirname "$test_root_input")
mkdir -p "$test_parent"
test_root=$(realpath "$test_parent")/$(basename "$test_root_input")
case "$test_root/" in
  "$build_root"/*) ;;
  *)
    echo "test root must be below the repository build directory" >&2
    exit 1
    ;;
esac
[[ $test_root != "$build_root" ]] || {
  echo "test root must not equal the build directory" >&2
  exit 1
}
rm -rf -- "$test_root"
mkdir -p "$test_root"

commit=0123456789abcdef0123456789abcdef01234567
old_commit=aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa
repository=jackfahdin/ZzPureTools
run_url=https://github.com/jackfahdin/ZzPureTools/actions/runs/123456789
platforms=(
  linux-x86_64
  windows-msvc2022-x86_64
  windows-mingw-x86_64
  macos-arm64
  macos-x86_64
)

extension_for() {
  case $1 in
    linux-x86_64) printf '%s\n' AppImage ;;
    windows-*) printf '%s\n' zip ;;
    macos-*) printf '%s\n' dmg ;;
    *) return 1 ;;
  esac
}

architecture_for() {
  case $1 in
    macos-arm64) printf '%s\n' arm64 ;;
    *) printf '%s\n' x86_64 ;;
  esac
}

write_expected_asset_names() {
  local output=$1
  local expected_commit=$2
  local short_commit=${expected_commit:0:12}
  local platform extension package_name
  : > "$output"
  for platform in "${platforms[@]}"; do
    extension=$(extension_for "$platform")
    package_name="ZzPureToolsExample-continuous-${platform}-${short_commit}.${extension}"
    printf '%s\n' \
      "$package_name" \
      "$package_name.sha256" \
      "$package_name.build-info.json" >> "$output"
  done
  sort -o "$output" "$output"
}

create_artifacts() {
  local root=$1
  local platform extension architecture package_name package_path
  mkdir -p "$root"
  for platform in "${platforms[@]}"; do
    extension=$(extension_for "$platform")
    architecture=$(architecture_for "$platform")
    package_name="ZzPureToolsExample-continuous-${platform}-${commit:0:12}.${extension}"
    package_path="$root/$platform/$package_name"
    mkdir -p "$(dirname "$package_path")"
    printf 'fixture package bytes for %s\n' "$platform" > "$package_path"
    cmake \
      "-DZZ_PACKAGE_PATH=$package_path" \
      "-DZZ_PLATFORM_ID=$platform" \
      "-DZZ_COMMIT=$commit" \
      -DZZ_DIRTY=false \
      -DZZ_BUILT_AT_UTC=2026-08-28T00:00:00Z \
      -DZZ_RUNNER_OS=fixture-os \
      "-DZZ_ARCHITECTURE=$architecture" \
      -DZZ_QT_VERSION=6.8.3 \
      -DZZ_COMPILER_ID=fixture-compiler \
      -DZZ_COMPILER_VERSION=1.0.0 \
      -DZZ_PRESET=fixture-continuous \
      -DZZ_LINKAGE=shared \
      -DZZ_LTO=true \
      -P "$source_dir/scripts/package/WriteBuildInfo.cmake" >/dev/null
  done
}

create_fake_gh() {
  local bin_dir=$1
  mkdir -p "$bin_dir"
  cat > "$bin_dir/gh" <<'FAKE_GH'
#!/usr/bin/env bash
set -euo pipefail

: "${FAKE_GH_LOG:?}"
: "${FAKE_GH_STATE:?}"
mkdir -p "$FAKE_GH_STATE"
{
  printf 'gh'
  for argument in "$@"; do
    printf ' %s' "$argument"
  done
  printf '\n'
} >> "$FAKE_GH_LOG"

argument_after() {
  local wanted=$1
  shift
  while [[ $# -gt 0 ]]; do
    if [[ $1 == "$wanted" ]]; then
      [[ $# -ge 2 ]] || return 1
      printf '%s\n' "$2"
      return 0
    fi
    shift
  done
  return 1
}

if [[ ${1:-} == release ]]; then
  operation=${2:-}
  case $operation in
    view)
      if [[ -n ${FAKE_GH_VIEW_ERROR:-} ]]; then
        printf '%s\n' "$FAKE_GH_VIEW_ERROR" >&2
        exit 1
      fi
      [[ -f $FAKE_GH_STATE/release-exists ]] || {
        echo 'release not found' >&2
        exit 1
      }
      jq_expression=$(argument_after --jq "$@" || true)
      case $jq_expression in
        '.assets[].name') cat "$FAKE_GH_STATE/assets" ;;
        '.targetCommitish') cat "$FAKE_GH_STATE/target" ;;
        *) printf '%s\n' continuous-build ;;
      esac
      ;;
    create)
      [[ ! -f $FAKE_GH_STATE/release-exists ]] || {
        echo 'release already exists' >&2
        exit 1
      }
      target=$(argument_after --target "$@")
      : > "$FAKE_GH_STATE/assets"
      printf '%s\n' "$target" > "$FAKE_GH_STATE/tag-sha"
      printf '%s\n' "$target" > "$FAKE_GH_STATE/target"
      : > "$FAKE_GH_STATE/release-exists"
      ;;
    upload)
      count=0
      [[ ! -f $FAKE_GH_STATE/upload-count ]] ||
        count=$(cat "$FAKE_GH_STATE/upload-count")
      count=$((count + 1))
      printf '%s\n' "$count" > "$FAKE_GH_STATE/upload-count"
      if [[ ${FAKE_GH_FAIL_UPLOAD_AT:-0} -eq $count ]]; then
        echo "simulated upload failure at $count" >&2
        exit 1
      fi
      asset_name=$(basename "$4")
      if grep -Fxq "$asset_name" "$FAKE_GH_STATE/assets"; then
        echo "asset already exists: $asset_name" >&2
        exit 1
      fi
      printf '%s\n' "$asset_name" >> "$FAKE_GH_STATE/assets"
      ;;
    edit)
      target=$(argument_after --target "$@")
      title=$(argument_after --title "$@")
      notes=$(argument_after --notes-file "$@")
      printf '%s\n' "$target" > "$FAKE_GH_STATE/target"
      printf '%s\n' "$title" > "$FAKE_GH_STATE/title"
      cp "$notes" "$FAKE_GH_STATE/notes"
      : > "$FAKE_GH_STATE/prerelease"
      ;;
    delete-asset)
      asset_name=$4
      awk -v rejected="$asset_name" '$0 != rejected { print }' \
        "$FAKE_GH_STATE/assets" > "$FAKE_GH_STATE/assets.next"
      mv "$FAKE_GH_STATE/assets.next" "$FAKE_GH_STATE/assets"
      ;;
    delete)
      rm -f "$FAKE_GH_STATE/release-exists" \
        "$FAKE_GH_STATE/assets" "$FAKE_GH_STATE/tag-sha" \
        "$FAKE_GH_STATE/target" "$FAKE_GH_STATE/title" \
        "$FAKE_GH_STATE/notes" "$FAKE_GH_STATE/prerelease"
      ;;
    *)
      echo "unsupported fake gh release operation: $operation" >&2
      exit 2
      ;;
  esac
elif [[ ${1:-} == api ]]; then
  method=GET
  route=
  previous=
  for argument in "$@"; do
    if [[ $previous == --method ]]; then
      method=$argument
    elif [[ $argument == repos/* ]]; then
      route=$argument
    fi
    previous=$argument
  done
  [[ $route == */git/ref/tags/continuous-build ||
     $route == */git/refs/tags/continuous-build ]] || {
    echo "unsupported fake gh api route: $route" >&2
    exit 2
  }
  if [[ $method == PATCH ]]; then
    sha=$(argument_after -f "$@")
    sha=${sha#sha=}
    printf '%s\n' "$sha" > "$FAKE_GH_STATE/tag-sha"
  else
    cat "$FAKE_GH_STATE/tag-sha"
  fi
else
  echo "unsupported fake gh command: ${1:-}" >&2
  exit 2
fi
FAKE_GH
  chmod 755 "$bin_dir/gh"
}

initialize_existing_release() {
  local state=$1
  mkdir -p "$state"
  : > "$state/release-exists"
  printf '%s\n' "$old_commit" > "$state/tag-sha"
  printf '%s\n' "$old_commit" > "$state/target"
  write_expected_asset_names "$state/assets" "$old_commit"
}

run_publish() {
  local case_root=$1
  local artifact_root=$2
  local state=$3
  local fail_upload_at=${4:-0}
  local selected_run_url=${5:-$run_url}
  local view_error=${6:-}
  : > "$case_root/gh.log"
  PATH="$case_root/bin:$PATH" \
  FAKE_GH_LOG="$case_root/gh.log" \
  FAKE_GH_STATE="$state" \
  FAKE_GH_FAIL_UPLOAD_AT="$fail_upload_at" \
  FAKE_GH_VIEW_ERROR="$view_error" \
    bash "$publish_script" \
      --artifact-root "$artifact_root" \
      --repository "$repository" \
      --commit "$commit" \
      --run-url "$selected_run_url" \
      >"$case_root/stdout.log" 2>"$case_root/stderr.log"
}

assert_current_assets() {
  local state=$1
  local expected="$test_root/expected-current-assets"
  local actual="$test_root/actual-current-assets"
  write_expected_asset_names "$expected" "$commit"
  sort "$state/assets" > "$actual"
  diff -u "$expected" "$actual"
}

valid_artifacts="$test_root/valid-artifacts"
create_artifacts "$valid_artifacts"

create_case="$test_root/create"
mkdir -p "$create_case/state"
create_fake_gh "$create_case/bin"
run_publish "$create_case" "$valid_artifacts" "$create_case/state"
[[ -f $create_case/state/release-exists &&
   -f $create_case/state/prerelease &&
   "$(cat "$create_case/state/tag-sha")" == "$commit" ]]
assert_current_assets "$create_case/state"
grep -Fq '自动生成' "$create_case/state/notes"
grep -Fq '未签名' "$create_case/state/notes"
if grep -Fq 'release delete-asset' "$create_case/gh.log"; then
  echo "first publication unexpectedly deleted an asset" >&2
  exit 1
fi

update_case="$test_root/update"
mkdir -p "$update_case"
create_fake_gh "$update_case/bin"
initialize_existing_release "$update_case/state"
run_publish "$update_case" "$valid_artifacts" "$update_case/state"
[[ "$(cat "$update_case/state/tag-sha")" == "$commit" ]]
assert_current_assets "$update_case/state"
last_upload=$(grep -nF 'release upload' "$update_case/gh.log" |
  tail -n 1 | cut -d: -f1)
first_delete=$(grep -nF 'release delete-asset' "$update_case/gh.log" |
  head -n 1 | cut -d: -f1)
patch_line=$(grep -nF 'api --method PATCH' "$update_case/gh.log" |
  head -n 1 | cut -d: -f1)
[[ $first_delete -gt $last_upload && $first_delete -gt $patch_line ]]

resume_case="$test_root/resume-partial-upload"
mkdir -p "$resume_case"
create_fake_gh "$resume_case/bin"
initialize_existing_release "$resume_case/state"
current_linux_package="ZzPureToolsExample-continuous-linux-x86_64-${commit:0:12}.AppImage"
printf '%s\n' \
  "$current_linux_package" \
  "$current_linux_package.sha256" >> "$resume_case/state/assets"
run_publish "$resume_case" "$valid_artifacts" "$resume_case/state"
assert_current_assets "$resume_case/state"
residual_delete=$(grep -nF \
  "release delete-asset continuous-build $current_linux_package" \
  "$resume_case/gh.log" | head -n 1 | cut -d: -f1)
resume_first_upload=$(grep -nF 'release upload' "$resume_case/gh.log" |
  head -n 1 | cut -d: -f1)
resume_patch=$(grep -nF 'api --method PATCH' "$resume_case/gh.log" |
  head -n 1 | cut -d: -f1)
old_linux_package="ZzPureToolsExample-continuous-linux-x86_64-${old_commit:0:12}.AppImage"
old_asset_delete=$(grep -nF \
  "release delete-asset continuous-build $old_linux_package" \
  "$resume_case/gh.log" | head -n 1 | cut -d: -f1)
[[ $residual_delete -lt $resume_first_upload &&
   $old_asset_delete -gt $resume_patch ]]

missing_case="$test_root/missing"
mkdir -p "$missing_case/artifacts"
cp -R "$valid_artifacts/." "$missing_case/artifacts/"
rm -f "$missing_case/artifacts/macos-x86_64/"*.dmg
create_fake_gh "$missing_case/bin"
initialize_existing_release "$missing_case/state"
if run_publish "$missing_case" "$missing_case/artifacts" "$missing_case/state"; then
  echo "publish accepted a missing macOS package" >&2
  exit 1
fi
[[ ! -s $missing_case/gh.log &&
   "$(cat "$missing_case/state/tag-sha")" == "$old_commit" ]]

upload_failure_case="$test_root/upload-failure-existing"
mkdir -p "$upload_failure_case"
create_fake_gh "$upload_failure_case/bin"
initialize_existing_release "$upload_failure_case/state"
if run_publish "$upload_failure_case" "$valid_artifacts" \
    "$upload_failure_case/state" 4; then
  echo "publish ignored an upload failure" >&2
  exit 1
fi
[[ "$(cat "$upload_failure_case/state/tag-sha")" == "$old_commit" ]]
old_linux_package="ZzPureToolsExample-continuous-linux-x86_64-${old_commit:0:12}.AppImage"
grep -Fxq "$old_linux_package" "$upload_failure_case/state/assets"
if grep -Eq 'release edit|api --method PATCH|release delete-asset' \
    "$upload_failure_case/gh.log"; then
  echo "failed upload modified the existing release transaction" >&2
  exit 1
fi

upload_failure_create="$test_root/upload-failure-create"
mkdir -p "$upload_failure_create/state"
create_fake_gh "$upload_failure_create/bin"
if run_publish "$upload_failure_create" "$valid_artifacts" \
    "$upload_failure_create/state" 2; then
  echo "first publication ignored an upload failure" >&2
  exit 1
fi
[[ ! -f $upload_failure_create/state/release-exists ]]
grep -Fq 'release delete continuous-build' "$upload_failure_create/gh.log"

invalid_url_case="$test_root/invalid-run-url"
mkdir -p "$invalid_url_case/state"
create_fake_gh "$invalid_url_case/bin"
invalid_run_url="https://github.com/$repository/actions/runs/123/attempts/1"
if run_publish "$invalid_url_case" "$valid_artifacts" \
    "$invalid_url_case/state" 0 "$invalid_run_url"; then
  echo "publish accepted a workflow URL outside the exact run route" >&2
  exit 1
fi
[[ ! -s $invalid_url_case/gh.log ]]

view_failure_case="$test_root/view-failure"
mkdir -p "$view_failure_case/state"
create_fake_gh "$view_failure_case/bin"
if run_publish "$view_failure_case" "$valid_artifacts" \
    "$view_failure_case/state" 0 "$run_url" \
    'dial tcp: github.com host not found'; then
  echo "publish treated a GitHub connectivity failure as a missing release" >&2
  exit 1
fi
if grep -Eq \
    'release (create|upload|edit|delete-asset|delete)|api --method PATCH' \
    "$view_failure_case/gh.log"; then
  echo "release view failure triggered a remote write" >&2
  exit 1
fi

echo "PASS continuous publish transaction tests"
