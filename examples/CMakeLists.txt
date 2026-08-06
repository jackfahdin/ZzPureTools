# 各组件实施计划按既定顺序在此追加示例子目录。

add_subdirectory(ZzWindowKitDemo)
add_subdirectory(ZzFluentFoundationDemo)
add_subdirectory(ZzFluentControlsGallery)
add_subdirectory(ZzPureToolsDemo)
add_subdirectory(ZzPureToolsExample)

if(ZZ_BUILD_TESTS AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
    function(zz_add_linux_example_smoke test_name target timeout_variable)
        add_test(
            NAME "example.${test_name}"
            COMMAND "$<TARGET_FILE:${target}>"
        )
        set_tests_properties("example.${test_name}" PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;${timeout_variable}=100"
            LABELS "example;smoke"
            TIMEOUT 10
        )
    endfunction()

    zz_add_linux_example_smoke(
        windowkit ZzWindowKitDemo ZZ_WINDOWKIT_DEMO_AUTO_CLOSE_MS)
    zz_add_linux_example_smoke(
        fluent-foundation ZzFluentFoundationDemo ZZ_FLUENT_DEMO_AUTO_CLOSE_MS)
    zz_add_linux_example_smoke(
        fluent-controls ZzFluentControlsGallery ZZ_FLUENT_GALLERY_AUTO_CLOSE_MS)
    zz_add_linux_example_smoke(
        puretools ZzPureToolsDemo ZZ_PURETOOLS_DEMO_AUTO_CLOSE_MS)
    zz_add_linux_example_smoke(
        puretools-integration
        ZzPureToolsExample
        ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS)

    add_test(
        NAME example.puretools-integration-english
        COMMAND "$<TARGET_FILE:ZzPureToolsExample>"
    )
    set_tests_properties(
        example.puretools-integration-english
        PROPERTIES
            ENVIRONMENT
                "QT_QPA_PLATFORM=offscreen;ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=100;ZZ_PURETOOLS_EXAMPLE_EXPECT_ENGLISH=1;LANG=C.UTF-8;LC_ALL=C.UTF-8;LANGUAGE=en"
            LABELS "example;smoke;translation"
            TIMEOUT 10
    )

    foreach(zz_close_scenario IN ITEMS cancel minimize confirm)
        add_test(
            NAME "example.puretools-close-${zz_close_scenario}"
            COMMAND "$<TARGET_FILE:ZzPureToolsExample>"
        )
        set_tests_properties(
            "example.puretools-close-${zz_close_scenario}"
            PROPERTIES
                ENVIRONMENT
                    "QT_QPA_PLATFORM=offscreen;ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1000;ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO=close-${zz_close_scenario}"
                LABELS "example;smoke;close-guard"
                TIMEOUT 10
        )
    endforeach()

    add_test(
        NAME example.puretools-multi-window
        COMMAND "$<TARGET_FILE:ZzPureToolsExample>"
    )
    set_tests_properties(example.puretools-multi-window PROPERTIES
        ENVIRONMENT
            "QT_QPA_PLATFORM=offscreen;ZZ_PURETOOLS_EXAMPLE_AUTO_CLOSE_MS=1000;ZZ_PURETOOLS_EXAMPLE_SMOKE_SCENARIO=multi-window"
        LABELS "example;smoke;multi-window"
        TIMEOUT 10)
endif()
