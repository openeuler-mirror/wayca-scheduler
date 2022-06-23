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

# wayca-calibration
set(WAYCA_CALIBRATION_TOOL wayca_calibration.py)
set(WAYCA_CALIBRATION_TOOL_SRC ${PROJECT_SOURCE_DIR}/tools/wayca-calibration/${WAYCA_CALIBRATION_TOOL})
set(WAYCA_CALIBRATION_TOOL_LINK wayca-calibration)
set(WAYCA_CALIBRATION_CONF wayca-calibration/wayca_calibration.conf)

add_custom_target(${WAYCA_CALIBRATION_TOOL} ALL
	COMMAND ${CMAKE_COMMAND} -E copy
	${WAYCA_CALIBRATION_TOOL_SRC} ${WAYCA_CALIBRATION_TOOL}

	COMMAND sed -i "/WAYCA_CALIBRATION_VERSION =/s/0.0/\'${WAYCA_SCHEDULER_VERSION}\'/g"
	${WAYCA_CALIBRATION_TOOL}

	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${WAYCA_CALIBRATION_TOOL} ${WAYCA_CALIBRATION_TOOL_LINK}
)

install(
	PROGRAMS
	${CMAKE_CURRENT_BINARY_DIR}/${WAYCA_CALIBRATION_TOOL}
	${CMAKE_CURRENT_BINARY_DIR}/${WAYCA_CALIBRATION_TOOL_LINK}
	DESTINATION ${WAYCA_SC_INSTALL_PREFIX}/bin/
)

install(
	FILES
	${WAYCA_CALIBRATION_CONF}
	DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/wayca-scheduler
)

# wayca-lstopo
set(WAYCA_LSTOPO wayca_lstopo.py)
set(WAYCA_LSTOPO_LINK wayca-lstopo)
set(WAYCA_LSTOPO_SRC ${PROJECT_SOURCE_DIR}/tools/wayca-lstopo/${WAYCA_LSTOPO})
set(WAYCA_LSTOPO_CONF wayca-lstopo/wayca_lstopo.conf)

add_custom_target(${WAYCA_LSTOPO} ALL
	COMMAND ${CMAKE_COMMAND} -E copy ${WAYCA_LSTOPO_SRC} ${WAYCA_LSTOPO}

	COMMAND sed -i "/WAYCA_LSTOPO_VERSION =/s/0.0/\'${WAYCA_SCHEDULER_VERSION}\'/g"
	${WAYCA_LSTOPO}

	COMMAND ${CMAKE_COMMAND} -E create_symlink
	${WAYCA_LSTOPO} ${CMAKE_CURRENT_BINARY_DIR}/${WAYCA_LSTOPO_LINK}
)

install(
	PROGRAMS
	${CMAKE_CURRENT_BINARY_DIR}/${WAYCA_LSTOPO}
	${CMAKE_CURRENT_BINARY_DIR}/${WAYCA_LSTOPO_LINK}
	DESTINATION ${WAYCA_SC_INSTALL_PREFIX}/bin/
)

install(
	FILES ${WAYCA_LSTOPO_CONF}
	DESTINATION ${CMAKE_INSTALL_PREFIX}/etc/wayca-scheduler
)

# wayca-memory-bench
set(WAYCA_MEMORY_BENCH_NAME wayca-memory-bench)
set(WAYCA_MEMORY_BENCH_SRCS wayca-memory-bench/wayca-memory-bench.c)
add_executable(${WAYCA_MEMORY_BENCH_NAME} ${WAYCA_MEMORY_BENCH_SRCS})
target_link_libraries(${WAYCA_MEMORY_BENCH_NAME} pthread ${WAYCA_SC_LIB_NAME})

install(
	TARGETS ${WAYCA_MEMORY_BENCH_NAME}
	RUNTIME DESTINATION ${WAYCA_SC_INSTALL_PREFIX}/bin/
)
