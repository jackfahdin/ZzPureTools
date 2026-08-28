#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat >&2 <<'USAGE'
usage: publish-continuous-build.sh \
  --artifact-root <path> --repository <owner/name> \
  --commit <40-lower-hex> --run-url <https://github.com/...>
USAGE
}

artifact_root_arg=
repository=
commit=
run_url=

while [[ $# -gt 0 ]]; do
  [[ $# -ge 2 ]] || {
    usage
    exit 64
  }
  case $1 in
    --artifact-root) artifact_root_arg=$2 ;;
    --repository) repository=$2 ;;
    --commit) commit=$2 ;;
    --run-url) run_url=$2 ;;
    *)
      echo "unknown argument: $1" >&2
      usage
      exit 64
      ;;
  esac
  shift 2
done

for value_name in artifact_root_arg repository commit run_url; do
  [[ -n ${!value_name} ]] || {
    echo "missing required argument: $value_name" >&2
    usage
    exit 64
  }
done
[[ $repository =~ ^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$ ]] || {
  echo "repository must use owner/name syntax" >&2
  exit 1
}
[[ $commit =~ ^[0-9a-f]{40}$ ]] || {
  echo "commit must be 40 lowercase hexadecimal characters" >&2
  exit 1
}
run_url_prefix="https://github.com/$repository/actions/runs/"
run_id=${run_url#"$run_url_prefix"}
[[ $run_url == "$run_url_prefix"* && $run_id =~ ^[0-9]+$ ]] || {
  echo "run-url must identify a workflow run for $repository" >&2
  exit 1
}

for tool in basename cat cmake cp diff dirname find gh grep jq mkdir mktemp \
    realpath rm sed sort wc; do
  command -v "$tool" >/dev/null || {
    echo "required tool is unavailable: $tool" >&2
    exit 69
  }
done

source_dir=$(realpath "$(dirname "${BASH_SOURCE[0]}")/../..")
[[ -d $artifact_root_arg && ! -L $artifact_root_arg ]] || {
  echo "artifact-root must be a regular directory" >&2
  exit 1
}
artifact_root=$(realpath "$artifact_root_arg")
[[ $artifact_root != / && $artifact_root != "$source_dir" ]] || {
  echo "artifact-root must not identify a broad source or filesystem root" >&2
  exit 1
}

# 在执行任何 GitHub 读写之前关闭本地产物集合，缺包或摘要错误不得触发远端操作。
cmake \
  "-DZZ_ARTIFACT_ROOT=$artifact_root" \
  "-DZZ_EXPECTED_COMMIT=$commit" \
  -P "$source_dir/scripts/package/VerifyArtifactSet.cmake"

tag=continuous-build
short_commit=${commit:0:12}
title="Continuous Build $short_commit"
temp_root=$(realpath "${TMPDIR:-/tmp}")
work_dir=$(mktemp -d "$temp_root/zz-continuous-publish.XXXXXX")
upload_dir="$work_dir/upload"
notes_file="$work_dir/release-notes.md"
desired_names_file="$work_dir/desired-assets.txt"
release_rows_file="$work_dir/release-rows.md"
mkdir -p "$upload_dir"
: > "$desired_names_file"
: > "$release_rows_file"

created_release=false
cleanup() {
  local status=$?
  trap - EXIT
  if [[ $status -ne 0 && $created_release == true ]]; then
    gh release delete "$tag" \
      --repo "$repository" --yes >/dev/null 2>&1 || status=1
  fi
  case "$work_dir/" in
    "$temp_root"/zz-continuous-publish.*) rm -rf -- "$work_dir" ;;
    *)
      echo "refusing to clean unexpected publish work directory: $work_dir" >&2
      status=1
      ;;
  esac
  exit "$status"
}
trap cleanup EXIT

