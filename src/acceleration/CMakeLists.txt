# Metal GPU Acceleration module
# Only built on Apple platforms

if(APPLE)
    find_library(METAL_FRAMEWORK Metal REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation REQUIRED)

    # Metal shader compilation
    set(METAL_SOURCES
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gauss_blur.metal
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/morphology.metal
    )
    set(METALLIB_OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/default.metallib)

    # Compile .metal -> .air -> .metallib (compile all shaders then link)
    add_custom_command(
        OUTPUT ${METALLIB_OUTPUT}
        COMMAND xcrun -sdk macosx metal -c ${CMAKE_CURRENT_SOURCE_DIR}/shaders/gauss_blur.metal -o ${CMAKE_CURRENT_BINARY_DIR}/gauss_blur.air
        COMMAND xcrun -sdk macosx metal -c ${CMAKE_CURRENT_SOURCE_DIR}/shaders/morphology.metal -o ${CMAKE_CURRENT_BINARY_DIR}/morphology.air
        COMMAND xcrun -sdk macosx metallib ${CMAKE_CURRENT_BINARY_DIR}/gauss_blur.air ${CMAKE_CURRENT_BINARY_DIR}/morphology.air -o ${METALLIB_OUTPUT}
        DEPENDS ${METAL_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Compiling Metal shaders"
    )

    # Custom target to ensure metallib is built
    add_custom_target(metal_shaders DEPENDS ${METALLIB_OUTPUT})

    set(sources
        MetalContext.h
        MetalContext.mm
        MetalGaussBlur.h
        MetalGaussBlur.mm
        MetalMorphology.h
        MetalMorphology.mm
    )

    add_library(acceleration STATIC ${sources})
    add_dependencies(acceleration metal_shaders)

    target_link_libraries(acceleration
        PUBLIC ${METAL_FRAMEWORK}
        PUBLIC ${FOUNDATION_FRAMEWORK}
    )

    target_include_directories(acceleration PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

    # Set metallib as a resource to be copied to app bundle
    set(SCANTAILOR_METALLIB ${METALLIB_OUTPUT} CACHE INTERNAL "Path to compiled Metal shader library")
endif()
