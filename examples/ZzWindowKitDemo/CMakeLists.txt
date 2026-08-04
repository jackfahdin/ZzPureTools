add_executable(ZzWindowKitDemo
    main.cpp
    ZzWindowKitDemoWindow.cpp
    ZzWindowKitDemoWindowPrivate.cpp
)
target_link_libraries(ZzWindowKitDemo PRIVATE
    Qt6::Widgets
    Zz::WindowKit
)
zz_enable_project_warnings(ZzWindowKitDemo)
zz_enable_sanitizers(ZzWindowKitDemo)
