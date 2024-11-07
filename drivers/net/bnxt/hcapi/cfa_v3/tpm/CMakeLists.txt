#
# Copyright(c) 2021 Broadcom Limited, all rights reserved
# Contains proprietary and confidential information.
#
# This source file is the property of Broadcom Limited, and
# may not be copied or distributed in any isomorphic form without
# the prior written consent of Broadcom Limited.
#

add_library(cfa-tpm-lib-common INTERFACE)
target_include_directories(cfa-tpm-lib-common INTERFACE include
						../include
						../../include
						../../generic-common/include
						../../../tf_core)

set (CFA_TPM_SRCS cfa_tpm.c)

# Production version
add_library(cfa-tpm-lib STATIC EXCLUDE_FROM_ALL ${CFA_TPM_SRCS})
set_property(TARGET cfa-tpm-lib PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(cfa-tpm-lib PUBLIC cfa-tpm-lib-common nxt-platform nxt-arch)
target_include_directories(cfa-tpm-lib PUBLIC ../include/platform/fw)

# UT version
add_library(cfa-tpm-lib-ut STATIC EXCLUDE_FROM_ALL ${CFA_TPM_SRCS})
set_property(TARGET cfa-tpm-lib-ut PROPERTY POSITION_INDEPENDENT_CODE 1)
target_link_libraries(cfa-tpm-lib-ut PUBLIC cfa-tpm-lib-common nxt-ut nxt-platform nxt-arch nxt-env-ut)
target_include_directories(cfa-tpm-lib-ut PUBLIC ../include/platform/ut)

set(ignoreMe "${SKIP_TPM_UT}")
if(NOT DEFINED SKIP_TPM_UT)
add_subdirectory(ut)
endif()

# Update Doxygen Path for tpm api documentation
set(CFA_API_DOC_DIRS ${CFA_API_DOC_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}/include # Public api
	CACHE INTERNAL "")

# Update Doxygen Path for tpm design documentation
set(CFA_DESIGN_DOC_DIRS ${CFA_DESIGN_DOC_DIRS}
	${CMAKE_CURRENT_SOURCE_DIR}         # tpm implementation
	CACHE INTERNAL "")
