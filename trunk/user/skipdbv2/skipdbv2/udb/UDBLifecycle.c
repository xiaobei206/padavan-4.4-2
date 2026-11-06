/* UDB allocation, path ownership, and close lifecycle. */
#include "UDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

UDB *UDB_new(void)
{
	UDB *self = (UDB *)calloc(1, sizeof(UDB));

	if (NULL == self)
		return NULL;

	self->index = UDBIndex_new();
	self->records = UDBRecords_new();
	if (NULL == self->index || NULL == self->records) {
		UDBIndex_free(self->index);
		UDBRecords_free(self->records);
		free(self);
		return NULL;
	}

	UDB_setPath_(self, "default");
	if (NULL == self->path || NULL == self->commitMarker.path ||
		NULL == self->index->file->path || NULL == self->index->file->logPath ||
		NULL == self->records->file->path || NULL == self->records->file->logPath) {
		UDBIndex_free(self->index);
		UDBRecords_free(self->records);
		free(self->path);
		UDBCommitMarker_clear(&self->commitMarker);
		free(self);
		return NULL;
	}
	return self;
}

void UDB_free(UDB *self)
{
	if (NULL == self)
		return;
	UDB_close(self);
	free(self->path);    
	free(self->commitMarker.path);    
	
	UDBIndex_free(self->index);   
	UDBRecords_free(self->records);   
	free(self);
}

void UDB_delete(UDB *self)
{
	if (NULL == self)
		return;
	UDB_close(self);
	UDBIndex_delete(self->index);
	UDBRecords_delete(self->records);
	if (self->commitMarker.path)
		remove(self->commitMarker.path);
}

void UDB_setPath_(UDB *self, const char *path)
{
	size_t n;
	char *main_path, *marker_path, *index_path, *index_log, *data_path, *data_log;

	if (!self || !path || self->isOpen || !self->index || !self->records ||
		!self->index->file || !self->records->file)
		return;

	n = strlen(path);
	if (n > (size_t)-1 - sizeof(".committed") ||
		n > (size_t)-1 - sizeof(".udbIndex.log") ||
		n > (size_t)-1 - sizeof(".udbData.log"))
		return;

	main_path = (char *)malloc(n + 1);
	marker_path = (char *)malloc(n + sizeof(".committed"));
	index_path = (char *)malloc(n + sizeof(".udbIndex"));
	index_log = (char *)malloc(n + sizeof(".udbIndex.log"));
	data_path = (char *)malloc(n + sizeof(".udbData"));
	data_log = (char *)malloc(n + sizeof(".udbData.log"));
	if (!main_path || !marker_path || !index_path || !index_log ||
		!data_path || !data_log) {
		free(main_path);
		free(marker_path);
		free(index_path);
		free(index_log);
		free(data_path);
		free(data_log);
		return;
	}

	memcpy(main_path, path, n + 1);
	memcpy(marker_path, path, n);
	memcpy(marker_path + n, ".committed", sizeof(".committed"));
	memcpy(index_path, path, n);
	memcpy(index_path + n, ".udbIndex", sizeof(".udbIndex"));
	memcpy(index_log, path, n);
	memcpy(index_log + n, ".udbIndex.log", sizeof(".udbIndex.log"));
	memcpy(data_path, path, n);
	memcpy(data_path + n, ".udbData", sizeof(".udbData"));
	memcpy(data_log, path, n);
	memcpy(data_log + n, ".udbData.log", sizeof(".udbData.log"));

	if (UDBCommitMarker_setPath_(&self->commitMarker, marker_path) != 0) {
		free(main_path);
		free(marker_path);
		free(index_path);
		free(index_log);
		free(data_path);
		free(data_log);
		return;
	}
	free(marker_path);

	free(self->path);
	free(self->index->file->path);
	free(self->index->file->logPath);
	free(self->records->file->path);
	free(self->records->file->logPath);
	self->path = main_path;
	self->index->file->path = index_path;
	self->index->file->logPath = index_log;
	self->records->file->path = data_path;
	self->records->file->logPath = data_log;
}
/*
void UDB_setSecondaryPath_(UDB *self, const char *secondaryPath)
{
	UDBIndex_setPath_(self->index, self->path);
	UDBIndex_setLogPath_(self->index, secondaryPath);
	
	UDBRecords_setPath_(self->records, secondaryPath);
	UDBRecords_setLogPath_(self->records, self->path);
}
*/


void UDB_setLogPath_(UDB *self, const char *path)
{
	JFile index_stage = {0};
	JFile records_stage = {0};

	if (NULL == self || NULL == path || self->isOpen ||
		NULL == self->index || NULL == self->records ||
		NULL == self->index->file || NULL == self->records->file)
		return;

	/*
	 * Derive both names before replacing either live path. If either
	 * allocation or basename validation fails, preserve the old pair.
	 */
	index_stage.path = self->index->file->path;
	records_stage.path = self->records->file->path;
	JFile_setLogPath_(&index_stage, path);
	JFile_setLogPath_(&records_stage, path);
	if (NULL == index_stage.logPath || NULL == records_stage.logPath) {
		free(index_stage.logPath);
		free(records_stage.logPath);
		return;
	}

	free(self->index->file->logPath);
	free(self->records->file->logPath);
	self->index->file->logPath = index_stage.logPath;
	self->records->file->logPath = records_stage.logPath;
}

char *UDB_path(UDB *self)
{
	return self ? self->path : NULL;
}

int UDB_isOpen(UDB *self)
{
	return self ? self->isOpen : 0;
}

void UDB_close(UDB *self)
{
	if (NULL == self)
		return;
	UDBIndex_close(self->index);
	UDBRecords_close(self->records);
	if (self->index && self->index->file)
		self->index->file->deferLogReplay = 0;
	if (self->records && self->records->file)
		self->records->file->deferLogReplay = 0;
	UDBCommitMarker_close(&self->commitMarker);
	self->isOpen = 0;
	self->withinTransaction = 0;
}
