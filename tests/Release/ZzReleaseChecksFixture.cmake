cmake_minimum_required(VERSION 3.23)

foreach(required IN ITEMS ZZ_SOURCE_DIR ZZ_BINARY_ROOT ZZ_WORK_DIR)
    if(NOT DEFINED ${required} OR "${${required}}" STREQUAL "")
        message(FATAL_ERROR "缺少 -D${required}=...")
    endif()
endforeach()
if(NOT IS_DIRECTORY "${ZZ_SOURCE_DIR}" OR NOT IS_DIRECTORY "${ZZ_BINARY_ROOT}")
    message(FATAL_ERROR "源码根目录或构建根目录不存在")
endif()

cmake_path(ABSOLUTE_PATH ZZ_SOURCE_DIR NORMALIZE OUTPUT_VARIABLE source_root)
cmake_path(ABSOLUTE_PATH ZZ_BINARY_ROOT NORMALIZE OUTPUT_VARIABLE binary_root)
cmake_path(ABSOLUTE_PATH ZZ_WORK_DIR NORMALIZE OUTPUT_VARIABLE work_root)
cmake_path(GET work_root ROOT_PATH filesystem_root)
cmake_path(IS_PREFIX binary_root "${work_root}"
    NORMALIZE work_is_below_binary)
if("${work_root}" STREQUAL "${filesystem_root}"
   OR "${work_root}" STREQUAL "${source_root}"
   OR "${work_root}" STREQUAL "${binary_root}"
   OR NOT work_is_below_binary)
    message(FATAL_ERROR "拒绝不安全的完整发布 fixture 目录：${work_root}")
endif()

file(REMOVE_RECURSE "${work_root}")
set(fixture_source "${work_root}/source")
set(fixture_evidence "${work_root}/evidence")
file(MAKE_DIRECTORY
    "${fixture_source}/docs/third-party"
    "${fixture_source}/ZzThirdParty/qwindowkit/qmsetup/src/corecmd"
    "${fixture_evidence}/qwindowkit"
    "${fixture_evidence}/qt-5.15.2"
    "${fixture_evidence}/reviews")

file(WRITE "${fixture_source}/LICENSE"
    "Fixture project license text for release validation.\n")
file(WRITE
    "${fixture_source}/ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp"
    "// Fixture derived source used only by the release checker test.\n")
file(WRITE "${fixture_evidence}/qwindowkit/source.tar.gz"
    "Fixture QWindowKit source archive bytes.\n")
file(WRITE "${fixture_evidence}/qwindowkit/provenance-review.json"
    "{\"reviewer\":\"fixture-reviewer\",\"reviewedAt\":\"2026-08-02T00:00:00Z\"}\n")
file(WRITE "${fixture_evidence}/qt-5.15.2/utils.cpp"
    "// Fixture Qt 5.15.2 upstream source bytes.\n")
file(WRITE "${fixture_evidence}/qt-5.15.2/LICENSES.txt"
    "Fixture Qt upstream license bytes.\n")
file(WRITE "${fixture_evidence}/reviews/windeployqt-review.json"
    "{\"reviewer\":\"fixture-reviewer\",\"reviewedAt\":\"2026-08-02T00:00:00Z\",\"conclusion\":\"approved\"}\n")
file(WRITE "${fixture_evidence}/reviews/project-license-approval.json"
    "{\"owner\":\"fixture-owner\",\"reviewedAt\":\"2026-08-02T00:00:00Z\",\"conclusion\":\"approved\",\"spdxExpression\":\"MIT\"}\n")

file(SHA256 "${fixture_source}/LICENSE" project_license_sha)
file(SHA256
    "${fixture_source}/ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp"
    local_source_sha)
file(SHA256 "${fixture_evidence}/qwindowkit/source.tar.gz" q_archive_sha)
file(SHA256
    "${fixture_evidence}/qwindowkit/provenance-review.json" q_review_sha)
file(SHA256 "${fixture_evidence}/qt-5.15.2/utils.cpp" qt_source_sha)
file(SHA256 "${fixture_evidence}/qt-5.15.2/LICENSES.txt" qt_license_sha)
file(SHA256
    "${fixture_evidence}/reviews/windeployqt-review.json" qt_review_sha)
file(SHA256
    "${fixture_evidence}/reviews/project-license-approval.json"
    project_approval_sha)

set(qwindowkit_commit 0123456789abcdef0123456789abcdef01234567)
set(vendor_template [=[
{
  "name": "QWindowKit",
  "declaredVersion": "1.5.1.0",
  "upstreamUrl": "https://github.com/stdware/qwindowkit",
  "upstreamCommit": "@qwindowkit_commit@",
  "importDate": "2026-08-02",
  "archiveSha256": "@q_archive_sha@",
  "licenses": ["Apache-2.0", "MIT"],
  "localPatches": [],
  "validatedMatrix": [
    {
      "environment": "fixture-only",
      "platform": "Linux",
      "compiler": "GCC fixture",
      "qt": "Qt 6.8 fixture",
      "reviewedAt": "2026-08-02T00:00:00Z"
    }
  ],
  "releaseBlockers": []
}
]=])
string(CONFIGURE "${vendor_template}" vendor_json @ONLY)
file(WRITE
    "${fixture_source}/docs/third-party/qwindowkit-vendor.json"
    "${vendor_json}")

set(release_template [=[
{
  "schemaVersion": 1,
  "review": {
    "reviewer": "fixture-reviewer",
    "reviewedAt": "2026-08-02T00:00:00Z"
  },
  "releaseBlockers": [],
  "evidence": {
    "qwindowkit": {
      "upstreamCommit": "@qwindowkit_commit@",
      "sourceArchive": {
        "scope": "external",
        "path": "qwindowkit/source.tar.gz",
        "sha256": "@q_archive_sha@"
      },
      "provenanceReview": {
        "scope": "external",
        "path": "qwindowkit/provenance-review.json",
        "sha256": "@q_review_sha@"
      }
    },
    "windeployqtDerivedWork": {
      "upstreamProject": "Qt",
      "upstreamVersion": "5.15.2",
      "upstreamFile": "qttools/src/windeployqt/utils.cpp",
      "localFile": "ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp",
      "upstreamSource": {
        "scope": "external",
        "path": "qt-5.15.2/utils.cpp",
        "sha256": "@qt_source_sha@"
      },
      "upstreamLicense": {
        "scope": "external",
        "path": "qt-5.15.2/LICENSES.txt",
        "sha256": "@qt_license_sha@"
      },
      "localSourceSha256": "@local_source_sha@",
      "redistributionConclusion": "approved",
      "reviewRecord": {
        "scope": "external",
        "path": "reviews/windeployqt-review.json",
        "sha256": "@qt_review_sha@"
      }
    },
    "projectLicense": {
      "spdxExpression": "MIT",
      "licenseFile": {
        "scope": "repository",
        "path": "LICENSE",
        "sha256": "@project_license_sha@"
      },
      "approvalRecord": {
        "scope": "external",
        "path": "reviews/project-license-approval.json",
        "sha256": "@project_approval_sha@"
      }
    }
  }
}
]=])
string(CONFIGURE "${release_template}" release_json @ONLY)
file(WRITE
    "${fixture_source}/docs/third-party/release-evidence.json"
    "${release_json}")

include("${source_root}/cmake/ZzReleaseChecks.cmake")
zz_verify_release_evidence(
    SOURCE_ROOT "${fixture_source}"
    EVIDENCE_ROOT "${fixture_evidence}")
