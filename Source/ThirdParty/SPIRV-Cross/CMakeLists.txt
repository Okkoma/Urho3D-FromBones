# Copyright 2016-2020 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


set(TARGET_NAME SPIRV-Cross)

option(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS "Instead of throwing exceptions assert" OFF)
option(SPIRV_CROSS_STATIC "Build the C and C++ API as static libraries." ON)

option(SPIRV_CROSS_ENABLE_GLSL "Enable GLSL support." ON)
option(SPIRV_CROSS_ENABLE_HLSL "Enable HLSL target support." ON)
option(SPIRV_CROSS_ENABLE_MSL "Enable MSL target support." ON)
option(SPIRV_CROSS_ENABLE_CPP "Enable C++ target support." ON)
option(SPIRV_CROSS_ENABLE_REFLECT "Enable JSON reflection target support." ON)

option(SPIRV_CROSS_SANITIZE_ADDRESS "Sanitize address" OFF)
option(SPIRV_CROSS_SANITIZE_MEMORY "Sanitize memory" OFF)
option(SPIRV_CROSS_SANITIZE_THREADS "Sanitize threads" OFF)
option(SPIRV_CROSS_SANITIZE_UNDEFINED "Sanitize undefined" OFF)

option(SPIRV_CROSS_NAMESPACE_OVERRIDE "" "Override the namespace used in the C++ API.")
option(SPIRV_CROSS_FORCE_STL_TYPES "Force use of STL types instead of STL replacements in certain places. Might reduce performance." OFF)

option(SPIRV_CROSS_WERROR "Fail build on warnings." OFF)
option(SPIRV_CROSS_MISC_WARNINGS "Misc warnings useful for Travis runs." OFF)

option(SPIRV_CROSS_FORCE_PIC "Force position-independent code for all targets." OFF)

if(${CMAKE_GENERATOR} MATCHES "Makefile")
	if(${CMAKE_CURRENT_SOURCE_DIR} STREQUAL ${CMAKE_CURRENT_BINARY_DIR})
		message(FATAL_ERROR "Build out of tree to avoid overwriting Makefile")
	endif()
endif()

set(spirv-compiler-options "")
set(spirv-compiler-defines "")
set(spirv-cross-link-flags "")
set(spirv-cross-build-version "unknown")

string(TIMESTAMP spirv-cross-timestamp)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/gitversion.in.h ${CMAKE_CURRENT_BINARY_DIR}/gitversion.h @ONLY)

