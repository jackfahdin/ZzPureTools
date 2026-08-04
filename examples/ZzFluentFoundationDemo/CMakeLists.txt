add_executable(ZzFluentFoundationDemo main.cpp)
target_link_libraries(ZzFluentFoundationDemo PRIVATE
    Zz::FluentUI
    Qt6::Widgets
)
zz_enable_project_warnings(ZzFluentFoundationDemo)
zz_enable_sanitizers(ZzFluentFoundationDemo)
