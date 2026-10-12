cmake_minimum_required(VERSION 3.23)

if(NOT DEFINED ZZ_SOURCE_DIR OR "${ZZ_SOURCE_DIR}" STREQUAL "")
    message(FATAL_ERROR "Missing -DZZ_SOURCE_DIR=...")
endif()

function(zz_contract_metric_kind output unit)
    if(unit STREQUAL "ms" OR unit STREQUAL "us")
        set(kind statistical-duration)
    elseif(unit STREQUAL "count" OR unit STREQUAL "ratio"
           OR unit STREQUAL "px" OR unit STREQUAL "objects")
        set(kind deterministic)
    elseif(unit STREQUAL "bytes" OR unit STREQUAL "percent")
        set(kind sampled-resource)
    else()
        message(FATAL_ERROR "Unsupported performance metric unit: ${unit}")
    endif()
    set(${output} "${kind}" PARENT_SCOPE)
endfunction()

function(zz_validate_formal_thresholds)
    set(threshold_file
        "${ZZ_SOURCE_DIR}/docs/performance/reference/linux/regression-thresholds.json")
    file(READ "${threshold_file}" thresholds_json)
    string(JSON threshold_schema GET "${thresholds_json}" schemaVersion)
    if(NOT threshold_schema EQUAL 2)
        message(FATAL_ERROR "Formal thresholds must use schemaVersion 2")
    endif()

    file(GLOB reporters LIST_DIRECTORIES FALSE
        "${ZZ_SOURCE_DIR}/docs/performance/reference/linux/*.json")
    list(REMOVE_ITEM reporters "${threshold_file}")
    set(expected_scenarios)
    foreach(reporter IN LISTS reporters)
        file(READ "${reporter}" reporter_json)
        string(JSON scenario GET "${reporter_json}" scenario)
        list(APPEND expected_scenarios "${scenario}")
    endforeach()
    list(SORT expected_scenarios)

    string(JSON scenario_count LENGTH "${thresholds_json}" scenarios)
    set(actual_scenarios)
    math(EXPR last_scenario "${scenario_count} - 1")
    foreach(scenario_index RANGE 0 ${last_scenario})
        string(JSON scenario MEMBER "${thresholds_json}" scenarios ${scenario_index})
        list(APPEND actual_scenarios "${scenario}")
    endforeach()
    list(SORT actual_scenarios)
    if(NOT "${actual_scenarios}" STREQUAL "${expected_scenarios}")
        set(missing_reports "${actual_scenarios}")
        set(missing_thresholds "${expected_scenarios}")
        if(expected_scenarios)
            list(REMOVE_ITEM missing_reports ${expected_scenarios})
        endif()
        if(actual_scenarios)
            list(REMOVE_ITEM missing_thresholds ${actual_scenarios})
        endif()
        list(JOIN missing_reports ", " missing_reports_text)
        list(JOIN missing_thresholds ", " missing_thresholds_text)
        message(FATAL_ERROR
            "Formal threshold scenarios differ from reporters\n"
            "Missing reference reports: [${missing_reports_text}]\n"
            "Reports without thresholds: [${missing_thresholds_text}]")
    endif()

    foreach(reporter IN LISTS reporters)
        file(READ "${reporter}" reporter_json)
        string(JSON scenario GET "${reporter_json}" scenario)
        string(JSON reporter_metric_count LENGTH "${reporter_json}" metrics)
        string(JSON threshold_metric_count LENGTH "${thresholds_json}"
            scenarios "${scenario}" metrics)
        set(reporter_metrics)
        set(threshold_metrics)
        math(EXPR last_reporter_metric "${reporter_metric_count} - 1")
        math(EXPR last_threshold_metric "${threshold_metric_count} - 1")
        foreach(metric_index RANGE 0 ${last_reporter_metric})
            string(JSON metric MEMBER "${reporter_json}" metrics ${metric_index})
            list(APPEND reporter_metrics "${metric}")
        endforeach()
        foreach(metric_index RANGE 0 ${last_threshold_metric})
            string(JSON metric MEMBER "${thresholds_json}"
                scenarios "${scenario}" metrics ${metric_index})
            list(APPEND threshold_metrics "${metric}")
        endforeach()
        list(SORT reporter_metrics)
        list(SORT threshold_metrics)
        if(NOT "${reporter_metrics}" STREQUAL "${threshold_metrics}")
            message(FATAL_ERROR
                "Formal threshold metrics differ from reporter for ${scenario}")
        endif()

        foreach(metric IN LISTS reporter_metrics)
            string(JSON unit GET "${reporter_json}" metrics "${metric}" unit)
            zz_contract_metric_kind(expected_kind "${unit}")
            string(JSON actual_kind GET "${thresholds_json}"
                scenarios "${scenario}" metrics "${metric}" metricKind)
            if(NOT "${actual_kind}" STREQUAL "${expected_kind}")
                message(FATAL_ERROR
                    "Formal threshold kind mismatch for ${scenario}/${metric}")
            endif()
            foreach(field p95 max)
                string(JSON mode GET "${thresholds_json}" scenarios "${scenario}"
                    metrics "${metric}" "${field}" mode)
                string(JSON percent GET "${thresholds_json}" scenarios "${scenario}"
                    metrics "${metric}" "${field}" percent)
                if(expected_kind STREQUAL "statistical-duration")
                    if(field STREQUAL "max" AND NOT mode STREQUAL "observe")
                        message(FATAL_ERROR
                            "Duration max must observe for ${scenario}/${metric}")
                    endif()
                    if(field STREQUAL "p95" AND mode STREQUAL "gate"
                       AND percent GREATER 10)
                        message(FATAL_ERROR
                            "Duration P95 gate exceeds 10 percent for ${scenario}/${metric}")
                    endif()
                elseif(NOT mode STREQUAL "gate" OR percent GREATER 20)
                    message(FATAL_ERROR
                        "${expected_kind} must gate within 20 percent for ${scenario}/${metric}")
                endif()
            endforeach()
        endforeach()
    endforeach()
endfunction()

zz_validate_formal_thresholds()
message(STATUS "Formal performance references are complete")
