include_directories("${PROJECT_SOURCE_DIR}/tests/support")

add_executable(ZzExampleAnimatedStackTest
    ZzExampleAnimatedStackTest.cpp ../ZzExampleTabBarsPage.cpp)
target_link_libraries(ZzExampleAnimatedStackTest PRIVATE Qt6::Test Zz::FluentUI)
set_target_properties(ZzExampleAnimatedStackTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzExampleAnimatedStackTest)
zz_enable_sanitizers(ZzExampleAnimatedStackTest)
add_test(NAME example.animated-stack COMMAND ZzExampleAnimatedStackTest)
set_tests_properties(example.animated-stack PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "example;integration" TIMEOUT 20)

add_executable(ZzExampleAudioSpectrumTest
    ZzExampleAudioSpectrumTest.cpp ../ZzExampleAudioSpectrumPage.cpp)
target_link_libraries(ZzExampleAudioSpectrumTest PRIVATE Qt6::Test Zz::FluentUI)
set_target_properties(ZzExampleAudioSpectrumTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzExampleAudioSpectrumTest)
zz_enable_sanitizers(ZzExampleAudioSpectrumTest)
add_test(NAME example.audio-spectrum COMMAND ZzExampleAudioSpectrumTest)
set_tests_properties(example.audio-spectrum PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "example;integration" TIMEOUT 20)

add_executable(ZzExampleCarouselPlaybackTest
    ZzExampleCarouselPlaybackTest.cpp
    ../ZzExampleCarouselPlayback.cpp
    ../ZzExampleCarouselPlayback.h)
target_link_libraries(ZzExampleCarouselPlaybackTest PRIVATE Qt6::Test Zz::FluentUI)
set_target_properties(ZzExampleCarouselPlaybackTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzExampleCarouselPlaybackTest)
zz_enable_sanitizers(ZzExampleCarouselPlaybackTest)
add_test(NAME example.carousel-playback COMMAND ZzExampleCarouselPlaybackTest)
set_tests_properties(example.carousel-playback PROPERTIES
    ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "example;integration" TIMEOUT 20)

