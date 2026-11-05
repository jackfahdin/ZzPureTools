# Keep the upstream C files byte-identical; KissFFT supports compiling as C++.
add_library(ZzKissFftPrivate OBJECT kiss_fft.c kiss_fftr.c)
set_source_files_properties(kiss_fft.c kiss_fftr.c PROPERTIES LANGUAGE CXX)
set_target_properties(ZzKissFftPrivate PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN ON
    AUTOMOC OFF
    AUTOUIC OFF
    AUTORCC OFF
)

# Prefix every external entry point, including in static builds where hidden
# visibility alone cannot prevent a collision with an application's KissFFT.
target_compile_definitions(ZzKissFftPrivate PRIVATE
    kiss_fft_alloc=zz_fluent_kiss_fft_alloc
    kiss_fft=zz_fluent_kiss_fft
    kiss_fft_stride=zz_fluent_kiss_fft_stride
    kiss_fft_cleanup=zz_fluent_kiss_fft_cleanup
    kiss_fft_next_fast_size=zz_fluent_kiss_fft_next_fast_size
    kiss_fftr_alloc=zz_fluent_kiss_fftr_alloc
    kiss_fftr=zz_fluent_kiss_fftr
    kiss_fftri=zz_fluent_kiss_fftri
)
target_include_directories(ZzKissFftPrivate PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# Use object sources rather than a link dependency so installed static targets
# never reference a private, unexported third-party CMake target.
function(zz_use_private_kissfft target)
    target_sources(${target} PRIVATE $<TARGET_OBJECTS:ZzKissFftPrivate>)
    get_target_property(kissfft_definitions ZzKissFftPrivate COMPILE_DEFINITIONS)
    target_compile_definitions(${target} PRIVATE ${kissfft_definitions})
    get_target_property(kissfft_includes ZzKissFftPrivate INCLUDE_DIRECTORIES)
    target_include_directories(${target} SYSTEM PRIVATE ${kissfft_includes})
endfunction()
