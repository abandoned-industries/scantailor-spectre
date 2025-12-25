# GenerateVersionH.cmake - Generate version.h with current build timestamp
# Called at build time to ensure timestamp is always fresh

string(TIMESTAMP BUILD_TIMESTAMP "%Y%m%d.%H%M")

# Read input template
file(READ "${VERSION_H_IN}" VERSION_H_CONTENT)

# Substitute the placeholder
string(REPLACE "@BUILD_TIMESTAMP@" "${BUILD_TIMESTAMP}" VERSION_H_CONTENT "${VERSION_H_CONTENT}")

# Always write output (force update even if content is same, to trigger recompile)
file(WRITE "${VERSION_H_OUT}" "${VERSION_H_CONTENT}")
