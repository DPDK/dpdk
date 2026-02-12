#
# Copyright(c) 2021 Broadcom Limited, all rights reserved
# Contains proprietary and confidential information.
#
# This source file is the property of Broadcom Limited, and
# may not be copied or distributed in any isomorphic form without
# the prior written consent of Broadcom Limited.
#

add_library(cfa-mm-lib-common INTERFACE)
target_include_directories(cfa-mm-lib-common INTERFACE include
						../include
						../../include)

set (CFA_MM_SRCS cfa_mm.c)

# Production version
add_library(cfa-mm-lib STATIC EXCLUDE_FROM_ALL ${CFA_MM_SRCS})
set_property(TARGET cfa-mm-lib PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(cfa-mm-lib PUBLIC cfa-mm-lib-common nxt-platform nxt-arch)
target_include_directories(cfa-mm-lib PUBLIC ../include/platform/fw)

# UT version
add_library(cfa-mm-lib-ut STATIC EXCLUDE_FROM_ALL ${CFA_MM_SRCS})
set_property(TARGET cfa-mm-lib-ut PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(cfa-mm-lib-ut PUBLIC cfa-mm-lib-common nxt-ut nxt-platform nxt-arch nxt-env-ut)
target_include_directories(cfa-mm-lib-ut PUBLIC ../include/platform/ut)

set(ignoreMe "${SKIP_MM_UT}")
if(NOT DEFINED SKIP_MM_UT)
add_subdirectory(ut)
endif()

# Update Doxygen Path for mm api documentation
set(CFA_API_DOC_DIRS ${CFA_API_DOC_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/include # Public api
	CACHE INTERNAL "")

# Update Doxygen Path for mm design documentation
set(CFA_DESIGN_DOC_DIRS ${CFA_DESIGN_DOC_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}         # mm implementation
	CACHE INTERNAL "")
