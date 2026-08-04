add_executable(ZzIdleProbe main.cpp)
target_link_libraries(ZzIdleProbe PRIVATE
    ZzBenchmarkSupport Zz::PureTools Zz::FluentUI Zz::WindowKit
    Qt6::Widgets)
zz_enable_project_warnings(ZzIdleProbe)
zz_enable_sanitizers(ZzIdleProbe)
