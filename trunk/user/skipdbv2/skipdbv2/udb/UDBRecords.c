/*#io
UDBRecords ioDoc(
			  docCopyright("Steve Dekorte", 2004)
			  docLicense("BSD revised")
			  docObject("UDBRecords")    
			  docDescription("")
			  */

#include "UDB.h"
#include <stdlib.h>

UDBRecords *UDBRecords_new(void)
{
	UDBRecords *self = (UDBRecords *)calloc(1, sizeof(*self));
	if (NULL == self)
		return NULL;

	self->file = JFile_new();
	self->record = UDBRecord_new();
	self->firstEmptyRecord = UDBRecord_new();
	if (NULL == self->file || NULL == self->record ||
		NULL == self->firstEmptyRecord)
		goto failed;

	UDBRecords_setPath_(self, "default");
	if (NULL == self->file->path || NULL == self->file->logPath)
		goto failed;
	return self;

failed:
	/* Use the normal destructor so partial construction has one owner path. */
	UDBRecords_free(self);
	return NULL;
}

void UDBRecords_free(UDBRecords *self)
{
	if (NULL == self)
		return;
	UDBRecords_close(self);
	JFile_free(self->file);    
	UDBRecord_free(self->record);   
	UDBRecord_free(self->firstEmptyRecord);   
	free(self);
}

JFile *UDBRecords_jfile(UDBRecords *self)
{
	return self ? self->file : NULL;
}

void UDBRecords_delete(UDBRecords *self)
{
	if (self)
		JFile_delete(self->file);
}

void UDBRecords_setPath_(UDBRecords *self, const char *path)
{
	if (self && self->file && path)
		JFile_setPath_withExtension_(self->file, path, "udbData");
}

void UDBRecords_setLogPath_(UDBRecords *self, const char *path)
{
	if (self && self->file && path)
		JFile_setLogPath_(self->file, path);
}

char *UDBRecords_path(UDBRecords *self)
{
	return self ? JFile_path(self->file) : NULL;
}
