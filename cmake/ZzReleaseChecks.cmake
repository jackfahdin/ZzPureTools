include_guard(GLOBAL)

function(_zz_release_json_value json expected_type out_value out_valid)
    set(json_path ${ARGN})
    string(JSON value_type ERROR_VARIABLE type_error
        TYPE "${json}" ${json_path})
    if(NOT "${type_error}" STREQUAL "NOTFOUND"
       OR NOT "${value_type}" STREQUAL "${expected_type}")
        set(${out_value} "" PARENT_SCOPE)
        set(${out_valid} FALSE PARENT_SCOPE)
        return()
    endif()

    string(JSON value ERROR_VARIABLE value_error GET "${json}" ${json_path})
    if(NOT "${value_error}" STREQUAL "NOTFOUND")
        set(${out_value} "" PARENT_SCOPE)
        set(${out_valid} FALSE PARENT_SCOPE)
        return()
    endif()
    set(${out_value} "${value}" PARENT_SCOPE)
    set(${out_valid} TRUE PARENT_SCOPE)
endfunction()

function(_zz_release_is_meaningful value out_valid)
    string(STRIP "${value}" normalized_value)
    string(TOUPPER "${normalized_value}" upper_value)
    if("${normalized_value}" STREQUAL ""
       OR "${upper_value}" STREQUAL "NULL"
       OR "${upper_value}" STREQUAL "UNKNOWN"
       OR "${upper_value}" STREQUAL "UNVERIFIED")
        set(${out_valid} FALSE PARENT_SCOPE)
    else()
        set(${out_valid} TRUE PARENT_SCOPE)
    endif()
endfunction()

