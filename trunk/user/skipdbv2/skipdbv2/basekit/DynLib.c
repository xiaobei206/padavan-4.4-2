/*
 * Cross-platform dynamic library lifecycle and symbol lookup.
 */
#include "Base.h"
#include "DynLib.h"
#include <string.h>

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(_MSC_VER)
#include <windows.h>
#define RTLD_NOW 0
#define RTLD_GLOBAL 0

static void *dlopen(const char *path, int mode)
{
    HMODULE module;
    (void)mode;
    module = LoadLibraryExA(path, NULL, 0);
    if (module)
        SetLastError(ERROR_SUCCESS);
    return (void *)module;
}

static int dlclose(void *handle)
{
    if (NULL == handle) {
        SetLastError(ERROR_INVALID_HANDLE);
        return -1;
    }
    return FreeLibrary((HMODULE)handle) ? 0 : -1;
}

static const char *dlerror(void)
{
    static char message[256];
    DWORD error = GetLastError();

    if (ERROR_SUCCESS == error)
        return NULL;
    if (0 == FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL, error,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            message, (DWORD)sizeof(message), NULL)) {
        SetLastError(ERROR_SUCCESS);
        return "Windows dynamic loader operation failed";
    }
    /* Match POSIX dlerror(): retrieving an error consumes the loader state. */
    SetLastError(ERROR_SUCCESS);
    return message;
}

static void *dlsym(void *handle, const char *symbol)
{
    if (NULL == handle || NULL == symbol) {
        SetLastError(ERROR_INVALID_PARAMETER);
        return NULL;
    }
    SetLastError(ERROR_SUCCESS);
    return (void *)GetProcAddress((HMODULE)handle, symbol);
}
#else
#include <dlfcn.h>
#endif

/* Copy before publishing so failed allocation and aliased input are safe. */
static int DynLib_replaceString_(char **target, const char *value)
{
    size_t length;
    char *copy;
    char *oldValue;

    if (NULL == target || NULL == value)
        return 0;
    length = strlen(value);
    if ((size_t)-1 == length)
        return 0;

    copy = (char *)io_malloc(length + 1);
    if (NULL == copy)
        return 0;
    memcpy(copy, value, length + 1);

    oldValue = *target;
    *target = copy;
    io_free(oldValue);
    return 1;
}

static void DynLib_clearLoaderError_(void)
{
#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(_MSC_VER)
    SetLastError(ERROR_SUCCESS);
#else
    (void)dlerror();
#endif
}

/* Loader error strings are transient; retain a private copy in DynLib. */
static int DynLib_captureLoaderError_(DynLib *self)
{
    const char *message;

    if (NULL == self)
        return 0;
    message = dlerror();
    if (message) {
        DynLib_setError_(self, message);
        return 1;
    }
    DynLib_setError_(self, NULL);
    return 0;
}

DynLib *DynLib_new(void)
{
    DynLib *self = (DynLib *)io_calloc(1, sizeof(DynLib));
    if (NULL == self)
        return NULL;
    if (!DynLib_replaceString_(&self->path, "")) {
        io_free(self);
        return NULL;
    }
    return self;
}

void DynLib_free(DynLib *self)
{
    if (NULL == self)
        return;

    /* Destruction does not call the optional plugin callback, as historically. */
    if (self->handle) {
        DynLib_clearLoaderError_();
        (void)dlclose(self->handle);
        self->handle = NULL;
    }
    io_free(self->path);
    io_free(self->initFuncName);
    io_free(self->freeFuncName);
    io_free(self->error);
    io_free(self);
}

void DynLib_setPath_(DynLib *self, const char *path)
{
    if (self)
        (void)DynLib_replaceString_(&self->path, path);
}

char *DynLib_path(DynLib *self)
{
    return self ? self->path : NULL;
}

void DynLib_setInitFuncName_(DynLib *self, const char *name)
{
    if (self)
        (void)DynLib_replaceString_(&self->initFuncName, name);
}

char *DynLib_initFuncName(DynLib *self)
{
    return self ? self->initFuncName : NULL;
}

