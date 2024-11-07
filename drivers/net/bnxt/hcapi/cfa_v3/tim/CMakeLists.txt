#
# Copyright(c) 2021 Broadcom Limited, all rights reserved
# Contains proprietary and confidential information.
#
# This source file is the property of Broadcom Limited, and
# may not be copied or distributed in any isomorphic form without
# the prior written consent of Broadcom Limited.
#

add_library(cfa-tim-lib-common INTERFACE)
target_include_directories(cfa-tim-lib-common INTERFACE include
						../include
						../../include
						../../../tf_core)

set (CFA_TIM_SRCS cfa_tim.c)

# Production version
add_library(cfa-tim-lib STATIC EXCLUDE_FROM_ALL ${CFA_TIM_SRCS})
set_property(TARGET cfa-tim-lib PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(cfa-tim-lib PUBLIC cfa-tim-lib-common nxt-platform nxt-arch)
target_include_directories(cfa-tim-lib PUBLIC ../include/platform/fw)

# UT version
add_library(cfa-tim-lib-ut STATIC EXCLUDE_FROM_ALL ${CFA_TIM_SRCS})
set_property(TARGET cfa-tim-lib-ut PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(cfa-tim-lib-ut PUBLIC  cfa-tim-lib-common nxt-ut nxt-platform nxt-arch nxt-env-ut)
target_include_directories(cfa-tim-lib-ut PUBLIC ../include/platform/ut)

set(ignoreMe "${SKIP_TIM_UT}")
if(NOT DEFINED SKIP_TIM_UT)
add_subdirectory(ut)
endif()

# Update Doxygen Path for tim api documentation
set(CFA_API_DOC_DIRS ${CFA_API_DOC_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/include # Public api
	CACHE INTERNAL "")

# Update Doxygen Path for tim design documentation
set(CFA_DESIGN_DOC_DIRS ${CFA_DESIGN_DOC_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}         # tim implementation
	CACHE INTERNAL "")
