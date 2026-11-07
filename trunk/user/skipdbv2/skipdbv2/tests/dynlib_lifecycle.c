#include "DynLib.h"

#include <stdio.h>
#include <string.h>

typedef struct
{
    void *library;
    void (*closeLibrary)(void *);
    int callbackCalls;
    int callbackReturned;
} ReentrantFreeState;

static void close_library_from_plugin(void *library)
{
    DynLib_close((DynLib *)library);
}

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(_MSC_VER)
#define TEST_LIBRARY "kernel32.dll"
#define TEST_SYMBOL "GetCurrentProcess"
#else
#define TEST_LIBRARY "libc.so.6"
#define TEST_SYMBOL "malloc"
#endif

#define CHECK(condition, message) do { \
    if (!(condition)) { \
        fprintf(stderr, "DynLib lifecycle test: %s\n", message); \
        return 1; \
    } \
} while (0)

int main(int argc, char **argv)
{
    DynLib *library;
    void *originalHandle;

    DynLib_free(NULL);
    DynLib_open(NULL);
    DynLib_close(NULL);
    DynLib_updateError(NULL);
    DynLib_setPath_(NULL, "ignored");
    DynLib_setInitFuncName_(NULL, "ignored");
    DynLib_setInitArg_(NULL, NULL);
    DynLib_setFreeFuncName_(NULL, "ignored");
    DynLib_setFreeArg_(NULL, NULL);
    DynLib_setError_(NULL, "ignored");

    CHECK(NULL == DynLib_path(NULL), "NULL path getter was not safe");
    CHECK(NULL == DynLib_initFuncName(NULL), "NULL init-name getter was not safe");
    CHECK(NULL == DynLib_freeFuncName(NULL), "NULL free-name getter was not safe");
    CHECK(NULL == DynLib_error(NULL), "NULL error getter was not safe");
    CHECK(0 == DynLib_isOpen(NULL), "NULL open-state query was not safe");
    CHECK(0 == DynLib_hasError(NULL), "NULL error-state query was not safe");
    CHECK(NULL == DynLib_pointerForSymbolName_(NULL, TEST_SYMBOL),
          "NULL symbol lookup was not safe");

    library = DynLib_new();
    CHECK(NULL != library, "constructor failed");
    CHECK(DynLib_path(library) && 0 == strcmp(DynLib_path(library), ""),
          "constructor did not initialize an owned empty path");

    DynLib_setPath_(library, NULL);
    CHECK(0 == strcmp(DynLib_path(library), ""),
          "NULL path replacement changed the current path");
    DynLib_setPath_(library, DynLib_path(library));
    CHECK(0 == strcmp(DynLib_path(library), ""),
          "self-aliased path replacement corrupted the path");

    DynLib_setError_(library, "previous error");
    DynLib_setError_(library, DynLib_error(library));
    CHECK(DynLib_error(library) &&
          0 == strcmp(DynLib_error(library), "previous error"),
          "self-aliased error replacement corrupted the error");
    DynLib_setError_(library, NULL);
    CHECK(0 == DynLib_hasError(library), "NULL error did not clear the error");

    DynLib_open(library);
    CHECK(!DynLib_isOpen(library) && DynLib_hasError(library),
          "empty path was not rejected");

    DynLib_setPath_(library, "missing-dynlib-test-library");
    DynLib_open(library);
    CHECK(!DynLib_isOpen(library) && DynLib_hasError(library),
          "failed load did not report an error");

    DynLib_setPath_(library, TEST_LIBRARY);
    DynLib_setInitFuncName_(library, "__skipdb_dynlib_missing_init__");
    DynLib_open(library);
    CHECK(!DynLib_isOpen(library) && DynLib_hasError(library),
          "missing init symbol left a partially opened library");

    DynLib_setInitFuncName_(library, "");
    DynLib_open(library);
    CHECK(DynLib_isOpen(library), "valid system library load failed");
    CHECK(!DynLib_hasError(library), "successful load retained a stale error");

    originalHandle = library->handle;
    DynLib_open(library);
    CHECK(library->handle == originalHandle,
          "repeated open replaced an already-owned loader handle");

    CHECK(NULL != DynLib_pointerForSymbolName_(library, TEST_SYMBOL),
          "valid symbol lookup failed");
    CHECK(!DynLib_hasError(library), "successful symbol lookup retained a stale error");
    CHECK(NULL == DynLib_pointerForSymbolName_(library, "__skipdb_dynlib_missing_symbol__"),
          "missing symbol unexpectedly resolved");
    CHECK(DynLib_hasError(library), "missing symbol did not report an error");
    DynLib_updateError(library);
    CHECK(!DynLib_hasError(library),
          "loader error state was not consumed after update");
    CHECK(NULL != DynLib_pointerForSymbolName_(library, TEST_SYMBOL),
          "symbol lookup did not recover after a previous failure");

    DynLib_setFreeFuncName_(library, "__skipdb_dynlib_missing_free__");
    DynLib_close(library);
    CHECK(!DynLib_isOpen(library),
          "missing free callback prevented the OS handle from closing");
    CHECK(DynLib_hasError(library),
          "missing free callback error was lost during successful close");
    DynLib_close(library);
    CHECK(!DynLib_isOpen(library), "repeated close changed the closed state");
    DynLib_free(library);

    library = DynLib_new();
    CHECK(NULL != library, "second constructor failed");
    DynLib_setPath_(library, TEST_LIBRARY);
    DynLib_open(library);
    CHECK(DynLib_isOpen(library), "second system library load failed");
    DynLib_free(library);

    CHECK(argc > 1, "reentrant fixture path was not supplied");

    library = DynLib_new();
    CHECK(NULL != library, "reentrant init fixture constructor failed");
    {
        ReentrantFreeState state;
        memset(&state, 0, sizeof(state));
        state.library = library;
        state.closeLibrary = close_library_from_plugin;
        DynLib_setPath_(library, argv[1]);
        DynLib_setInitFuncName_(library, "__skipdb_dynlib_reentrant_init__");
        DynLib_setInitArg_(library, &state);
        DynLib_open(library);
        CHECK(state.callbackCalls == 1 && state.callbackReturned == 1,
              "init callback unloaded its own image before returning");
        CHECK(!DynLib_isOpen(library),
              "init callback close request was not drained before open returned");
    }
    DynLib_free(library);

    library = DynLib_new();
    CHECK(NULL != library, "reentrant fixture constructor failed");
    {
        ReentrantFreeState state;
        memset(&state, 0, sizeof(state));
        state.library = library;
        state.closeLibrary = close_library_from_plugin;
        DynLib_setPath_(library, argv[1]);
        DynLib_setFreeFuncName_(library, "__skipdb_dynlib_reentrant_free__");
        DynLib_setFreeArg_(library, &state);
        DynLib_open(library);
        CHECK(DynLib_isOpen(library), "reentrant fixture load failed");
        DynLib_close(library);
        CHECK(state.callbackCalls == 1 && state.callbackReturned == 1,
              "recursive close invoked the plugin free callback more than once");
        CHECK(!DynLib_isOpen(library),
              "recursive close left the original loader handle open");
    }
    DynLib_free(library);

    puts("DynLib lifecycle test passed");
    return 0;
}
