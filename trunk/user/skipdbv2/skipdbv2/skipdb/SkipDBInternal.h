/* Private cross-translation-unit contracts for the SkipDB implementation. */
#ifndef SKIPDB_INTERNAL_DEFINED
#define SKIPDB_INTERNAL_DEFINED 1
#include "SkipDB.h"
#ifdef __cplusplus
extern "C" {
#endif
void SkipDB_dealloc(SkipDB *self);
void SkipDB_readRootRecord(SkipDB *self);
void SkipDB_freeExcessCachedRecords(SkipDB *self);
int SkipDB_level(SkipDB *self);
void SkipDB_level_(SkipDB *self, int level);
int SkipDB_pickRandomRecordLevel(SkipDB *self);
#ifdef __cplusplus
}
#endif
#endif
