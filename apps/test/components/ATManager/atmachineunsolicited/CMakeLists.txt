#*******************************************************************************
# Copyright (C) 2014, Sierra Wireless Inc., all rights reserved.
#*******************************************************************************

find_package(Legato REQUIRED)
find_package(CUnit REQUIRED)

message("CUNIT_INCLUDE_DIRS: ${CUNIT_INCLUDE_DIRS}")

set(APP_NAME        "atmachineunsolicited")
set(APP_TARGET      "test${APP_NAME}")
set(APP_SOURCES     "test_${APP_NAME}.c")

mkexe(${APP_TARGET}
            .
            ${CUNIT_LIBRARIES}
            -i ${CUNIT_INSTALL}/include
            -i ${CUNIT_INSTALL}/include/CUnit
            -i ${LEGATO_ROOT}/components/atManager/inc
            -i ${LEGATO_ROOT}/components/atManager/devices/adapter_layer/inc
            -i ${LEGATO_ROOT}/components/atManager/devices/uart/inc
            -i ${LEGATO_ROOT}/components/atManager/src
            -i ${LEGATO_ROOT}/components
        DEPENDS
            legato
            Component.cdef
            ${APP_SOURCES}
         )

add_test(${APP_TARGET} ${EXECUTABLE_OUTPUT_PATH}/${APP_TARGET})
