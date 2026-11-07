/*
 * Test-only plugin callback that recursively closes its owning DynLib.
 * The callback reaches the library object through an opaque test state so
 * this fixture does not introduce a link-time dependency on the test binary.
 */
#include <stddef.h>

typedef struct
{
    void *library;
    void (*closeLibrary)(void *);
    int callbackCalls;
} ReentrantFreeState;

#if defined(_WIN32) || defined(__WIN32__) || defined(WIN32)
#define TEST_PLUGIN_EXPORT __declspec(dllexport)
#else
#define TEST_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

TEST_PLUGIN_EXPORT void __skipdb_dynlib_reentrant_free__(void *argument)
{
    ReentrantFreeState *state = (ReentrantFreeState *)argument;
    if (NULL == state)
        return;
    state->callbackCalls++;
    if (state->closeLibrary)
        state->closeLibrary(state->library);
}
