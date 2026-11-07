#include "MainArgs.h"

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


static int testCorruptArgcCleanup(int corruptArgc)
{
    const char *owned[] = { "first", "second", "third" };
    MainArgs *args = MainArgs_new();

    if (NULL == args) {
        fprintf(stderr, "MainArgs corrupt-count fixture allocation failed\n");
        return 0;
    }
    MainArgs_argc_argv_(args, 3, owned);
    if (args->argc != 3 || NULL == args->argv || args->argv[3] != NULL) {
        fprintf(stderr, "MainArgs corrupt-count fixture setup failed\n");
        MainArgs_free(args);
        return 0;
    }

    args->argc = corruptArgc;
    MainArgs_free(args);
    if (io_allocatedBytes() != 0 || io_allocs() != io_frees()) {
        fprintf(stderr, "MainArgs teardown trusted corrupt argc and leaked allocations\n");
        return 0;
    }
    return 1;
}

int main(void)
{
    MainArgs *args = MainArgs_new();
    char app[] = "padavan";
    char option[] = "--database";
    char path[] = "skipdb.db";
    char replacement_app[] = "skipdb";
    char replacement_option[] = "--version";
    const char *initial[] = { app, option, path };
    const char *replacement[] = { replacement_app, replacement_option };
    const char *null_argument[] = { NULL };
    MainArgs malformed = { 1, NULL };

    if (NULL == args) {
        fprintf(stderr, "MainArgs allocation failed\n");
        return EXIT_FAILURE;
    }

    MainArgs_argc_argv_(args, 3, initial);
    if (args->argc != 3 || NULL == args->argv ||
        strcmp(args->argv[0], "padavan") != 0 ||
        strcmp(args->argv[1], "--database") != 0 ||
        strcmp(args->argv[2], "skipdb.db") != 0 ||
        args->argv[args->argc] != NULL) {
        fprintf(stderr, "MainArgs did not copy its initial arguments\n");
        MainArgs_free(args);
        return EXIT_FAILURE;
    }

    if (MainArgs_argCount(args) != 3 ||
        NULL == MainArgs_argAt_(args, 0) ||
        strcmp(MainArgs_argAt_(args, 0), "padavan") != 0 ||
        strcmp(MainArgs_argAt_(args, 1), "--database") != 0 ||
        strcmp(MainArgs_argAt_(args, 2), "skipdb.db") != 0 ||
        MainArgs_argAt_(args, 3) != NULL ||
        MainArgs_argAt_(args, -1) != NULL ||
        MainArgs_argCount(NULL) != 0 ||
        MainArgs_argAt_(NULL, 0) != NULL ||
        MainArgs_argCount(&malformed) != 0 ||
        MainArgs_argAt_(&malformed, 0) != NULL) {
        fprintf(stderr, "MainArgs checked accessors accepted invalid receiver or index\n");
        MainArgs_free(args);
        return EXIT_FAILURE;
    }

    app[0] = 'X';
    option[0] = 'X';
    path[0] = 'X';
    if (strcmp(args->argv[0], "padavan") != 0 ||
        strcmp(args->argv[1], "--database") != 0 ||
        strcmp(args->argv[2], "skipdb.db") != 0) {
        fprintf(stderr, "MainArgs retained borrowed argument storage\n");
        MainArgs_free(args);
        return EXIT_FAILURE;
    }

    MainArgs_argc_argv_(args, 2, replacement);
    if (args->argc != 2 || MainArgs_argCount(args) != 2 ||
        strcmp(MainArgs_argAt_(args, 0), "skipdb") != 0 ||
        strcmp(MainArgs_argAt_(args, 1), "--version") != 0 ||
        args->argv[args->argc] != NULL) {
        fprintf(stderr, "MainArgs replacement failed\n");
        MainArgs_free(args);
        return EXIT_FAILURE;
    }

    MainArgs_argc_argv_(args, -1, replacement);
    MainArgs_argc_argv_(args, 1, NULL);
    MainArgs_argc_argv_(args, 1, null_argument);
    if (args->argc != 2 || strcmp(args->argv[0], "skipdb") != 0 ||
        strcmp(args->argv[1], "--version") != 0) {
        fprintf(stderr, "invalid MainArgs input corrupted the previous state\n");
        MainArgs_free(args);
        return EXIT_FAILURE;
    }

    MainArgs_argc_argv_(args, 0, NULL);
    if (args->argc != 0 || NULL == args->argv || args->argv[0] != NULL) {
        fprintf(stderr, "empty MainArgs replacement did not retain a NULL sentinel\n");
        MainArgs_free(args);
        return EXIT_FAILURE;
    }

    MainArgs_free(NULL);
    MainArgs_free(args);
    if (!testCorruptArgcCleanup(-1) ||
        !testCorruptArgcCleanup(INT_MAX))
        return EXIT_FAILURE;
    puts("MainArgs lifecycle test passed");
    return EXIT_SUCCESS;
}