# 同时构建模拟输入退化路径，确保安装 Multimedia 不会掩盖无模块版本的问题。
foreach(variant IN ITEMS simulation playback)
    if(variant STREQUAL "playback" AND NOT (ZZ_EXAMPLE_AUDIO_PLAYBACK AND TARGET Qt6::Multimedia))
        continue()
    endif()
    set(audio_source_target ZzExampleAudioMeterSourceTest_${variant})
    add_executable(${audio_source_target}
        ZzExampleAudioMeterSourceTest.cpp
        ../ZzExampleAudioMeterSource.cpp
        ../ZzExampleAudioMeterSource.h
        ../ZzExampleColorButton.h
        ../ZzExampleCustomCard.h)
    target_link_libraries(${audio_source_target} PRIVATE Qt6::Test Zz::FluentUI)
    if(variant STREQUAL "playback")
        target_sources(${audio_source_target} PRIVATE ../ZzExampleAudioLevels.cpp)
        target_link_libraries(${audio_source_target} PRIVATE Qt6::Multimedia)
        target_compile_definitions(${audio_source_target} PRIVATE ZZ_EXAMPLE_HAS_MULTIMEDIA=1)
    endif()
    set_target_properties(${audio_source_target} PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(${audio_source_target})
    zz_enable_sanitizers(${audio_source_target})
    add_test(NAME example.audio-meter-${variant} COMMAND ${audio_source_target})
    set_tests_properties(example.audio-meter-${variant} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen" LABELS "example;integration" TIMEOUT 20)
    if(variant STREQUAL "playback")
        # QAudioBufferOutput 是 Qt 6.8+ 的 FFmpeg 后端功能。
        set_property(TEST example.audio-meter-playback APPEND PROPERTY ENVIRONMENT "QT_MEDIA_BACKEND=ffmpeg")
    endif()
endforeach()

if(ZZ_EXAMPLE_AUDIO_PLAYBACK AND TARGET Qt6::Multimedia)
    add_executable(ZzExampleAudioLevelsTest ZzExampleAudioLevelsTest.cpp ../ZzExampleAudioLevels.cpp)
    target_link_libraries(ZzExampleAudioLevelsTest PRIVATE Qt6::Test Qt6::Multimedia)
    set_target_properties(ZzExampleAudioLevelsTest PROPERTIES AUTOMOC ON)
    zz_enable_project_warnings(ZzExampleAudioLevelsTest)
    add_test(NAME example.audio-levels COMMAND ZzExampleAudioLevelsTest)
    set_tests_properties(example.audio-levels PROPERTIES LABELS "example;unit")
endif()

add_executable(ZzExampleWorkspaceSmokeTest
    ZzExampleWorkspaceSmokeTest.cpp
    ../ZzExampleShowcasePage.cpp
    ../ZzExampleShowcasePagePrivate.cpp
    ../ZzExampleTabBarsPage.cpp
    ../ZzExampleRangeSliderPage.cpp
    ../ZzExampleBorderBeamPage.cpp
    ../ZzExampleAudioLevelMeterPage.cpp
    ../ZzExampleAudioSpectrumPage.cpp
    ../ZzExampleRadialGaugePage.cpp
    ../ZzExampleLiquidGaugePage.cpp
    ../ZzExampleProgressRingPage.cpp
    ../ZzExampleTimelinePage.cpp
    ../ZzExampleTimelineEventEditor.cpp
    ../ZzExampleInfoBarPage.cpp
    ../ZzExampleCarouselPage.cpp
    ../ZzExampleCarouselPlayback.cpp
    ../ZzExampleRadialGaugeHelpers.cpp
    ../ZzExampleRadialGaugePresets.cpp
    ../ZzExampleRadialGaugeEditor.cpp
    ../ZzExampleRadialRingEditor.cpp
    ../ZzExampleMultiRadialGaugeEditor.cpp
    ../ZzExampleAudioMeterSource.cpp
    ../ZzExampleColorPickerPage.cpp
    ../ZzExampleColorPickerPage.h
    ../ZzExampleColorPickerSmoke.cpp
    ../ZzExampleColorPickerSmoke.h
    ../ZzExampleColorButton.h
    ../ZzExampleCustomCard.h
    ../ZzExampleControlPage.cpp
    ../ZzExampleControlPage.h
    ../ZzExampleControlPagePrivate.cpp
    ../ZzExampleControlPagePrivate.h
    ../ZzExampleDataViewModel.cpp
    ../ZzExampleDataViewModel.h
    ../ZzExampleActivityModel.cpp
    ../ZzExampleActivityModel.h
    ../ZzExampleActivityModelPrivate.h
    ../ZzExampleAboutWindow.cpp
    ../ZzExampleAboutWindow.h
    ../ZzExampleAboutWindowPrivate.cpp
    ../ZzExampleAboutWindowPrivate.h
    ../ZzExampleApplicationContext.cpp
    ../ZzExampleApplicationContext.h
    ../ZzExampleApplicationContextPrivate.cpp
    ../ZzExampleApplicationContextPrivate.h
    ../ZzExampleLanguageController.cpp
    ../ZzExampleLanguageController.h
    ../ZzExampleRouteCatalog.cpp
    ../ZzExampleRouteCatalog.h
    ../ZzExampleSessionModel.cpp
    ../ZzExampleSessionModel.h
    ../ZzExampleFileModel.cpp
    ../ZzExampleFileModel.h
    ../ZzExampleSettingsWindow.cpp
    ../ZzExampleSettingsWindow.h
    ../ZzExampleSettingsWindowPrivate.cpp
    ../ZzExampleSettingsWindowPrivate.h
    ../ZzExampleSystemPage.cpp
    ../ZzExampleSystemPage.h
    ../ZzExampleSystemPagePrivate.cpp
    ../ZzExampleSystemPagePrivate.h
    ../ZzExampleSystemPageKind.h
    ../ZzExampleSystemPresenter.cpp
    ../ZzExampleSystemPresenter.h
    ../ZzExampleSystemPresenterPrivate.cpp
    ../ZzExampleSystemPresenterPrivate.h
    ../ZzExampleSystemViewModel.cpp
    ../ZzExampleSystemViewModel.h
    ../ZzExampleWindowShell.cpp
    ../ZzExampleWindowShell.h
    ../ZzExampleWindowShellPrivate.cpp
    ../ZzExampleWindowShellPrivate.h
    ../ZzExampleWorkspaceContent.cpp
    ../ZzExampleWorkspaceContent.h
)

qt_add_resources(ZzExampleWorkspaceSmokeTest "zz_example_workspace_test_icons"
    PREFIX "/ZzPureToolsExample/workspace-icons"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/../resources"
    FILES
        ../resources/Sessions.svg
        ../resources/Files.svg
        ../resources/Properties.svg
        ../resources/Tasks.svg
)

target_include_directories(ZzExampleWorkspaceSmokeTest PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/..")
set_source_files_properties("${zz_pure_tools_example_english_qm}"
    PROPERTIES QT_RESOURCE_ALIAS "translations/ZzPureToolsExample_en.qm")
qt_add_resources(ZzExampleWorkspaceSmokeTest "zz_example_test_translations"
    PREFIX "/"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/.."
    FILES "${zz_pure_tools_example_english_qm}" ../translations/ZzPureToolsExample_en.ts)
target_link_libraries(ZzExampleWorkspaceSmokeTest PRIVATE
    Qt6::Test
    Zz::FluentUI
    Zz::PureTools
    Zz::WindowKit
    ZzLog::ZzLog
)
set_target_properties(ZzExampleWorkspaceSmokeTest PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzExampleWorkspaceSmokeTest)
zz_enable_sanitizers(ZzExampleWorkspaceSmokeTest)

add_test(
    NAME example.workspace-smoke
    COMMAND ZzExampleWorkspaceSmokeTest
)
set(zz_example_workspace_environment "QT_QPA_PLATFORM=offscreen")
if(APPLE)
    set(zz_example_workspace_environment "QT_QPA_PLATFORM=cocoa")
endif()
set_tests_properties(example.workspace-smoke PROPERTIES
    ENVIRONMENT "${zz_example_workspace_environment}"
    LABELS "example;smoke"
    TIMEOUT 20
)
