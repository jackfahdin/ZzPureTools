add_executable(ZzFluentControlsGallery
    main.cpp
    ZzFluentControlsGallery.cpp
    ZzFluentControlsGalleryPrivate.cpp
)
qt_add_resources(ZzFluentControlsGallery "zzfluent-gallery-icons"
    PREFIX "/zzfluent/gallery"
    BASE "${CMAKE_CURRENT_SOURCE_DIR}/resources"
    FILES
        resources/ZzGalleryRefresh.svg
)
target_link_libraries(ZzFluentControlsGallery PRIVATE
    Qt6::Widgets
    Zz::FluentUI
)
set_target_properties(ZzFluentControlsGallery PROPERTIES AUTOMOC ON)
zz_enable_project_warnings(ZzFluentControlsGallery)
zz_enable_sanitizers(ZzFluentControlsGallery)
