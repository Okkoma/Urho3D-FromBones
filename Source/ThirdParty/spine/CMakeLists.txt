# Define target name
set (TARGET_NAME spine)

if(MSVC)
	message("MSCV detected")
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
	set (CMAKE_C_FLAGS "${CMAKE_CXX_FLAGS}")
else()
	set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -pedantic -std=c89")
	set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wnon-virtual-dtor -pedantic -std=c++11 -fno-exceptions -fno-rtti")
endif()

# Define source files
define_source_files (GLOB_CPP_PATTERNS spine-c/src/spine/*.c GLOB_H_PATTERNS spine-c/include/spine/*.h)

# Define dependency libs
set (INCLUDE_DIRS spine-c/include)

# Setup target
setup_library ()

# Install headers for building the Urho3D library
install_header_files (DIRECTORY spine-c/include/spine DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty FILES_MATCHING PATTERN *.h)

