# Read current build number
if(EXISTS ${BUILD_NUMBER_FILE})
    file(READ ${BUILD_NUMBER_FILE} BUILD_NUMBER)
    string(STRIP ${BUILD_NUMBER} BUILD_NUMBER)
else()
    set(BUILD_NUMBER 0)
endif()

# Increment build number
math(EXPR BUILD_NUMBER "${BUILD_NUMBER} + 1")

# Write new build number back to file
file(WRITE ${BUILD_NUMBER_FILE} ${BUILD_NUMBER})

# Configure the version template
configure_file(${VERSION_TEMPLATE} ${VERSION_OUTPUT} @ONLY)

message(STATUS "Build number incremented to: ${BUILD_NUMBER}")