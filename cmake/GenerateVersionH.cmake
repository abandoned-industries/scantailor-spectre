# GenerateVersionH.cmake
# Generates version.h with build timestamp at build time

# Generate timestamp in format YYYYMMDD.HHMM
string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d.%H%M")

# Read the template file
file(READ "${VERSION_H_IN}" VERSION_H_CONTENT)

# Replace the placeholder with actual timestamp
string(REPLACE "@BUILD_TIMESTAMP@" "${BUILD_TIMESTAMP}" VERSION_H_CONTENT "${VERSION_H_CONTENT}")

# Write the output file
file(WRITE "${VERSION_H_OUT}" "${VERSION_H_CONTENT}")

message(STATUS "Generated version.h with BUILD_TIMESTAMP=${BUILD_TIMESTAMP}")
