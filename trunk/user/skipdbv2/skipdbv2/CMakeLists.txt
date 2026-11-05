cmake_minimum_required(VERSION 3.5)
PROJECT(skipdbv2)
ADD_DEFINITIONS(-Os -g -Wstrict-prototypes -Wall  -falign-loops=16 -DBUILDING_SKIPDB_DLL)

SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)
SET(LIBRARY_OUTPUT_PATH ${PROJECT_BINARY_DIR}/lib)
set(LIBEV_LIBRARIES CACHE PATH "Path to the libev library")
set(LIBEV_INCLUDE_DIRS ${PROJECT_SOURCE_DIR}/libev)

#INCLUDE_DIRECTORIES(${PROJECT_SOURCE_DIR}/src)

if ("${LIBEV_LIBRARIES}" STREQUAL "")
    set(LIBEV_LIBRARIES ev)
endif()

INCLUDE_DIRECTORIES(${LIBEV_INCLUDE_DIRS} ${PROJECT_SOURCE_DIR}/basekit/simd_cph/include ${PROJECT_SOURCE_DIR}/basekit ${PROJECT_SOURCE_DIR}/jfile ${PROJECT_SOURCE_DIR}/udb ${PROJECT_SOURCE_DIR}/skipdb)
FILE(GLOB_RECURSE BASEKIT_SRC ${PROJECT_SOURCE_DIR}/basekit/*.c)
FILE(GLOB_RECURSE FILE_SRC ${PROJECT_SOURCE_DIR}/jfile/*.c)
SET(UDB_SRC
    ${PROJECT_SOURCE_DIR}/udb/UDB.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRead.c
    ${PROJECT_SOURCE_DIR}/udb/UDBWrite.c
    ${PROJECT_SOURCE_DIR}/udb/UDBStorage.c
    ${PROJECT_SOURCE_DIR}/udb/UDBCompact.c
    ${PROJECT_SOURCE_DIR}/udb/UDBCommitMarker.c
    ${PROJECT_SOURCE_DIR}/udb/UDBIndex.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecord.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecords.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordsStorage.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordsCursor.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordsFreeList.c
)
FILE(GLOB_RECURSE SKIPDB_SRC ${PROJECT_SOURCE_DIR}/skipdb/*.c)
FILE(GLOB_RECURSE MGR_SRC ${PROJECT_SOURCE_DIR}/mgr/*.c)
FILE(GLOB_RECURSE CLIENT_SRC ${PROJECT_SOURCE_DIR}/client/*.c)

#set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/brcm-arm/usr/lib)

ADD_LIBRARY(skipdb STATIC ${BASEKIT_SRC} ${FILE_SRC} ${UDB_SRC} ${SKIPDB_SRC})

#INCLUDE_DIRECTORIES(${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(test ./tests/test.c)
#TARGET_LINK_LIBRARIES(test skipdb m ${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(test2 ./tests/test2.c)
#TARGET_LINK_LIBRARIES(test2 skipdb m ${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(jftest ./tests/jftest.c)
#TARGET_LINK_LIBRARIES(jftest skipdb m ${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(udbtest ./tests/udbtest.c)
#TARGET_LINK_LIBRARIES(udbtest skipdb m ${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(example ./tests/example.c)
#TARGET_LINK_LIBRARIES(example skipdb m ${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(skiplist ./tests/skiplist)

ADD_EXECUTABLE(skipd ${PROJECT_SOURCE_DIR}/mgr/skipd.c ${PROJECT_SOURCE_DIR}/mgr/daemonize.c)
TARGET_LINK_LIBRARIES(skipd skipdb m ${LIBEV_LIBRARIES})

ADD_EXECUTABLE(dbus ${PROJECT_SOURCE_DIR}/mgr/dbus.c ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c)
TARGET_LINK_LIBRARIES(dbus ${LIBEV_LIBRARIES})

ADD_EXECUTABLE(dbapi ${PROJECT_SOURCE_DIR}/mgr/dbapi.c ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c)
TARGET_LINK_LIBRARIES(dbapi ${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(env ${PROJECT_SOURCE_DIR}/tests/env.c)
#TARGET_LINK_LIBRARIES(env ${LIBEV_LIBRARIES})

#ADD_EXECUTABLE(server ${PROJECT_SOURCE_DIR}/mgr/array-heap.c ${PROJECT_SOURCE_DIR}/mgr/unix-echo-server.c)
#ADD_EXECUTABLE(client ${PROJECT_SOURCE_DIR}/mgr/array-heap.c ${PROJECT_SOURCE_DIR}/mgr/unix-echo-client.c)
#TARGET_LINK_LIBRARIES(server skipdb m ${LIBEV_LIBRARIES})
#TARGET_LINK_LIBRARIES(client skipdb m ${LIBEV_LIBRARIES})

ADD_EXECUTABLE(skbus ${CLIENT_SRC})
TARGET_LINK_LIBRARIES(skbus m ${LIBEV_LIBRARIES})

option(SKIPDB_BUILD_TESTS "Build SkipDB regression tests" OFF)
if(SKIPDB_BUILD_TESTS)
    enable_testing()
    ADD_EXECUTABLE(udb_reserved_pid_test
        ${PROJECT_SOURCE_DIR}/tests/udb_reserved_pid.c)
    TARGET_LINK_LIBRARIES(udb_reserved_pid_test skipdb m)
    add_test(NAME udb_reserved_pid_test COMMAND udb_reserved_pid_test)

    ADD_EXECUTABLE(udb_record_api_test
        ${PROJECT_SOURCE_DIR}/tests/udb_record_api.c)
    TARGET_LINK_LIBRARIES(udb_record_api_test skipdb m)
    add_test(NAME udb_record_api_test COMMAND udb_record_api_test)

    ADD_EXECUTABLE(dbus_protocol_test
        ${PROJECT_SOURCE_DIR}/tests/dbus_protocol.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c)
    TARGET_LINK_LIBRARIES(dbus_protocol_test skipdb m ${LIBEV_LIBRARIES})
    add_test(NAME dbus_protocol_test COMMAND dbus_protocol_test)

    ADD_EXECUTABLE(dbapi_protocol_test
        ${PROJECT_SOURCE_DIR}/tests/dbapi_protocol.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c)
    TARGET_LINK_LIBRARIES(dbapi_protocol_test skipdb m ${LIBEV_LIBRARIES})
    add_test(NAME dbapi_protocol_test COMMAND dbapi_protocol_test)

    ADD_EXECUTABLE(skipdb_cursor_previous_test
        ${PROJECT_SOURCE_DIR}/tests/skipdb_cursor_previous.c)
    TARGET_LINK_LIBRARIES(skipdb_cursor_previous_test skipdb m)
    add_test(NAME skipdb_cursor_previous_test COMMAND skipdb_cursor_previous_test)
    ADD_EXECUTABLE(ipc_io_test
        ${PROJECT_SOURCE_DIR}/tests/ipc_io.c)
    add_test(NAME ipc_io_test COMMAND ipc_io_test)

    ADD_EXECUTABLE(ipc_transport_test
        ${PROJECT_SOURCE_DIR}/tests/ipc_transport.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c)
    add_test(NAME ipc_transport_test COMMAND ipc_transport_test)

    ADD_EXECUTABLE(commit_marker_test
        ${PROJECT_SOURCE_DIR}/tests/commit_marker.c)
    TARGET_LINK_LIBRARIES(commit_marker_test skipdb m)
    add_test(NAME commit_marker_test COMMAND commit_marker_test)

    ADD_EXECUTABLE(jfile_journal_test
        ${PROJECT_SOURCE_DIR}/tests/jfile_journal.c)
    TARGET_LINK_LIBRARIES(jfile_journal_test skipdb m)
    add_test(NAME jfile_journal_test COMMAND jfile_journal_test)

endif()
