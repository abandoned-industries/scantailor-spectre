# FindLibHaru.cmake
# Find the libharu PDF library
#
# This module defines:
#   LibHaru_FOUND        - True if libharu was found
#   LibHaru_INCLUDE_DIRS - The include directories
#   LibHaru_LIBRARIES    - The libraries to link against
#   LibHaru::LibHaru     - Imported target

# Look for the header file
find_path(LibHaru_INCLUDE_DIR
    NAMES hpdf.h
    PATHS
        /opt/homebrew/include      # macOS ARM Homebrew
        /usr/local/include         # macOS Intel Homebrew / Linux
        /usr/include
    PATH_SUFFIXES libharu
)

# Look for the library
find_library(LibHaru_LIBRARY
    NAMES hpdf libhpdf haru libharu
    PATHS
        /opt/homebrew/lib          # macOS ARM Homebrew
        /usr/local/lib             # macOS Intel Homebrew / Linux
        /usr/lib
        /usr/lib/x86_64-linux-gnu
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(LibHaru
    REQUIRED_VARS LibHaru_LIBRARY LibHaru_INCLUDE_DIR
)

if(LibHaru_FOUND)
    set(LibHaru_LIBRARIES ${LibHaru_LIBRARY})
    set(LibHaru_INCLUDE_DIRS ${LibHaru_INCLUDE_DIR})

    if(NOT TARGET LibHaru::LibHaru)
        add_library(LibHaru::LibHaru UNKNOWN IMPORTED)
        set_target_properties(LibHaru::LibHaru PROPERTIES
            IMPORTED_LOCATION "${LibHaru_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LibHaru_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(LibHaru_INCLUDE_DIR LibHaru_LIBRARY)
