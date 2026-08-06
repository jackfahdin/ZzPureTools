set(zz_pure_tools_example_sources
    main.cpp
    ZzExampleActivityModel.cpp
    ZzExampleApplicationContext.cpp
    ZzExampleApplicationContextPrivate.cpp
    ZzExampleApplicationModule.cpp
    ZzExampleApplicationModulePrivate.cpp
    ZzExampleCardsPage.cpp
    ZzExampleCardsPagePrivate.cpp
    ZzExampleCardsViewModel.cpp
    ZzExampleDataPage.cpp
    ZzExampleDataPagePrivate.cpp
    ZzExampleDataPresenter.cpp
    ZzExampleDataViewModel.cpp
    ZzExampleGalleryPage.cpp
    ZzExampleGalleryPagePrivate.cpp
    ZzExampleNavigationPresenter.cpp
    ZzExamplePageFactory.cpp
    ZzExampleRouteCatalog.cpp
    ZzExampleShowcasePage.cpp
    ZzExampleShowcasePagePrivate.cpp
    ZzExampleSystemPage.cpp
    ZzExampleSystemPagePrivate.cpp
    ZzExampleSystemPresenter.cpp
    ZzExampleSystemPresenterPrivate.cpp
    ZzExampleSystemViewModel.cpp
    ZzExampleWindowShell.cpp
    ZzExampleWindowShellPrivate.cpp
)

add_executable(ZzPureToolsExample ${zz_pure_tools_example_sources})

find_package(Qt6 6.8 REQUIRED COMPONENTS LinguistTools)
qt_add_lrelease(ZzPureToolsExample
    TS_FILES translations/ZzPureToolsExample_en.ts
    OPTIONS -nounfinished
    QM_FILES_OUTPUT_VARIABLE zz_pure_tools_example_qm_files
)
list(LENGTH zz_pure_tools_example_qm_files
    zz_pure_tools_example_qm_count)
if(NOT zz_pure_tools_example_qm_count EQUAL 1)
    message(FATAL_ERROR "Expected exactly one example translation QM file")
endif()
list(GET zz_pure_tools_example_qm_files 0
    zz_pure_tools_example_english_qm)
set_source_files_properties(
    "${zz_pure_tools_example_english_qm}"
    PROPERTIES QT_RESOURCE_ALIAS "ZzPureToolsExample_en.qm")
qt_add_resources(ZzPureToolsExample "zz_example_translations"
    PREFIX "/translations"
    FILES "${zz_pure_tools_example_english_qm}"
)

target_link_libraries(ZzPureToolsExample PRIVATE
    Zz::FluentUI
    Zz::PureTools
    Zz::WindowKit
    ZzLog::ZzLog
)

zz_configure_first_party_target(ZzPureToolsExample
    SOURCES ${zz_pure_tools_example_sources})
