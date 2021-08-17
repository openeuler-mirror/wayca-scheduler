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

set(MANIFEST "${CMAKE_CURRENT_BINARY_DIR}/install_manifest.txt")

if(NOT EXISTS ${MANIFEST})
    message(FATAL_ERROR "Cannot find install mainfest: ${MANIFEST}")
endif()

file(STRINGS ${MANIFEST} files)
foreach(file ${files})
    if(EXISTS ${file} OR IS_SYMLINK ${file})
        message(STATUS "Removing: ${file}")

	execute_process(COMMAND rm -f ${file}
            RESULT_VARIABLE result
            OUTPUT_QUIET
            ERROR_VARIABLE stderr
            ERROR_STRIP_TRAILING_WHITESPACE
        )

        if(NOT ${result} EQUAL 0)
            message(FATAL_ERROR "${stderr}")
        endif()
    else()
        message(STATUS "Does-not-exist: ${file}")
    endif()
endforeach(file)