void DynLib_setInitArg_(DynLib *self, void *arg)
{
    if (self)
        self->initArg = arg;
}

void DynLib_setFreeFuncName_(DynLib *self, const char *name)
{
    if (self)
        (void)DynLib_replaceString_(&self->freeFuncName, name);
}

char *DynLib_freeFuncName(DynLib *self)
{
    return self ? self->freeFuncName : NULL;
}

void DynLib_setFreeArg_(DynLib *self, void *arg)
{
    if (self)
        self->freeArg = arg;
}

void DynLib_setError_(DynLib *self, const char *error)
{
    if (NULL == self)
        return;
    if (NULL == error) {
        io_free(self->error);
        self->error = NULL;
        return;
    }
    (void)DynLib_replaceString_(&self->error, error);
}

char *DynLib_error(DynLib *self)
{
    return self ? self->error : NULL;
}

void DynLib_updateError(DynLib *self)
{
    (void)DynLib_captureLoaderError_(self);
}

unsigned char DynLib_hasError(DynLib *self)
{
    return (self && self->error) ? 1 : 0;
}

void DynLib_open(DynLib *self)
{
    void *function;

    if (NULL == self || self->handle)
        return;
    DynLib_setError_(self, NULL);
    if (NULL == self->path || 0 == self->path[0]) {
        DynLib_setError_(self, "library path is empty");
        return;
    }

    DynLib_clearLoaderError_();
    self->handle = dlopen(self->path, RTLD_NOW | RTLD_GLOBAL);
    if (NULL == self->handle) {
        if (!DynLib_captureLoaderError_(self))
            DynLib_setError_(self, "library could not be opened");
        return;
    }
    (void)DynLib_captureLoaderError_(self);

    if (self->initFuncName && self->initFuncName[0]) {
        function = DynLib_pointerForSymbolName_(self, self->initFuncName);
        if (NULL == function) {
            void *handle = self->handle;
            DynLib_clearLoaderError_();
            if (0 == dlclose(handle))
                self->handle = NULL;
            else if (!DynLib_captureLoaderError_(self))
                DynLib_setError_(self, "library could not be closed after init lookup failure");
            return;
        }
        if (self->initArg)
            (*(DynLibOneArgFunction *)function)(self->initArg);
        else
            (*(DynLibNoArgFunction *)function)();
    }
}

unsigned char DynLib_isOpen(DynLib *self)
{
    return (self && self->handle) ? 1 : 0;
}

void DynLib_close(DynLib *self)
{
    void *function;
    void *handle;
    int callbackFailed = 0;

    if (NULL == self || NULL == self->handle)
        return;
    if (self->freeFuncName && self->freeFuncName[0]) {
        function = DynLib_pointerForSymbolName_(self, self->freeFuncName);
        if (function) {
            if (self->freeArg)
                (*(DynLibOneArgFunction *)function)(self->freeArg);
            else
                (*(DynLibNoArgFunction *)function)();
        } else {
            callbackFailed = 1;
        }
    }

    handle = self->handle;
    DynLib_clearLoaderError_();
    if (0 == dlclose(handle)) {
        self->handle = NULL;
        if (!callbackFailed)
            DynLib_setError_(self, NULL);
    } else if (!DynLib_captureLoaderError_(self)) {
        DynLib_setError_(self, "library could not be closed");
    }
}

void *DynLib_pointerForSymbolName_(DynLib *self, const char *symbolName)
{
    void *symbol;

    if (NULL == self)
        return NULL;
    if (NULL == symbolName || 0 == symbolName[0]) {
        DynLib_setError_(self, "symbol name is empty");
        return NULL;
    }
    if (NULL == self->handle) {
        DynLib_setError_(self, "library is not open");
        return NULL;
    }

    DynLib_setError_(self, NULL);
    DynLib_clearLoaderError_();
    symbol = dlsym(self->handle, symbolName);
    if (DynLib_captureLoaderError_(self))
        return NULL;
    if (NULL == symbol)
        DynLib_setError_(self, "symbol was not found");
    return symbol;
}
