/*#io
UDB ioDoc(
		docCopyright("Steve Dekorte", 2004)
		docLicense("BSD revised")
		docObject("UDB")    
		docDescription("")
		*/

#include "UDB.h"
#include <stdio.h>

PID_TYPE UDB_nextPid(UDB *self)
{
	return self && self->isOpen && self->index
		? UDBIndex_nextPid(self->index) : 0;
}

PID_TYPE UDB_allocPid(UDB *self)
{
	return self && self->isOpen && self->withinTransaction && self->index
		? UDBIndex_allocPid(self->index) : 0;
}

void UDB_show(UDB *self)
{
	UDBRecord *record = UDBRecords_firstRecord(self->records);
	printf("UDB Records:\n");
	
	while (record)
	{	
		UDBRecord_show(record);
		record = UDBRecords_nextRecord(self->records);
	}
}

void UDB_showIndex(UDB *self)
{
	UDBIndex_show(self->index);
}
