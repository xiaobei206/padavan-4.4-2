/* Private cross-module contracts for UDBIndex. */
#ifndef UDB_INDEX_INTERNAL_DEFINED
#define UDB_INDEX_INTERNAL_DEFINED 1
#include "UDBIndex.h"
#ifdef __cplusplus
extern "C" {
#endif
int UDBIndex_isValid_(UDBIndex *self);
int UDBIndex_hasFile_(UDBIndex *self);
int UDBIndex_hasStreams_(UDBIndex *self);
int UDBIndex_hasReservation_(UDBIndex *self, PID_TYPE pid);
int UDBIndex_reservePid_(UDBIndex *self, PID_TYPE pid);
void UDBIndex_unreservePid_(UDBIndex *self, PID_TYPE pid);
void UDBIndex_removeHole_(UDBIndex *self, PID_TYPE pid);
int UDBIndex_releasePid_(UDBIndex *self, PID_TYPE pid);
void UDBIndex_forgetPid_(UDBIndex *self, PID_TYPE pid);
#ifdef __cplusplus
}
#endif
#endif
