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
SET(JFILE_SRC
    ${PROJECT_SOURCE_DIR}/jfile/JFile.c
    ${PROJECT_SOURCE_DIR}/jfile/JFileJournal.c
    ${PROJECT_SOURCE_DIR}/jfile/JFileJournalScan.c
    ${PROJECT_SOURCE_DIR}/jfile/JFileSync.c
)
SET(UDB_SRC
    ${PROJECT_SOURCE_DIR}/udb/UDB.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRead.c
    ${PROJECT_SOURCE_DIR}/udb/UDBWrite.c
    ${PROJECT_SOURCE_DIR}/udb/UDBLifecycle.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecovery.c
    ${PROJECT_SOURCE_DIR}/udb/UDBTransaction.c
    ${PROJECT_SOURCE_DIR}/udb/UDBCompact.c
    ${PROJECT_SOURCE_DIR}/udb/UDBCommitMarker.c
    ${PROJECT_SOURCE_DIR}/udb/UDBIndex.c
    ${PROJECT_SOURCE_DIR}/udb/UDBIndexFreeList.c
    ${PROJECT_SOURCE_DIR}/udb/UDBIndexMap.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecord.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordWrite.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordRead.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordMove.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecords.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordsStorage.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordsCursor.c
    ${PROJECT_SOURCE_DIR}/udb/UDBRecordsFreeList.c
)
SET(SKIPDB_SRC
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDB.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBCache.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBTransaction.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBSearch.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBWrite.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBQuery.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBPrefix.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBCursor.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBRecord.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBRecordCache.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBRecordLinks.c
    ${PROJECT_SOURCE_DIR}/skipdb/SkipDBRecordStorage.c
)
FILE(GLOB_RECURSE MGR_SRC ${PROJECT_SOURCE_DIR}/mgr/*.c)
FILE(GLOB_RECURSE CLIENT_SRC ${PROJECT_SOURCE_DIR}/client/*.c)

#set(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} /opt/brcm-arm/usr/lib)

ADD_LIBRARY(skipdb STATIC ${BASEKIT_SRC} ${JFILE_SRC} ${UDB_SRC} ${SKIPDB_SRC})

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

ADD_EXECUTABLE(skipd
    ${PROJECT_SOURCE_DIR}/mgr/skipd.c
    ${PROJECT_SOURCE_DIR}/mgr/skipd_command.c
    ${PROJECT_SOURCE_DIR}/mgr/skipd_client.c
    ${PROJECT_SOURCE_DIR}/mgr/skipd_database.c
    ${PROJECT_SOURCE_DIR}/mgr/skipd_scheduler.c
    ${PROJECT_SOURCE_DIR}/mgr/skipd_socket.c
    ${PROJECT_SOURCE_DIR}/mgr/skipd_script.c
    ${PROJECT_SOURCE_DIR}/mgr/daemonize.c
)
TARGET_LINK_LIBRARIES(skipd skipdb m ${LIBEV_LIBRARIES})

ADD_EXECUTABLE(dbus ${PROJECT_SOURCE_DIR}/mgr/dbus.c ${PROJECT_SOURCE_DIR}/mgr/dbus_result.c ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c ${PROJECT_SOURCE_DIR}/mgr/ipc_protocol.c ${PROJECT_SOURCE_DIR}/mgr/ipc_request.c)
TARGET_LINK_LIBRARIES(dbus ${LIBEV_LIBRARIES})

ADD_EXECUTABLE(dbapi ${PROJECT_SOURCE_DIR}/mgr/dbapi.c ${PROJECT_SOURCE_DIR}/mgr/dbapi_result.c ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c ${PROJECT_SOURCE_DIR}/mgr/ipc_protocol.c ${PROJECT_SOURCE_DIR}/mgr/ipc_request.c)
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

    ADD_EXECUTABLE(udb_index_layers_test
        ${PROJECT_SOURCE_DIR}/tests/udb_index_layers.c)
    TARGET_LINK_LIBRARIES(udb_index_layers_test skipdb m)
    add_test(NAME udb_index_layers_test COMMAND udb_index_layers_test)

    ADD_EXECUTABLE(udb_record_api_test
        ${PROJECT_SOURCE_DIR}/tests/udb_record_api.c)
    TARGET_LINK_LIBRARIES(udb_record_api_test skipdb m)
    add_test(NAME udb_record_api_test COMMAND udb_record_api_test)

    ADD_EXECUTABLE(udb_record_storage_test
        ${PROJECT_SOURCE_DIR}/tests/udb_record_storage.c)
    TARGET_LINK_LIBRARIES(udb_record_storage_test skipdb m)
    add_test(NAME udb_record_storage_test COMMAND udb_record_storage_test)

    ADD_EXECUTABLE(udb_transaction_lifecycle_test
        ${PROJECT_SOURCE_DIR}/tests/udb_transaction_lifecycle.c)
    TARGET_LINK_LIBRARIES(udb_transaction_lifecycle_test skipdb m)
    add_test(NAME udb_transaction_lifecycle_test COMMAND udb_transaction_lifecycle_test)

    ADD_EXECUTABLE(skipd_script_test
        ${PROJECT_SOURCE_DIR}/tests/skipd_script.c
        ${PROJECT_SOURCE_DIR}/mgr/skipd_script.c)
    TARGET_INCLUDE_DIRECTORIES(skipd_script_test PRIVATE ${PROJECT_SOURCE_DIR}/mgr)
    add_test(NAME skipd_script_test COMMAND skipd_script_test)

    ADD_EXECUTABLE(dbus_protocol_test
        ${PROJECT_SOURCE_DIR}/tests/dbus_protocol.c
        ${PROJECT_SOURCE_DIR}/mgr/dbus_result.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_protocol.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_request.c)
    TARGET_LINK_LIBRARIES(dbus_protocol_test skipdb m ${LIBEV_LIBRARIES})
    add_test(NAME dbus_protocol_test COMMAND dbus_protocol_test)

    ADD_EXECUTABLE(dbapi_protocol_test
        ${PROJECT_SOURCE_DIR}/tests/dbapi_protocol.c
        ${PROJECT_SOURCE_DIR}/mgr/dbapi_result.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_transport.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_protocol.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_request.c)
    TARGET_LINK_LIBRARIES(dbapi_protocol_test skipdb m ${LIBEV_LIBRARIES})
    add_test(NAME dbapi_protocol_test COMMAND dbapi_protocol_test)

    ADD_EXECUTABLE(skipdb_core_layers_test
        ${PROJECT_SOURCE_DIR}/tests/skipdb_core_layers.c)
    TARGET_LINK_LIBRARIES(skipdb_core_layers_test skipdb m)
    add_test(NAME skipdb_core_layers_test COMMAND skipdb_core_layers_test)

    ADD_EXECUTABLE(skipdb_prefix_test
        ${PROJECT_SOURCE_DIR}/tests/skipdb_prefix.c)
    TARGET_LINK_LIBRARIES(skipdb_prefix_test skipdb m)
    add_test(NAME skipdb_prefix_test COMMAND skipdb_prefix_test)

    ADD_EXECUTABLE(skipdb_record_storage_test
        ${PROJECT_SOURCE_DIR}/tests/skipdb_record_storage.c)
    TARGET_LINK_LIBRARIES(skipdb_record_storage_test skipdb m)
    add_test(NAME skipdb_record_storage_test COMMAND skipdb_record_storage_test)

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

    ADD_EXECUTABLE(ipc_protocol_test
        ${PROJECT_SOURCE_DIR}/tests/ipc_protocol.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_protocol.c)
    add_test(NAME ipc_protocol_test COMMAND ipc_protocol_test)

    ADD_EXECUTABLE(ipc_request_test
        ${PROJECT_SOURCE_DIR}/tests/ipc_request.c
        ${PROJECT_SOURCE_DIR}/mgr/ipc_request.c)
    add_test(NAME ipc_request_test COMMAND ipc_request_test)

    ADD_EXECUTABLE(commit_marker_test
        ${PROJECT_SOURCE_DIR}/tests/commit_marker.c)
    TARGET_LINK_LIBRARIES(commit_marker_test skipdb m)
    add_test(NAME commit_marker_test COMMAND commit_marker_test)

    ADD_EXECUTABLE(jfile_journal_test
        ${PROJECT_SOURCE_DIR}/tests/jfile_journal.c)
    TARGET_LINK_LIBRARIES(jfile_journal_test skipdb m)
    add_test(NAME jfile_journal_test COMMAND jfile_journal_test)

    ADD_EXECUTABLE(stack_lifecycle_test
        ${PROJECT_SOURCE_DIR}/tests/stack_lifecycle.c)
    TARGET_LINK_LIBRARIES(stack_lifecycle_test skipdb m)
    add_test(NAME stack_lifecycle_test COMMAND stack_lifecycle_test)

    ADD_EXECUTABLE(list_memory_size_test
        ${PROJECT_SOURCE_DIR}/tests/list_memory_size.c)
    TARGET_LINK_LIBRARIES(list_memory_size_test skipdb m)
    add_test(NAME list_memory_size_test COMMAND list_memory_size_test)

    ADD_EXECUTABLE(uarray_stack_resize_test
        ${PROJECT_SOURCE_DIR}/tests/uarray_stack_resize.c)
    TARGET_LINK_LIBRARIES(uarray_stack_resize_test skipdb m)
    add_test(NAME uarray_stack_resize_test COMMAND uarray_stack_resize_test)

    ADD_EXECUTABLE(uarray_size_in_bytes_test
        ${PROJECT_SOURCE_DIR}/tests/uarray_size_in_bytes.c)
    TARGET_LINK_LIBRARIES(uarray_size_in_bytes_test skipdb m)
    add_test(NAME uarray_size_in_bytes_test COMMAND uarray_size_in_bytes_test)

    ADD_EXECUTABLE(common_alloc_test
        ${PROJECT_SOURCE_DIR}/tests/common_alloc.c
        ${PROJECT_SOURCE_DIR}/basekit/Common.c)
    TARGET_COMPILE_DEFINITIONS(common_alloc_test PRIVATE IO_CHECK_ALLOC)
    add_test(NAME common_alloc_test COMMAND common_alloc_test)

    ADD_EXECUTABLE(random_gen_lifecycle_test
        ${PROJECT_SOURCE_DIR}/tests/random_gen_lifecycle.c)
    TARGET_LINK_LIBRARIES(random_gen_lifecycle_test skipdb m)
    add_test(NAME random_gen_lifecycle_test COMMAND random_gen_lifecycle_test)

    ADD_EXECUTABLE(convert_utf8_bounds_test
        ${PROJECT_SOURCE_DIR}/tests/convert_utf8_bounds.c)
    TARGET_LINK_LIBRARIES(convert_utf8_bounds_test skipdb m)
    add_test(NAME convert_utf8_bounds_test COMMAND convert_utf8_bounds_test)

    ADD_EXECUTABLE(date_time_init_test
        ${PROJECT_SOURCE_DIR}/tests/date_time_init.c)
    TARGET_LINK_LIBRARIES(date_time_init_test skipdb m)
    add_test(NAME date_time_init_test COMMAND date_time_init_test)

    ADD_EXECUTABLE(duration_lifecycle_test
        ${PROJECT_SOURCE_DIR}/tests/duration_lifecycle.c)
    TARGET_LINK_LIBRARIES(duration_lifecycle_test skipdb m)
    add_test(NAME duration_lifecycle_test COMMAND duration_lifecycle_test)

    ADD_EXECUTABLE(portable_strptime_bounds_test
        ${PROJECT_SOURCE_DIR}/tests/portable_strptime_bounds.c)
    TARGET_LINK_LIBRARIES(portable_strptime_bounds_test skipdb m)
    add_test(NAME portable_strptime_bounds_test COMMAND portable_strptime_bounds_test)

    ADD_EXECUTABLE(portable_gettimeofday_test
        ${PROJECT_SOURCE_DIR}/tests/portable_gettimeofday.c)
    TARGET_LINK_LIBRARIES(portable_gettimeofday_test skipdb m)
    add_test(NAME portable_gettimeofday_test COMMAND portable_gettimeofday_test)

    ADD_EXECUTABLE(main_args_lifecycle_test
        ${PROJECT_SOURCE_DIR}/tests/main_args_lifecycle.c)
    TARGET_LINK_LIBRARIES(main_args_lifecycle_test skipdb m)
    add_test(NAME main_args_lifecycle_test COMMAND main_args_lifecycle_test)

    ADD_EXECUTABLE(datum_boundaries_test
        ${PROJECT_SOURCE_DIR}/tests/datum_boundaries.c)
    TARGET_LINK_LIBRARIES(datum_boundaries_test skipdb m)
    add_test(NAME datum_boundaries_test COMMAND datum_boundaries_test)

    ADD_EXECUTABLE(dynlib_lifecycle_test
        ${PROJECT_SOURCE_DIR}/tests/dynlib_lifecycle.c)
    TARGET_LINK_LIBRARIES(dynlib_lifecycle_test skipdb m ${CMAKE_DL_LIBS})
    add_test(NAME dynlib_lifecycle_test COMMAND dynlib_lifecycle_test)

    ADD_EXECUTABLE(shash_compact_test
        ${PROJECT_SOURCE_DIR}/tests/shash_compact.c)
    TARGET_LINK_LIBRARIES(shash_compact_test skipdb m)
    add_test(NAME shash_compact_test COMMAND shash_compact_test)

    ADD_EXECUTABLE(phash_compact_test
        ${PROJECT_SOURCE_DIR}/tests/phash_compact.c)
    TARGET_LINK_LIBRARIES(phash_compact_test skipdb m)
    add_test(NAME phash_compact_test COMMAND phash_compact_test)

endif()
