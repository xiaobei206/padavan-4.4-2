/* Private cross-translation-unit contracts for the UDB implementation. */
#ifndef UDB_INTERNAL_DEFINED
#define UDB_INTERNAL_DEFINED 1
#include "UDB.h"
#ifdef __cplusplus
extern "C" {
#endif
int UDB_streamsOpen_(UDB *self);
int UDB_isInTransaction(UDB *self);
UDBRecord *UDB_recordAtPid_(UDB *self, PID_TYPE pid);
#ifdef __cplusplus
}
#endif
#endif
