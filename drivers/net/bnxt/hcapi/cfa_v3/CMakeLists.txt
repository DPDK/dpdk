#
# Copyright(c) 2019-2021 Broadcom Limited, all rights reserved
# Contains proprietary and confidential information.
#
# This source file is the property of Broadcom Limited, and
# may not be copied or distributed in any isomorphic form without
# the prior written consent of Broadcom Limited.
#

# Needed for compilation with chip-specific regdef.h
add_definitions(-DFIRMWARE_VIEW=1)

# Platform specific defines
if (cfa_p70)
    add_definitions(-DSUPPORT_CFA_HW_P70=1)
    set(PXX_FOLDER p70)
    set (tsm_needed 1)
    set (mm_needed 1)
endif ()

if (cfa_p80)
    add_definitions(-DSUPPORT_CFA_HW_P80=1)
    set(PXX_FOLDER p80)
    set (tcm_needed 1)
endif ()

# Reset Doc dir variables
set(CFA_API_DOC_DIRS "" CACHE INTERNAL "")
set(CFA_DESIGN_DOC_DIRS "" CACHE INTERNAL "")
set(CFA_UT_DOC_DIRS "" CACHE INTERNAL "")

# Include sub directories

if (idm_needed)
    add_subdirectory(idm)
    set(idm_libs cfa-idm-lib cfa-idm-lib-ut)
endif ()

if (tbm_needed)
    add_subdirectory(tbm)
    set(tbm_libs cfa-tbm-lib cfa-tbm-lib-ut)
endif ()

if (gim_needed)
    add_subdirectory(gim)
    set(gim_libs cfa-gim-lib cfa-gim-lib-ut)
endif ()

if (mm_needed)
    add_subdirectory(mm)
    set(mm_libs cfa-mm-lib cfa-mm-lib-ut)
endif ()

if (tsm_needed)
    add_subdirectory(tpm)
    add_subdirectory(tim)
    set(cfa-tim-lib cfa-tim-lib-ut cfa-tpm-lib cfa-tpm-lib-ut)
endif ()

if (tcm_needed)
    add_subdirectory(tcm)
    set(tcm_libs cfa-tcm-lib cfa-tcm-lib-ut)
endif ()

if (rdm_needed)
    add_subdirectory(rdm)
    set(rdm_libs cfa-rdm-lib cfa-rdm-lib-ut)
endif ()

# Update Doxygen dirs for api documentation
#set(CFA_API_DOC_DIRS ${CFA_API_DOC_DIRS}
#    ${CMAKE_CURRENT_SOURCE_DIR}/include
#    CACHE INTERNAL "")

# Update Doxygen dirs for design documentation
#set(CFA_DESIGN_DOC_DIRS ${CFA_DESIGN_DOC_DIRS}
#    ${CMAKE_CURRENT_SOURCE_DIR}/include
#    CACHE INTERNAL "")

# Include docs
#if (DOXYGEN_FOUND)
#    add_subdirectory(docs)
#    add_custom_target(cfa-v3-docs
#        DEPENDS hcapi-cfa-api-docs
#                hcapi-cfa-design-docs
#                hcapi-cfa-ut-docs
#        )
#endif (DOXYGEN_FOUND)

add_custom_target(cfa-v3-libs
    ALL
    DEPENDS ${tpm_libs} ${tim_libs} ${mm_libs})
