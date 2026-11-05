/* Private persistence contracts shared by JFile implementation units. */
#ifndef JFILE_INTERNAL_DEFINED
#define JFILE_INTERNAL_DEFINED 1
#include "JFile.h"
#define JFILE_END_TRANSACTION_ITEM 1
#define JFILE_END_TRANSACTION 2
#ifdef __cplusplus
extern "C" {
#endif
FILE *JFile_openManagedFile_(const char *path, int *created);
void JFile_writeLogToFile(JFile *self);
void JFile_syncFileWritesToDisk(JFile *self);
void JFile_syncLogWritesToDisk(JFile *self);
#ifdef __cplusplus
}
#endif
#endif