function(_zz_release_is_utc_timestamp value out_valid)
    string(LENGTH "${value}" value_length)
    if(value_length EQUAL 20
       AND "${value}" MATCHES
           "^[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]Z$")
        set(${out_valid} TRUE PARENT_SCOPE)
    else()
        set(${out_valid} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(_zz_release_is_lower_hex value required_length out_valid)
    string(LENGTH "${value}" value_length)
    if(value_length EQUAL required_length
       AND "${value}" MATCHES "^[0-9a-f]+$")
        set(${out_valid} TRUE PARENT_SCOPE)
    else()
        set(${out_valid} FALSE PARENT_SCOPE)
    endif()
endfunction()

function(_zz_release_read_json path label out_json out_errors)
    set(errors)
    if(NOT EXISTS "${path}" OR IS_DIRECTORY "${path}" OR IS_SYMLINK "${path}")
        list(APPEND errors "${label} 必须是存在的普通文件：${path}")
        set(${out_json} "{}" PARENT_SCOPE)
        set(${out_errors} "${errors}" PARENT_SCOPE)
        return()
    endif()
    file(SIZE "${path}" file_size)
    if(file_size EQUAL 0)
        list(APPEND errors "${label} 不能为空：${path}")
        set(${out_json} "{}" PARENT_SCOPE)
        set(${out_errors} "${errors}" PARENT_SCOPE)
        return()
    endif()

    file(READ "${path}" json)
    string(JSON root_type ERROR_VARIABLE json_error TYPE "${json}")
    if(NOT "${json_error}" STREQUAL "NOTFOUND"
       OR NOT "${root_type}" STREQUAL "OBJECT")
        list(APPEND errors "${label} 必须是合法的 JSON 对象：${json_error}")
        set(json "{}")
    endif()
    set(${out_json} "${json}" PARENT_SCOPE)
    set(${out_errors} "${errors}" PARENT_SCOPE)
endfunction()

function(_zz_release_validate_review json label out_errors)
    set(json_path ${ARGN})
    set(errors)

    set(reviewer_path ${json_path})
    list(APPEND reviewer_path reviewer)
    _zz_release_json_value(
        "${json}" STRING reviewer reviewer_valid ${reviewer_path})
    _zz_release_is_meaningful("${reviewer}" reviewer_meaningful)
    if(NOT reviewer_valid OR NOT reviewer_meaningful)
        list(APPEND errors "${label}.reviewer 必须是具名审核人")
    endif()

    set(date_path ${json_path})
    list(APPEND date_path reviewedAt)
    _zz_release_json_value(
        "${json}" STRING reviewed_at reviewed_at_valid ${date_path})
    _zz_release_is_utc_timestamp("${reviewed_at}" reviewed_at_is_utc)
    if(NOT reviewed_at_valid OR NOT reviewed_at_is_utc)
        list(APPEND errors
            "${label}.reviewedAt 必须匹配 UTC YYYY-MM-DDTHH:MM:SSZ")
    endif()
    set(${out_errors} "${errors}" PARENT_SCOPE)
endfunction()

function(_zz_release_collect_object_blockers json out_blockers out_errors)
    set(json_path ${ARGN})
    set(blockers)
    set(errors)
    string(JSON blockers_type ERROR_VARIABLE type_error
        TYPE "${json}" ${json_path})
    if(NOT "${type_error}" STREQUAL "NOTFOUND"
       OR NOT "${blockers_type}" STREQUAL "ARRAY")
        list(APPEND errors "release-evidence.releaseBlockers 必须是数组")
    else()
        string(JSON blocker_count LENGTH "${json}" ${json_path})
        if(blocker_count GREATER 0)
            math(EXPR last_blocker "${blocker_count} - 1")
            foreach(index RANGE 0 ${last_blocker})
                set(id_path ${json_path} ${index} id)
                _zz_release_json_value(
                    "${json}" STRING blocker_id blocker_id_valid ${id_path})
                _zz_release_is_meaningful(
                    "${blocker_id}" blocker_id_meaningful)
                if(blocker_id_valid AND blocker_id_meaningful)
                    list(APPEND blockers "${blocker_id}")
                else()
                    list(APPEND errors
                        "release-evidence.releaseBlockers[${index}].id 无效")
                endif()
            endforeach()
        endif()
    endif()
    set(${out_blockers} "${blockers}" PARENT_SCOPE)
    set(${out_errors} "${errors}" PARENT_SCOPE)
endfunction()

function(_zz_release_collect_string_blockers json out_blockers out_errors)
    set(json_path ${ARGN})
    set(blockers)
    set(errors)
    string(JSON blockers_type ERROR_VARIABLE type_error
        TYPE "${json}" ${json_path})
    if(NOT "${type_error}" STREQUAL "NOTFOUND"
       OR NOT "${blockers_type}" STREQUAL "ARRAY")
        list(APPEND errors "qwindowkit-vendor.releaseBlockers 必须是数组")
    else()
        string(JSON blocker_count LENGTH "${json}" ${json_path})
        if(blocker_count GREATER 0)
            math(EXPR last_blocker "${blocker_count} - 1")
            foreach(index RANGE 0 ${last_blocker})
                set(id_path ${json_path} ${index})
                _zz_release_json_value(
                    "${json}" STRING blocker_id blocker_id_valid ${id_path})
                _zz_release_is_meaningful(
                    "${blocker_id}" blocker_id_meaningful)
                if(blocker_id_valid AND blocker_id_meaningful)
                    list(APPEND blockers "${blocker_id}")
                else()
                    list(APPEND errors
                        "qwindowkit-vendor.releaseBlockers[${index}] 无效")
                endif()
            endforeach()
        endif()
    endif()
    set(${out_blockers} "${blockers}" PARENT_SCOPE)
    set(${out_errors} "${errors}" PARENT_SCOPE)
endfunction()

function(_zz_release_verify_file_object
         json source_root evidence_root label
         out_scope out_relative_path out_resolved_path out_sha256 out_errors)
    set(json_path ${ARGN})
    set(errors)
    set(scope "")
    set(relative_path "")
    set(resolved_path "")
    set(actual_sha256 "")

    string(JSON object_type ERROR_VARIABLE object_error
        TYPE "${json}" ${json_path})
    if(NOT "${object_error}" STREQUAL "NOTFOUND"
       OR NOT "${object_type}" STREQUAL "OBJECT")
        list(APPEND errors
            "${label} 必须是仅含 scope/path/sha256 的文件对象")
    else()
        string(JSON member_count LENGTH "${json}" ${json_path})
        set(members)
        if(member_count GREATER 0)
            math(EXPR last_member "${member_count} - 1")
            foreach(index RANGE 0 ${last_member})
                string(JSON member MEMBER "${json}" ${json_path} ${index})
                list(APPEND members "${member}")
            endforeach()
        endif()
        list(SORT members)
        if(NOT "${members}" STREQUAL "path;scope;sha256")
            list(APPEND errors
                "${label} 必须精确包含 scope/path/sha256 三个成员")
        endif()
    endif()

    set(scope_path ${json_path} scope)
    _zz_release_json_value("${json}" STRING scope scope_valid ${scope_path})
    if(NOT scope_valid
       OR (NOT "${scope}" STREQUAL "repository"
           AND NOT "${scope}" STREQUAL "external"))
        list(APPEND errors "${label}.scope 必须是 repository 或 external")
    endif()

    set(path_path ${json_path} path)
    _zz_release_json_value(
        "${json}" STRING relative_path path_valid ${path_path})
    _zz_release_is_meaningful("${relative_path}" path_meaningful)
    set(path_for_check "${relative_path}")
    cmake_path(IS_ABSOLUTE path_for_check path_is_absolute)
    if(NOT path_valid OR NOT path_meaningful
       OR path_is_absolute
       OR "${relative_path}" MATCHES "^[A-Za-z]:[/\\\\]"
       OR "${relative_path}" MATCHES "^[/\\\\]"
       OR "${relative_path}" MATCHES "(^|[/\\\\])\\.\\.([/\\\\]|$)")
        list(APPEND errors
            "${label}.path 必须是受控根目录下且不含 .. 的相对路径")
        set(path_safe FALSE)
    else()
        set(path_safe TRUE)
    endif()

    set(hash_path ${json_path} sha256)
    _zz_release_json_value(
        "${json}" STRING declared_sha256 hash_valid ${hash_path})
    _zz_release_is_lower_hex("${declared_sha256}" 64 hash_is_lower_hex)
    if(NOT hash_valid OR NOT hash_is_lower_hex)
        list(APPEND errors "${label}.sha256 必须是 64 位小写十六进制")
    endif()

    if("${scope}" STREQUAL "repository")
        set(scope_root "${source_root}")
    elseif("${scope}" STREQUAL "external")
        set(scope_root "${evidence_root}")
    else()
        set(scope_root "")
    endif()

    if(path_safe AND NOT "${scope_root}" STREQUAL "")
        if(NOT IS_DIRECTORY "${scope_root}")
            list(APPEND errors "${label} 的受控根目录不存在：${scope_root}")
        else()
            file(REAL_PATH "${scope_root}" normalized_root)
            set(candidate_path "${normalized_root}/${relative_path}")
            if(NOT EXISTS "${candidate_path}"
               OR IS_DIRECTORY "${candidate_path}"
               OR IS_SYMLINK "${candidate_path}")
                list(APPEND errors
                    "${label} 必须指向存在的非符号链接普通文件：${relative_path}")
            else()
                file(REAL_PATH "${candidate_path}" resolved_path)
                cmake_path(IS_PREFIX normalized_root "${resolved_path}"
                    NORMALIZE file_is_below_root)
                if(NOT file_is_below_root)
                    list(APPEND errors
                        "${label} 解析后越过了受控根目录：${relative_path}")
                    set(resolved_path "")
                else()
                    file(SIZE "${resolved_path}" evidence_size)
                    if(evidence_size EQUAL 0)
                        list(APPEND errors "${label} 指向的文件不能为空")
                    else()
                        file(SHA256 "${resolved_path}" actual_sha256)
                        if(hash_valid AND hash_is_lower_hex
                           AND NOT "${actual_sha256}" STREQUAL
                                   "${declared_sha256}")
                            list(APPEND errors
                                "${label} 的 SHA-256 与实际文件不匹配")
                        endif()
                    endif()
                endif()
            endif()
        endif()
    elseif(path_safe AND "${scope}" STREQUAL "external")
        list(APPEND errors "${label} 缺少外部证据根目录")
    endif()

    set(${out_scope} "${scope}" PARENT_SCOPE)
    set(${out_relative_path} "${relative_path}" PARENT_SCOPE)
    set(${out_resolved_path} "${resolved_path}" PARENT_SCOPE)
    set(${out_sha256} "${actual_sha256}" PARENT_SCOPE)
    set(${out_errors} "${errors}" PARENT_SCOPE)
endfunction()

function(zz_verify_release_evidence)
    set(options)
    set(one_value_args SOURCE_ROOT EVIDENCE_ROOT)
    set(multi_value_args FORCED_BLOCKERS)
    cmake_parse_arguments(
        ZZ_RELEASE "${options}" "${one_value_args}" "${multi_value_args}"
        ${ARGN})
    if(ZZ_RELEASE_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR
            "zz_verify_release_evidence 收到未知参数：${ZZ_RELEASE_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT DEFINED ZZ_RELEASE_SOURCE_ROOT
       OR "${ZZ_RELEASE_SOURCE_ROOT}" STREQUAL "")
        message(FATAL_ERROR
            "zz_verify_release_evidence 必须显式传入 SOURCE_ROOT")
    endif()

    set(global_errors)
    set(qwindowkit_errors)
    set(derived_work_errors)
    set(project_license_errors)
    set(blockers)

    if(NOT IS_DIRECTORY "${ZZ_RELEASE_SOURCE_ROOT}")
        list(APPEND global_errors
            "SOURCE_ROOT 必须是存在的目录：${ZZ_RELEASE_SOURCE_ROOT}")
        set(source_root "${ZZ_RELEASE_SOURCE_ROOT}")
    else()
        file(REAL_PATH "${ZZ_RELEASE_SOURCE_ROOT}" source_root)
    endif()

    if("${ZZ_RELEASE_EVIDENCE_ROOT}" STREQUAL ""
       OR NOT IS_ABSOLUTE "${ZZ_RELEASE_EVIDENCE_ROOT}"
       OR NOT IS_DIRECTORY "${ZZ_RELEASE_EVIDENCE_ROOT}")
        list(APPEND global_errors
            "EVIDENCE_ROOT 必须是存在的绝对目录：${ZZ_RELEASE_EVIDENCE_ROOT}")
        set(evidence_root "${ZZ_RELEASE_EVIDENCE_ROOT}")
    else()
        file(REAL_PATH "${ZZ_RELEASE_EVIDENCE_ROOT}" evidence_root)
    endif()

    set(manifest_root "${source_root}/docs/third-party")
    _zz_release_read_json(
        "${manifest_root}/release-evidence.json"
        "release-evidence.json" release_json release_manifest_errors)
    _zz_release_read_json(
        "${manifest_root}/qwindowkit-vendor.json"
        "qwindowkit-vendor.json" vendor_json vendor_manifest_errors)
    list(APPEND global_errors ${release_manifest_errors})
    list(APPEND qwindowkit_errors ${vendor_manifest_errors})

    _zz_release_json_value(
        "${release_json}" NUMBER schema_version schema_valid schemaVersion)
    if(NOT schema_valid OR NOT "${schema_version}" STREQUAL "1")
        list(APPEND global_errors
            "release-evidence.schemaVersion 必须精确等于 1")
    endif()
    _zz_release_validate_review(
        "${release_json}" "release-evidence.review" review_errors review)
    list(APPEND global_errors ${review_errors})

    _zz_release_collect_object_blockers(
        "${release_json}" manifest_blockers blocker_errors releaseBlockers)
    list(APPEND blockers ${manifest_blockers})
    list(APPEND global_errors ${blocker_errors})

    _zz_release_json_value(
        "${vendor_json}" STRING vendor_commit vendor_commit_valid
        upstreamCommit)
    _zz_release_is_lower_hex("${vendor_commit}" 40 vendor_commit_is_hex)
    if(NOT vendor_commit_valid OR NOT vendor_commit_is_hex)
        list(APPEND qwindowkit_errors
            "qwindowkit-vendor.upstreamCommit 必须是 40 位小写十六进制")
    endif()
    _zz_release_json_value(
        "${vendor_json}" STRING vendor_archive_sha vendor_archive_valid
        archiveSha256)
    _zz_release_is_lower_hex(
        "${vendor_archive_sha}" 64 vendor_archive_is_hex)
    if(NOT vendor_archive_valid OR NOT vendor_archive_is_hex)
        list(APPEND qwindowkit_errors
            "qwindowkit-vendor.archiveSha256 必须是 64 位小写十六进制")
    endif()

    string(JSON matrix_type ERROR_VARIABLE matrix_error
        TYPE "${vendor_json}" validatedMatrix)
    if(NOT "${matrix_error}" STREQUAL "NOTFOUND"
       OR NOT "${matrix_type}" STREQUAL "ARRAY")
        list(APPEND qwindowkit_errors
            "qwindowkit-vendor.validatedMatrix 必须是非空数组")
    else()
        string(JSON matrix_length LENGTH "${vendor_json}" validatedMatrix)
        if(matrix_length EQUAL 0)
            list(APPEND qwindowkit_errors
                "qwindowkit-vendor.validatedMatrix 必须是非空数组")
        endif()
    endif()

    _zz_release_collect_string_blockers(
        "${vendor_json}" vendor_blockers vendor_blocker_errors releaseBlockers)
    list(APPEND blockers ${vendor_blockers})
    list(APPEND qwindowkit_errors ${vendor_blocker_errors})
    if(vendor_blockers)
        list(APPEND qwindowkit_errors
            "qwindowkit-vendor.releaseBlockers 必须为空")
    endif()

    _zz_release_json_value(
        "${release_json}" STRING evidence_commit evidence_commit_valid
        evidence qwindowkit upstreamCommit)
    _zz_release_is_lower_hex("${evidence_commit}" 40 evidence_commit_is_hex)
    if(NOT evidence_commit_valid OR NOT evidence_commit_is_hex)
        list(APPEND qwindowkit_errors
            "evidence.qwindowkit.upstreamCommit 必须是 40 位小写十六进制")
    elseif(vendor_commit_valid AND vendor_commit_is_hex
           AND NOT "${evidence_commit}" STREQUAL "${vendor_commit}")
        list(APPEND qwindowkit_errors
            "QWindowKit 证据 commit 与 vendor manifest 不一致")
    endif()

    _zz_release_verify_file_object(
        "${release_json}" "${source_root}" "${evidence_root}"
        "evidence.qwindowkit.sourceArchive"
        q_archive_scope q_archive_relative q_archive_path q_archive_sha
        q_archive_errors evidence qwindowkit sourceArchive)
    list(APPEND qwindowkit_errors ${q_archive_errors})
    if(NOT "${q_archive_sha}" STREQUAL ""
       AND vendor_archive_valid AND vendor_archive_is_hex
       AND NOT "${q_archive_sha}" STREQUAL "${vendor_archive_sha}")
        list(APPEND qwindowkit_errors
            "QWindowKit 源码归档摘要与 vendor manifest 不一致")
    endif()

    _zz_release_verify_file_object(
        "${release_json}" "${source_root}" "${evidence_root}"
        "evidence.qwindowkit.provenanceReview"
        q_review_scope q_review_relative q_review_path q_review_sha
        q_review_errors evidence qwindowkit provenanceReview)
    list(APPEND qwindowkit_errors ${q_review_errors})
    if(NOT "${q_review_path}" STREQUAL "")
        _zz_release_read_json(
            "${q_review_path}" "QWindowKit 来源审核记录"
            q_review_json q_review_json_errors)
        list(APPEND qwindowkit_errors ${q_review_json_errors})
        _zz_release_validate_review(
            "${q_review_json}" "QWindowKit 来源审核记录"
            q_record_review_errors)
        list(APPEND qwindowkit_errors ${q_record_review_errors})
    endif()

    _zz_release_json_value(
        "${release_json}" STRING upstream_project upstream_project_valid
        evidence windeployqtDerivedWork upstreamProject)
    if(NOT upstream_project_valid OR NOT "${upstream_project}" STREQUAL "Qt")
        list(APPEND derived_work_errors
            "windeployqtDerivedWork.upstreamProject 必须是 Qt")
    endif()
    _zz_release_json_value(
        "${release_json}" STRING upstream_version upstream_version_valid
        evidence windeployqtDerivedWork upstreamVersion)
    if(NOT upstream_version_valid
       OR NOT "${upstream_version}" STREQUAL "5.15.2")
        list(APPEND derived_work_errors
            "windeployqtDerivedWork.upstreamVersion 必须精确等于 5.15.2")
    endif()
    _zz_release_json_value(
        "${release_json}" STRING upstream_file upstream_file_valid
        evidence windeployqtDerivedWork upstreamFile)
    if(NOT upstream_file_valid
       OR NOT "${upstream_file}" STREQUAL
              "qttools/src/windeployqt/utils.cpp")
        list(APPEND derived_work_errors
            "windeployqtDerivedWork.upstreamFile 路径不符合固定来源")
    endif()
    _zz_release_json_value(
        "${release_json}" STRING local_file local_file_valid
        evidence windeployqtDerivedWork localFile)
    set(expected_local_file
        "ZzThirdParty/qwindowkit/qmsetup/src/corecmd/utils_win.cpp")
    if(NOT local_file_valid
       OR NOT "${local_file}" STREQUAL "${expected_local_file}")
        list(APPEND derived_work_errors
            "windeployqtDerivedWork.localFile 路径不符合固定派生文件")
    endif()

    _zz_release_verify_file_object(
        "${release_json}" "${source_root}" "${evidence_root}"
        "windeployqtDerivedWork.upstreamSource"
        qt_source_scope qt_source_relative qt_source_path qt_source_sha
        qt_source_errors evidence windeployqtDerivedWork upstreamSource)
    list(APPEND derived_work_errors ${qt_source_errors})
    _zz_release_verify_file_object(
        "${release_json}" "${source_root}" "${evidence_root}"
        "windeployqtDerivedWork.upstreamLicense"
        qt_license_scope qt_license_relative qt_license_path qt_license_sha
        qt_license_errors evidence windeployqtDerivedWork upstreamLicense)
    list(APPEND derived_work_errors ${qt_license_errors})
    _zz_release_verify_file_object(
        "${release_json}" "${source_root}" "${evidence_root}"
        "windeployqtDerivedWork.reviewRecord"
        qt_review_scope qt_review_relative qt_review_path qt_review_sha
        qt_review_errors evidence windeployqtDerivedWork reviewRecord)
    list(APPEND derived_work_errors ${qt_review_errors})

    _zz_release_json_value(
        "${release_json}" STRING local_source_sha local_source_sha_valid
        evidence windeployqtDerivedWork localSourceSha256)
    _zz_release_is_lower_hex(
        "${local_source_sha}" 64 local_source_sha_is_hex)
    if(NOT local_source_sha_valid OR NOT local_source_sha_is_hex)
        list(APPEND derived_work_errors
            "windeployqtDerivedWork.localSourceSha256 必须是 64 位小写十六进制")
    endif()
    set(local_source_path "${source_root}/${expected_local_file}")
    if(NOT EXISTS "${local_source_path}"
       OR IS_DIRECTORY "${local_source_path}"
       OR IS_SYMLINK "${local_source_path}")
        list(APPEND derived_work_errors "固定的本地派生文件不存在")
    else()
        file(SIZE "${local_source_path}" local_source_size)
        file(SHA256 "${local_source_path}" actual_local_source_sha)
        if(local_source_size EQUAL 0)
            list(APPEND derived_work_errors "固定的本地派生文件不能为空")
        elseif(local_source_sha_valid AND local_source_sha_is_hex
               AND NOT "${actual_local_source_sha}" STREQUAL
                       "${local_source_sha}")
            list(APPEND derived_work_errors "本地派生文件 SHA-256 不匹配")
        endif()
    endif()

    _zz_release_json_value(
        "${release_json}" STRING redistribution_conclusion
        redistribution_conclusion_valid
        evidence windeployqtDerivedWork redistributionConclusion)
    if(NOT redistribution_conclusion_valid
       OR NOT "${redistribution_conclusion}" STREQUAL "approved")
        list(APPEND derived_work_errors
            "windeployqtDerivedWork.redistributionConclusion 必须是 approved")
    endif()
    if(NOT "${qt_review_path}" STREQUAL "")
        _zz_release_read_json(
            "${qt_review_path}" "windeployqt 再分发审核记录"
            qt_review_json qt_review_json_errors)
        list(APPEND derived_work_errors ${qt_review_json_errors})
        _zz_release_validate_review(
            "${qt_review_json}" "windeployqt 再分发审核记录"
            qt_record_review_errors)
        list(APPEND derived_work_errors ${qt_record_review_errors})
        _zz_release_json_value(
            "${qt_review_json}" STRING record_conclusion
            record_conclusion_valid conclusion)
        if(NOT record_conclusion_valid
           OR NOT "${record_conclusion}" STREQUAL "approved")
            list(APPEND derived_work_errors
                "windeployqt 再分发审核记录.conclusion 必须是 approved")
        endif()
    endif()

    _zz_release_json_value(
        "${release_json}" STRING spdx_expression spdx_valid
        evidence projectLicense spdxExpression)
    _zz_release_is_meaningful("${spdx_expression}" spdx_meaningful)
    if(NOT spdx_valid OR NOT spdx_meaningful)
        list(APPEND project_license_errors
            "projectLicense.spdxExpression 必须是明确的 SPDX 表达式")
    endif()

    _zz_release_verify_file_object(
        "${release_json}" "${source_root}" "${evidence_root}"
        "projectLicense.licenseFile"
        project_scope project_relative project_license_path project_license_sha
        project_file_errors evidence projectLicense licenseFile)
    list(APPEND project_license_errors ${project_file_errors})
    if(NOT "${project_scope}" STREQUAL "repository"
       OR NOT "${project_relative}" STREQUAL "LICENSE")
        list(APPEND project_license_errors
            "projectLicense.licenseFile 必须精确引用仓库根 LICENSE")
    endif()

    _zz_release_verify_file_object(
        "${release_json}" "${source_root}" "${evidence_root}"
        "projectLicense.approvalRecord"
        approval_scope approval_relative approval_path approval_sha
        approval_file_errors evidence projectLicense approvalRecord)
    list(APPEND project_license_errors ${approval_file_errors})
    if(NOT "${approval_path}" STREQUAL "")
        _zz_release_read_json(
            "${approval_path}" "项目许可证批准记录"
            approval_json approval_json_errors)
        list(APPEND project_license_errors ${approval_json_errors})
        _zz_release_json_value(
            "${approval_json}" STRING approval_owner approval_owner_valid owner)
        _zz_release_is_meaningful(
            "${approval_owner}" approval_owner_meaningful)
        if(NOT approval_owner_valid OR NOT approval_owner_meaningful)
            list(APPEND project_license_errors
                "项目许可证批准记录.owner 必须是具名所有者")
        endif()
        _zz_release_json_value(
            "${approval_json}" STRING approval_date approval_date_valid
            reviewedAt)
        _zz_release_is_utc_timestamp(
            "${approval_date}" approval_date_is_utc)
        if(NOT approval_date_valid OR NOT approval_date_is_utc)
            list(APPEND project_license_errors
                "项目许可证批准记录.reviewedAt 必须是 UTC 时间")
        endif()
        _zz_release_json_value(
            "${approval_json}" STRING approval_conclusion
            approval_conclusion_valid conclusion)
        if(NOT approval_conclusion_valid
           OR NOT "${approval_conclusion}" STREQUAL "approved")
            list(APPEND project_license_errors
                "项目许可证批准记录.conclusion 必须是 approved")
        endif()
        _zz_release_json_value(
            "${approval_json}" STRING approval_spdx approval_spdx_valid
            spdxExpression)
        if(NOT approval_spdx_valid OR NOT spdx_valid
           OR NOT "${approval_spdx}" STREQUAL "${spdx_expression}")
            list(APPEND project_license_errors
                "项目许可证批准记录.spdxExpression 与 manifest 不一致")
        endif()
    endif()

    if(global_errors)
        list(APPEND blockers
            qwindowkit.upstream-provenance
            qmsetup.windeployqt-5.15.2-derived-work
            project.license)
    endif()
    if(qwindowkit_errors)
        list(APPEND blockers qwindowkit.upstream-provenance)
    endif()
    if(derived_work_errors)
        list(APPEND blockers qmsetup.windeployqt-5.15.2-derived-work)
    endif()
    if(project_license_errors)
        list(APPEND blockers project.license)
    endif()
    foreach(forced_blocker IN LISTS ZZ_RELEASE_FORCED_BLOCKERS)
        _zz_release_is_meaningful("${forced_blocker}" forced_is_meaningful)
        if(forced_is_meaningful)
            list(APPEND blockers "${forced_blocker}")
        endif()
    endforeach()

    list(REMOVE_DUPLICATES blockers)
    set(all_errors
        ${global_errors}
        ${qwindowkit_errors}
        ${derived_work_errors}
        ${project_license_errors})
    list(REMOVE_DUPLICATES all_errors)
    if(blockers OR all_errors)
        set(report "发布证据校验失败。\n阻塞项：")
        foreach(blocker IN LISTS blockers)
            string(APPEND report "\n  - ${blocker}")
        endforeach()
        if(all_errors)
            string(APPEND report "\n证据问题：")
            foreach(error IN LISTS all_errors)
                string(APPEND report "\n  - ${error}")
            endforeach()
        endif()
        message(FATAL_ERROR "${report}")
    endif()

    message(STATUS "发布证据、文件摘要和审核记录校验通过")
endfunction()