if (SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
	set(spirv-compiler-defines ${spirv-compiler-defines} SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
	if (NOT MSVC)
		set(spirv-compiler-options ${spirv-compiler-options} -fno-exceptions)
	endif()
endif()

if (SPIRV_CROSS_FORCE_STL_TYPES)
	set(spirv-compiler-defines ${spirv-compiler-defines} SPIRV_CROSS_FORCE_STL_TYPES)
endif()

if (CMAKE_COMPILER_IS_GNUCXX OR (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang"))
	set(spirv-compiler-options ${spirv-compiler-options} -Wall -Wextra -Wshadow)
	if (SPIRV_CROSS_MISC_WARNINGS)
		if (${CMAKE_CXX_COMPILER_ID} MATCHES "Clang")
			set(spirv-compiler-options ${spirv-compiler-options} -Wshorten-64-to-32)
		endif()
	endif()
	if (SPIRV_CROSS_WERROR)
		set(spirv-compiler-options ${spirv-compiler-options} -Werror)
	endif()

	if (SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS)
		set(spirv-compiler-options ${spirv-compiler-options} -fno-exceptions)
	endif()

	if (SPIRV_CROSS_SANITIZE_ADDRESS)
		set(spirv-compiler-options ${spirv-compiler-options} -fsanitize=address)
		set(spirv-cross-link-flags "${spirv-cross-link-flags} -fsanitize=address")
	endif()

	if (SPIRV_CROSS_SANITIZE_UNDEFINED)
		set(spirv-compiler-options ${spirv-compiler-options} -fsanitize=undefined)
		set(spirv-cross-link-flags "${spirv-cross-link-flags} -fsanitize=undefined")
	endif()

	if (SPIRV_CROSS_SANITIZE_MEMORY)
		set(spirv-compiler-options ${spirv-compiler-options} -fsanitize=memory)
		set(spirv-cross-link-flags "${spirv-cross-link-flags} -fsanitize=memory")
	endif()

	if (SPIRV_CROSS_SANITIZE_THREADS)
		set(spirv-compiler-options ${spirv-compiler-options} -fsanitize=thread)
		set(spirv-cross-link-flags "${spirv-cross-link-flags} -fsanitize=thread")
	endif()
elseif (MSVC)
	set(spirv-compiler-options ${spirv-compiler-options} /wd4267 /wd4996)
endif()

macro(extract_headers out_abs file_list)
	set(${out_abs}) # absolute paths
	foreach(_a ${file_list})
		# get_filename_component only returns the longest extension, so use a regex
		string(REGEX REPLACE ".*\\.(h|hpp)" "\\1" ext ${_a})

        if(("${ext}" STREQUAL "h") OR ("${ext}" STREQUAL "hpp"))
            list(APPEND ${out_abs} "${_a}")
        endif()
	endforeach()
endmacro()

macro(spirv_cross_add_library name config_name library_type)
	add_library(${name} ${library_type} ${ARGN})
	extract_headers(hdrs "${ARGN}")
	target_include_directories(${name} PUBLIC
			$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
			$<INSTALL_INTERFACE:include/spirv_cross>)
	set_target_properties(${name} PROPERTIES
			PUBLIC_HEADERS "${hdrs}")
	if (SPIRV_CROSS_FORCE_PIC)
		set_target_properties(${name} PROPERTIES POSITION_INDEPENDENT_CODE ON)
	endif()
	target_compile_options(${name} PRIVATE ${spirv-compiler-options})
	target_compile_definitions(${name} PRIVATE ${spirv-compiler-defines})
	if (SPIRV_CROSS_NAMESPACE_OVERRIDE)
        target_compile_definitions(${name} PUBLIC SPIRV_CROSS_NAMESPACE_OVERRIDE=${SPIRV_CROSS_NAMESPACE_OVERRIDE})
	endif()
endmacro()

set(spirv-cross-core-sources
		${CMAKE_CURRENT_SOURCE_DIR}/GLSL.std.450.h
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_common.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_containers.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_error_handling.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_parser.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_parser.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_parsed_ir.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_parsed_ir.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cfg.hpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cfg.cpp)

set(spirv-cross-c-sources
		spirv.h
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_c.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_c.h)

set(spirv-cross-glsl-sources
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_glsl.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_glsl.hpp)

set(spirv-cross-cpp-sources
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cpp.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cpp.hpp)

set(spirv-cross-msl-sources
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_msl.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_msl.hpp)

set(spirv-cross-hlsl-sources
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_hlsl.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_hlsl.hpp)

set(spirv-cross-reflect-sources
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_reflect.hpp)

set(spirv-cross-util-sources
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_util.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/spirv_cross_util.hpp)


spirv_cross_add_library(spirv-cross-core spirv_cross_core STATIC
        ${spirv-cross-core-sources})

if (SPIRV_CROSS_ENABLE_GLSL)
    spirv_cross_add_library(spirv-cross-glsl spirv_cross_glsl STATIC
            ${spirv-cross-glsl-sources})
    target_link_libraries(spirv-cross-glsl PRIVATE spirv-cross-core)
endif()

if (SPIRV_CROSS_ENABLE_CPP)
    spirv_cross_add_library(spirv-cross-cpp spirv_cross_cpp STATIC
            ${spirv-cross-cpp-sources})

    if (SPIRV_CROSS_ENABLE_GLSL)
        target_link_libraries(spirv-cross-cpp PRIVATE spirv-cross-glsl)
    else()
        message(FATAL_ERROR "Must enable GLSL support to enable C++ support.")
    endif()
endif()

if (SPIRV_CROSS_ENABLE_REFLECT)
    if (SPIRV_CROSS_ENABLE_GLSL)
        spirv_cross_add_library(spirv-cross-reflect spirv_cross_reflect STATIC
                ${spirv-cross-reflect-sources})
    else()
        message(FATAL_ERROR "Must enable GLSL support to enable JSON reflection support.")
    endif()
endif()

if (SPIRV_CROSS_ENABLE_MSL)
    spirv_cross_add_library(spirv-cross-msl spirv_cross_msl STATIC
            ${spirv-cross-msl-sources})
    if (SPIRV_CROSS_ENABLE_GLSL)
        target_link_libraries(spirv-cross-msl PRIVATE spirv-cross-glsl)
    else()
        message(FATAL_ERROR "Must enable GLSL support to enable MSL support.")
    endif()
endif()

if (SPIRV_CROSS_ENABLE_HLSL)
    spirv_cross_add_library(spirv-cross-hlsl spirv_cross_hlsl STATIC
            ${spirv-cross-hlsl-sources})
    if (SPIRV_CROSS_ENABLE_GLSL)
        target_link_libraries(spirv-cross-hlsl PRIVATE spirv-cross-glsl)
    else()
        message(FATAL_ERROR "Must enable GLSL support to enable HLSL support.")
    endif()
endif()

set(spirv-cross-abi-major 0)
set(spirv-cross-abi-minor 24)
set(spirv-cross-abi-patch 0)

# Install the headers for Urho3D
install_header_files (DIRECTORY ./ DESTINATION ${DEST_INCLUDE_DIR}/ThirdParty/SPIRV-Cross FILES_MATCHING PATTERN *.hpp *.h BUILD_TREE_ONLY)  # Note: the trailing slash is significant

