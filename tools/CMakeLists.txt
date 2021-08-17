# Copyright (c) 2021 HiSilicon Technologies Co., Ltd.
# Wayca scheduler is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
# You may obtain a copy of Mulan PSL v2 at:
#          http://license.coscl.org.cn/MulanPSL2
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
# EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
# MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
#
# See the Mulan PSL v2 for more details.

# wayca-sc-info
set(WAYCA_SC_INFO_NAME ${WAYCA_SC_PREFIX}-info)
include_directories(wayca-sc-info/ ${PROJECT_SOURCE_DIR}/lib ${PROJECT_SOURCE_DIR})
set(WAYCA_SC_INFO_SRCS
    wayca-sc-info/wayca_sc_info.c
    wayca-sc-info/wayca_sc_topo.c
    ${PROJECT_SOURCE_DIR}/lib/log.c)
add_executable(${WAYCA_SC_INFO_NAME} ${WAYCA_SC_INFO_SRCS})

find_package(LibXml2 REQUIRED)
if(LibXml2_FOUND)
	include_directories(${LIBXML2_INCLUDE_DIR})
	target_link_libraries(${WAYCA_SC_INFO_NAME} xml2 ${WAYCA_SC_LIB_NAME})
else(LibXml2_FOUND)
	message(FATAL_ERROR "LibXml2 not found")
endif(LibXml2_FOUND)

install(
	TARGETS ${WAYCA_SC_INFO_NAME}
	RUNTIME DESTINATION ${WAYCA_SC_INSTALL_PREFIX}/bin/
)
