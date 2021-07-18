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

include_directories(${PROJECT_SOURCE_DIR}/lib)

# The project prefix for all the binaries
set(WAYCA_SC_TEST_PREFIX wayca_sc_test)

# wayca_sc_test_group
set(WAYCA_SC_TEST_GROUP_NAME ${WAYCA_SC_TEST_PREFIX}_group)
add_executable(${WAYCA_SC_TEST_GROUP_NAME} wayca_sc_group.c)
target_link_libraries(${WAYCA_SC_TEST_GROUP_NAME} ${WAYCA_SC_LIB_NAME})

# wayca_sc_test_thread
set(WAYCA_SC_TEST_THREAD_NAME ${WAYCA_SC_TEST_PREFIX}_thread)
add_executable(${WAYCA_SC_TEST_THREAD_NAME} wayca_thread.c)
target_link_libraries(${WAYCA_SC_TEST_THREAD_NAME} ${WAYCA_SC_LIB_NAME})

# wayca_sc_test_threadpool
set(WAYCA_SC_TEST_THREADPOOL_NAME ${WAYCA_SC_TEST_PREFIX}_threadpool)
add_executable(${WAYCA_SC_TEST_THREADPOOL_NAME} wayca_threadpool.c)
target_link_libraries(${WAYCA_SC_TEST_THREADPOOL_NAME} ${WAYCA_SC_LIB_NAME})

# wayca_sc_test_topo
set(WAYCA_SC_TEST_TOPO_NAME ${WAYCA_SC_TEST_PREFIX}_topo)
add_executable(${WAYCA_SC_TEST_TOPO_NAME} wayca_topo.c)
target_link_libraries(${WAYCA_SC_TEST_TOPO_NAME} ${WAYCA_SC_LIB_NAME})

# wayca_sc_test_bitmap
set(WAYCA_SC_TEST_BITMAP_NAME ${WAYCA_SC_TEST_PREFIX}_bitmap)
add_executable(${WAYCA_SC_TEST_BITMAP_NAME} wayca_bitmap.c)
target_link_libraries(${WAYCA_SC_TEST_BITMAP_NAME} ${WAYCA_SC_LIB_NAME})
