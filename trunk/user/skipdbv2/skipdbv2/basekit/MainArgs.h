//metadoc MainArgs copyright Steve Dekorte 2002
//metadoc MainArgs license BSD revised
/*metadoc MainArgs description
Structure for copying and storing command line arguments.")
*/

#ifndef MAINARGS_DEFINED
#define MAINARGS_DEFINED 1

#include "Common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
	int argc;
	const char **argv;
} MainArgs;

BASEKIT_API MainArgs *MainArgs_new(void);
BASEKIT_API void MainArgs_free(MainArgs *self);

/* Deep-copies argv and always stores argv[argc] == NULL (also when argc is zero). */
/* Invalid input or allocation failure leaves self unchanged. */
BASEKIT_API void MainArgs_argc_argv_(MainArgs *self, int argc, const char **argv);

/* Checked accessors return 0/NULL for invalid or inconsistent argument state. */
BASEKIT_API int MainArgs_argCount_(const MainArgs *self);
BASEKIT_API const char *MainArgs_argumentAt_(const MainArgs *self, size_t index);
#define MainArgs_argCount(self) MainArgs_argCount_(self)
#define MainArgs_argAt_(self, index) MainArgs_argumentAt_((self), (size_t)(index))

#ifdef __cplusplus
}
#endif
#endif