declare -a upload_paths=()
build_info_count=0
while IFS= read -r build_info; do
  [[ -f $build_info && ! -L $build_info ]] || {
    echo "build-info input must be a regular file: $build_info" >&2
    exit 1
  }
  package_name=$(jq -er '.packageFile' "$build_info")
  platform_id=$(jq -er '.platformId' "$build_info")
  package_sha=$(jq -er '.packageSha256' "$build_info")
  qt_version=$(jq -er '.qtVersion' "$build_info")
  compiler_id=$(jq -er '.compilerId' "$build_info")
  compiler_version=$(jq -er '.compilerVersion' "$build_info")
  build_info_dir=$(dirname "$build_info")
  package_path="$build_info_dir/$package_name"
  checksum_path="$package_path.sha256"
  alias_path="$upload_dir/$package_name.build-info.json"
  for input_path in "$package_path" "$checksum_path"; do
    [[ -f $input_path && ! -L $input_path ]] || {
      echo "artifact upload input must be a regular file: $input_path" >&2
      exit 1
    }
  done
  cp "$build_info" "$alias_path"
  upload_paths+=("$package_path" "$checksum_path" "$alias_path")
  printf '%s\n' \
    "$package_name" \
    "$package_name.sha256" \
    "$package_name.build-info.json" >> "$desired_names_file"

  compiler_text="$compiler_id $compiler_version"
  compiler_text=${compiler_text//$'\n'/ }
  compiler_text=${compiler_text//|//}
  qt_version=${qt_version//$'\n'/ }
  qt_version=${qt_version//|//}
  printf '| `%s` | `%s` | `%s` | `%s` | `%s` |\n' \
    "$platform_id" "$package_name" "$package_sha" \
    "$qt_version" "$compiler_text" >> "$release_rows_file"
  build_info_count=$((build_info_count + 1))
done < <(find "$artifact_root" -type f -name build-info.json | sort)
[[ $build_info_count -eq 5 && ${#upload_paths[@]} -eq 15 ]] || {
  echo "expected five artifact groups and fifteen upload files" >&2
  exit 1
}
sort -u -o "$desired_names_file" "$desired_names_file"
[[ $(wc -l < "$desired_names_file") -eq 15 ]] || {
  echo "continuous Release asset names are not unique" >&2
  exit 1
}

{
  printf '# %s\n\n' "$title"
  printf '> 此页面由 CI 自动生成。这是未签名的非稳定预发布版本，仅建议用于测试。\n\n'
  printf '来源工作流：[%s](%s)\n\n' "$run_url" "$run_url"
  printf '| 平台 | 下载包 | SHA-256 | Qt | 编译器 |\n'
  printf '|---|---|---|---|---|\n'
  cat "$release_rows_file"
  printf '\n'
  printf 'Windows 和 macOS 产物未进行代码签名；首次运行时系统可能显示安全提示。\n'
} > "$notes_file"

asset_is_desired() {
  local asset_name=$1
  grep -Fxq "$asset_name" "$desired_names_file"
}

fetch_remote_assets() {
  local output=$1
  gh release view "$tag" \
    --repo "$repository" \
    --json assets \
    --jq '.assets[].name' > "$output"
}

verify_desired_assets_present() {
  local remote_file=$1
  local asset_name count
  while IFS= read -r asset_name; do
    count=$(grep -Fxc "$asset_name" "$remote_file" || true)
    [[ $count -eq 1 ]] || {
      echo "remote Release lacks exactly one $asset_name" >&2
      exit 1
    }
  done < "$desired_names_file"
}

release_exists=false
view_error="$work_dir/release-view.err"
if gh release view "$tag" --repo "$repository" \
    >/dev/null 2>"$view_error"; then
  release_exists=true
elif grep -Eiq 'release not found|HTTP 404' "$view_error"; then
  release_exists=false
else
  cat "$view_error" >&2
  exit 1
fi

existing_assets="$work_dir/existing-assets.txt"
: > "$existing_assets"
tag_sha=
reuse_uploaded_assets=false
if [[ $release_exists == true ]]; then
  fetch_remote_assets "$existing_assets"
  tag_sha=$(gh api \
    "repos/$repository/git/ref/tags/$tag" \
    --jq '.object.sha')
  [[ $tag_sha =~ ^[0-9a-f]{40}$ ]] || {
    echo "continuous-build tag does not resolve to a commit SHA" >&2
    exit 1
  }

  desired_present=0
  while IFS= read -r asset_name; do
    if grep -Fxq "$asset_name" "$existing_assets"; then
      desired_present=$((desired_present + 1))
    fi
  done < "$desired_names_file"
  if [[ $tag_sha == "$commit" && $desired_present -eq 15 ]]; then
    reuse_uploaded_assets=true
  else
    # 清除上一轮失败遗留的同提交临时资产；旧提交的完整资产仍保持可下载。
    while IFS= read -r asset_name; do
      if asset_is_desired "$asset_name"; then
        gh release delete-asset "$tag" "$asset_name" \
          --repo "$repository" --yes
      fi
    done < "$existing_assets"
  fi
else
  gh release create "$tag" \
    --repo "$repository" \
    --target "$commit" \
    --title "$title" \
    --notes-file "$notes_file" \
    --prerelease \
    --latest=false
  created_release=true
fi

if [[ $reuse_uploaded_assets != true ]]; then
  for upload_path in "${upload_paths[@]}"; do
    gh release upload "$tag" "$upload_path" --repo "$repository"
  done
fi

uploaded_assets="$work_dir/uploaded-assets.txt"
fetch_remote_assets "$uploaded_assets"
verify_desired_assets_present "$uploaded_assets"

# 先更新发布说明，再移动固定 tag；两者都成功后才清除上一提交资产。
gh release edit "$tag" \
  --repo "$repository" \
  --target "$commit" \
  --title "$title" \
  --notes-file "$notes_file" \
  --prerelease \
  --latest=false
gh api --method PATCH \
  "repos/$repository/git/refs/tags/$tag" \
  -f "sha=$commit" \
  -F force=true >/dev/null
verified_tag_sha=$(gh api \
  "repos/$repository/git/ref/tags/$tag" \
  --jq '.object.sha')
[[ $verified_tag_sha == "$commit" ]] || {
  echo "continuous-build tag update was not observable" >&2
  exit 1
}

promoted_assets="$work_dir/promoted-assets.txt"
fetch_remote_assets "$promoted_assets"
while IFS= read -r asset_name; do
  [[ -n $asset_name ]] || continue
  if ! asset_is_desired "$asset_name"; then
    gh release delete-asset "$tag" "$asset_name" \
      --repo "$repository" --yes
  fi
done < "$promoted_assets"

final_assets="$work_dir/final-assets.txt"
fetch_remote_assets "$final_assets"
sort -u -o "$final_assets" "$final_assets"
diff -u "$desired_names_file" "$final_assets"

created_release=false
echo "PASS continuous-build Release: $repository@$commit"
