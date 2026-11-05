#ifndef UDB_COMMIT_MARKER_DEFINED
#define UDB_COMMIT_MARKER_DEFINED 1
#include <stddef.h>
#include <stdio.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct { char *path; FILE *file; } UDBCommitMarker;
int UDBCommitMarker_setPath_(UDBCommitMarker *self, const char *path);
void UDBCommitMarker_close(UDBCommitMarker *self);
void UDBCommitMarker_clear(UDBCommitMarker *self);
int UDBCommitMarker_open(UDBCommitMarker *self, int create);
int UDBCommitMarker_read(UDBCommitMarker *self);
int UDBCommitMarker_write(UDBCommitMarker *self, int flag);
int UDBCommitMarker_verifyJournal(const char *path);
int UDBCommitMarker_syncParentDirectory(const char *path);
int UDBCommitMarker_artifactsExist(const char *const paths[], size_t count);
int UDBCommitMarker_artifactsAreEmpty(const char *const paths[], size_t count);
#ifdef __cplusplus
}
#endif
#endif
