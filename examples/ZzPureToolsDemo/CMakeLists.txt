add_executable(ZzPureToolsDemo
    main.cpp
    ZzDemoModule.cpp
    ZzDemoPageFactory.cpp
)

target_link_libraries(ZzPureToolsDemo PRIVATE
    Zz::PureTools
    Zz::WindowKit
)

zz_enable_project_warnings(ZzPureToolsDemo)
zz_enable_sanitizers(ZzPureToolsDemo)
