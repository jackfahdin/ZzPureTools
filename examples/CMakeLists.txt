# 各组件实施计划按既定顺序在此追加示例子目录。

add_subdirectory(ZzWindowKitDemo)
add_subdirectory(ZzFluentFoundationDemo)
add_subdirectory(ZzFluentControlsGallery)
add_subdirectory(ZzPureToolsDemo)

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
endif()
