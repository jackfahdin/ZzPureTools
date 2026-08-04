add_executable(ZzStartupProbe main.cpp)
target_link_libraries(ZzStartupProbe PRIVATE
    Zz::PureTools Zz::WindowKit Qt6::Widgets)
zz_enable_project_warnings(ZzStartupProbe)
zz_enable_sanitizers(ZzStartupProbe)
