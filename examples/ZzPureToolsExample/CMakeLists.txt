set(zz_pure_tools_example_sources
    main.cpp
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
    ZzExampleWindowShell.cpp
    ZzExampleWindowShellPrivate.cpp
)

add_executable(ZzPureToolsExample ${zz_pure_tools_example_sources})

target_link_libraries(ZzPureToolsExample PRIVATE
    Zz::FluentUI
    Zz::PureTools
    Zz::WindowKit
    ZzLog::ZzLog
)

zz_configure_first_party_target(ZzPureToolsExample
    SOURCES ${zz_pure_tools_example_sources})
