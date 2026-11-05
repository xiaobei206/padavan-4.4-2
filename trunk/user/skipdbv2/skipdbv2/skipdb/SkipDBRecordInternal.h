/* Private SkipDBRecord helpers shared by record implementation units. */
#ifndef SKIPDB_RECORD_INTERNAL_DEFINED
#define SKIPDB_RECORD_INTERNAL_DEFINED 1

#include "SkipDB.h"
#include "SkipDBRecord.h"

#ifdef __cplusplus
extern "C" {
#endif

SkipDBPointer *SkipDBRecord_pointerAtLevel_(SkipDBRecord *self, int level);
void SkipDBRecord_atLevel_setPid_setSuffix_(SkipDBRecord *self, int level, PID_TYPE pid, uint8_t sMatch);
PID_TYPE SkipDBRecord_allocedPidAtLevel_(SkipDBRecord *self, int level);

#ifdef __cplusplus
}
#endif
#endif
