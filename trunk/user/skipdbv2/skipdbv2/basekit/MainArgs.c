#include "Base.h"
#include <stdlib.h>
#include <string.h>
#include "MainArgs.h"

static void MainArgs_freeArguments_(const char **argv, int argc)
{
    int i;
    if (NULL == argv)
        return;
    for (i = 0; i < argc; ++i)
        io_free((void *)argv[i]);
    io_free((void *)argv);
}

MainArgs *MainArgs_new(void)
{
    return (MainArgs *)io_calloc(1, sizeof(MainArgs));
}

void MainArgs_free(MainArgs *self)
{
    if (NULL == self)
        return;
    MainArgs_freeArguments_(self->argv, self->argc);
    io_free(self);
}

int MainArgs_argCount_(const MainArgs *self)
{
	if (NULL == self || self->argc < 0 ||
		(self->argc > 0 && NULL == self->argv))
		return 0;
	return self->argc;
}

const char *MainArgs_argumentAt_(const MainArgs *self, size_t index)
{
	if (NULL == self || self->argc <= 0 || NULL == self->argv ||
		index >= (size_t)self->argc)
		return NULL;
	return self->argv[index];
}

void MainArgs_argc_argv_(MainArgs *self, int argc, const char **argv)
{
    const char **newArgv = NULL;
    const char **oldArgv;
    int oldArgc;
    int i;

    if (NULL == self || argc < 0 || (argc > 0 && NULL == argv) ||
        (size_t)argc >= (size_t)-1 / sizeof(*newArgv))
        return;

    newArgv = (const char **)io_calloc((size_t)argc + 1, sizeof(*newArgv));
    if (NULL == newArgv)
        return;

    if (argc > 0) {

        for (i = 0; i < argc; ++i) {
            const char *argument = argv[i];
            size_t length;
            char *copy;
            if (NULL == argument) {
                MainArgs_freeArguments_(newArgv, argc);
                return;
            }
            length = strlen(argument);
            if (length == (size_t)-1) {
                MainArgs_freeArguments_(newArgv, argc);
                return;
            }
            copy = (char *)io_malloc(length + 1);
            if (NULL == copy) {
                MainArgs_freeArguments_(newArgv, argc);
                return;
            }
            memcpy(copy, argument, length + 1);
            newArgv[i] = copy;
        }
    }

    /* Publish only a complete replacement; failed copies leave self unchanged. */
    oldArgv = self->argv;
    oldArgc = self->argc;
    self->argv = newArgv;
    self->argc = argc;
    MainArgs_freeArguments_(oldArgv, oldArgc);
}
