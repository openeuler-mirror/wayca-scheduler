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

cmake_minimum_required(VERSION 3.5)
project(WaycaScheduler)
set(WAYCA_SCHEDULER_VERSION 0.1)

option(DEBUG "Build the debug version of the project" off)

option(WAYCA_SC_BUILD_TOOLS "Build various tools of the wayca scheduler" on)

option(WAYCA_SC_BUILD_TEST "Build the test program of the project" on)

add_compile_options(-Wall -fPIC)
add_definitions(-DWAYCA_SCHEDULER_VERSION=\"${WAYCA_SCHEDULER_VERSION}\")

if(DEBUG)
	add_compile_options(-g)
	add_definitions(-DWAYCA_SC_DEBUG)
endif(DEBUG)

include_directories(include/)

# libwaycascheduler
set(WAYCA_SC_LIB_NAME waycascheduler)
aux_source_directory(./lib WAYCA_SC_LIB_SRC_LISTS)
add_library(${WAYCA_SC_LIB_NAME} SHARED ${WAYCA_SC_LIB_SRC_LISTS})
target_link_libraries(${WAYCA_SC_LIB_NAME} pthread)
set_target_properties(${WAYCA_SC_LIB_NAME} PROPERTIES VERSION 1.0 SOVERSION 1)
set_target_properties(${WAYCA_SC_LIB_NAME} PROPERTIES PUBLIC_HEADER ./include/wayca-scheduler.h)

# The project prefix for all the binaries
set(WAYCA_SC_PREFIX wayca-sc)

# wayca-deployd
set(DAEMON_NAME ${WAYCA_SC_PREFIX}-deployd)
add_executable(${DAEMON_NAME} ${PROJECT_SOURCE_DIR}/deployd.c)
target_link_libraries(${DAEMON_NAME} ${WAYCA_SC_LIB_NAME})

# wayca-deployer
set(DEPLOYER_NAME ${WAYCA_SC_PREFIX}-deployer)
add_executable(${DEPLOYER_NAME} ${PROJECT_SOURCE_DIR}/deployer.c)
target_link_libraries(${DEPLOYER_NAME} ${WAYCA_SC_LIB_NAME})

# wayca-taskdeploy
set(TASKDEPLOY_NAME ${WAYCA_SC_PREFIX}-taskdeploy)
add_executable(${TASKDEPLOY_NAME} ${PROJECT_SOURCE_DIR}/deployer.c)
target_link_libraries(${TASKDEPLOY_NAME} ${WAYCA_SC_LIB_NAME})

# wayca-irqdeploy
set(IRQDEPLOY_NAME ${WAYCA_SC_PREFIX}-irqdeploy)
add_executable(${IRQDEPLOY_NAME} ${PROJECT_SOURCE_DIR}/irqdeploy.c)
target_link_libraries(${IRQDEPLOY_NAME} ${WAYCA_SC_LIB_NAME})

# wayca-irqdump
set(IRQDUMP_NAME ${WAYCA_SC_PREFIX}-irqdump)
add_executable(${IRQDUMP_NAME} ${PROJECT_SOURCE_DIR}/irqdump.c)
target_link_libraries(${IRQDUMP_NAME} ${WAYCA_SC_LIB_NAME})

if(DEFINED CMAKE_INSTALL_PREFIX)
	set(WAYCA_SC_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
else(DEFINED CMAKE_INSTALL_PREFIX)
	set(WAYCA_SC_INSTALL_PREFIX /usr)
endif(DEFINED CMAKE_INSTALL_PREFIX)

install(
	TARGETS	${WAYCA_SC_LIB_NAME}
	LIBRARY DESTINATION ${WAYCA_SC_INSTALL_PREFIX}/lib/
	PUBLIC_HEADER DESTINATION ${WAYCA_SC_INSTALL_PREFIX}/include/
)

add_custom_target(uninstall "${CMAKE_COMMAND}" -P
		  "${PROJECT_SOURCE_DIR}/cmake_uninstall.cmake")

if(WAYCA_SC_BUILD_TOOLS)
	add_subdirectory(tools)
endif(WAYCA_SC_BUILD_TOOLS)

if(WAYCA_SC_BUILD_TEST)
	add_subdirectory(test)
endif(WAYCA_SC_BUILD_TEST)
