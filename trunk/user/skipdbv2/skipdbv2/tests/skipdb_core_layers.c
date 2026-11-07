#include "SkipDB.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int value_is(SkipDB *db,const char *key,const char *expected)
{
 Datum value=SkipDB_at_(db,Datum_FromCString_(key)); size_t size=strlen(expected)+1;
 return value.data!=NULL&&value.size==size&&memcmp(value.data,expected,size)==0;
}
static int key_is(SkipDBRecord *record,const char *expected)
{
 Datum key;size_t size;if(!record)return 0;key=SkipDBRecord_keyDatum(record);size=strlen(expected)+1;
 return key.data!=NULL&&key.size==size&&memcmp(key.data,expected,size)==0;
}

static int test_record_level_rejects_invalid_receiver_metadata(void)
{
    SkipDBRecord *record = SkipDBRecord_new();
    SkipDBPointer *original_pointers;
    int invalid = 0;

    if (NULL == record) {
        fprintf(stderr, "record-level regression allocation failed\\n");
        return 0;
    }
    SkipDBRecord_level_(record, 2);
    if (record->level != 2 || NULL == record->pointers) {
        fprintf(stderr, "record-level regression could not allocate pointer storage\\n");
        SkipDBRecord_dealloc(record);
        return 0;
    }
    original_pointers = record->pointers;

    /* A negative old level must never be used as a pointer-array offset. */
    record->level = -1;
    SkipDBRecord_level_(record, 3);
    if (record->level != -1 || record->pointers != original_pointers) {
        fprintf(stderr, "negative existing record level was used for pointer growth\\n");
        invalid = 1;
    }
    if (invalid) {
        SkipDBRecord_dealloc(record);
        return 0;
    }

    /* An out-of-range positive level is equally untrusted resize metadata. */
    record->level = SKIPDB_MAX_LEVEL + 1;
    SkipDBRecord_level_(record, 1);
    if (record->level != SKIPDB_MAX_LEVEL + 1 ||
        record->pointers != original_pointers) {
        fprintf(stderr, "out-of-range existing record level was modified\\n");
        invalid = 1;
    }

    /* Deallocation owns the allocation and is safe for either malformed level. */
    SkipDBRecord_dealloc(record);
    return !invalid;
}

static SkipDB *reentrant_callback_db = NULL;
static int reentrant_callback_calls = 0;

static void reentrant_object_free(void *object)
{
    (void)object;
    ++reentrant_callback_calls;
    if (reentrant_callback_calls == 1 && reentrant_callback_db)
        SkipDB_clearCache(reentrant_callback_db);
}

int main(void)
{
 if (!test_record_level_rejects_invalid_receiver_metadata())
  return EXIT_FAILURE;
 SkipDB *source=NULL,*target=NULL;SkipDBCursor *cursor=NULL,*foreignCursor=NULL,*transferCursor=NULL;SkipDBRecord *record;
 Datum prefix=Datum_FromCString_("al");char sp[128],tp[128];int sl,tl,result=EXIT_FAILURE;
 source=SkipDB_new();target=SkipDB_new();
 if(!source||!target){fprintf(stderr,"SkipDB allocation failed\n");goto done;}
 sl=snprintf(sp,sizeof(sp),"skipdb_core_layers_source_%ld",(long)getpid());
 tl=snprintf(tp,sizeof(tp),"skipdb_core_layers_target_%ld",(long)getpid());
 if(sl<0||(size_t)sl>=sizeof(sp)||tl<0||(size_t)tl>=sizeof(tp)){fprintf(stderr,"test path overflow\n");goto done;}
 SkipDB_setPath_(source,sp);SkipDB_setPath_(target,tp);SkipDB_delete(source);SkipDB_delete(target);
 if(!SkipDB_open(source)||!SkipDB_isOpen(source)||!SkipDB_open(target)||!SkipDB_isOpen(target)){fprintf(stderr,"lifecycle open failed\n");goto done;}
 cursor=SkipDBCursor_newWithDB_(source);
 if(!cursor||cursor->sdb!=source||!List_contains_(source->cursors,cursor)){
  fprintf(stderr,"direct cursor constructor left the cursor unregistered\n");goto done;}
 SkipDBCursor_release(cursor);cursor=NULL;
 SkipDB_beginTransaction(source);
 if(!SkipDB_isOpen(source)||
  !SkipDB_at_put_(source,Datum_FromCString_("alpha"),Datum_FromCString_("1"))||
  !SkipDB_at_put_(source,Datum_FromCString_("alpine"),Datum_FromCString_("2"))||
  !SkipDB_at_put_(source,Datum_FromCString_("beta"),Datum_FromCString_("3"))||
  !SkipDB_at_put_(source,Datum_FromCString_("gamma"),Datum_FromCString_("4"))||!SkipDB_isOpen(source)){
  fprintf(stderr,"insert pipeline failed\n");goto done;}
 SkipDB_commitTransaction(source);
 if(!SkipDB_isOpen(source)||SkipDB_count(source)!=4||!value_is(source,"alpha","1")||
  !value_is(source,"gamma","4")||!SkipDB_exists(source,Datum_FromCString_("beta"))){
  fprintf(stderr,"read/count/lookup contract failed\n");goto done;}
 /* Rebinding a cursor must move cache protection and discard old record state. */
 transferCursor=SkipDB_createCursor(source);
 if(!transferCursor||!key_is(SkipDBCursor_goto_(transferCursor,Datum_FromCString_("alpha")),"alpha")){
  fprintf(stderr,"cursor ownership transfer fixture failed\n");goto done;}
 {
  SkipDBCursor *savedCursor=transferCursor;
  SkipDBCursor_sdb_(transferCursor,target);
  if(transferCursor->sdb!=target||List_contains_(source->cursors,transferCursor)||
     !List_contains_(target->cursors,transferCursor)||SkipDBCursor_current(transferCursor)!=NULL||
     transferCursor->nextSteps!=0||transferCursor->previousSteps!=0){
   fprintf(stderr,"cursor rebinding did not transfer registration and reset position\n");goto done;
  }
  SkipDBCursor_sdb_(transferCursor,NULL);
  if(transferCursor->sdb!=NULL||List_contains_(target->cursors,transferCursor)){
   fprintf(stderr,"cursor detachment left stale database registration\n");goto done;
  }
  SkipDBCursor_release(transferCursor);transferCursor=NULL;
  if(List_contains_(source->cursors,savedCursor)||List_contains_(target->cursors,savedCursor)){
   fprintf(stderr,"released cursor remained registered with a database\n");goto done;
  }
 }
 /* A corrupt negative root level is not an empty-list search result. */
 {
  int saved_level=source->header->level;
  source->header->level=-1;
  (void)SkipDB_at_(source,Datum_FromCString_("alpha"));
  if(SkipDB_isOpen(source)){
   source->header->level=saved_level;
   fprintf(stderr,"negative root level was treated as an empty search\n");goto done;
  }
  if(!SkipDB_open(source)||!SkipDB_isOpen(source)||SkipDB_count(source)!=4||
     !value_is(source,"alpha","1")){
   fprintf(stderr,"SkipDB failed to recover after rejecting invalid search level\n");goto done;
  }
 }
 record=SkipDB_list_first(source,prefix,&cursor);
 if(!key_is(record,"alpha")||!key_is(SkipDB_list_next(source,prefix,cursor),"alpine")||
  SkipDB_list_next(source,prefix,cursor)!=NULL){fprintf(stderr,"prefix traversal boundary failed\n");goto done;}
 SkipDBCursor_release(cursor);cursor=NULL;
 cursor=SkipDB_createCursor(source);
 if(!cursor||!key_is(SkipDBCursor_goto_(cursor,Datum_FromCString_("alpine")),"alpine")){
  fprintf(stderr,"active cache cursor setup failed\n");goto done;}
 SkipDB_setCacheHighWaterMark_(source,2);SkipDB_setCacheLowWaterMark_(source,1);
 (void)SkipDB_at_(source,Datum_FromCString_("gamma"));(void)SkipDB_at_(source,Datum_FromCString_("alpha"));
 SkipDB_beginTransaction(source);SkipDB_commitTransaction(source);
 if(!SkipDB_isOpen(source)||!key_is(SkipDBCursor_current(cursor),"alpine")){
  fprintf(stderr,"cache eviction lost active cursor record\n");goto done;}
 if(!key_is(SkipDBCursor_next(cursor),"beta")){
  fprintf(stderr,"cache eviction cursor lazy reload failed\n");goto done;}
 if(!key_is(SkipDBCursor_previous(cursor),"alpine")){
  fprintf(stderr,"cursor reverse traversal after forward step failed\n");goto done;}
 if(!key_is(SkipDBCursor_next(cursor),"beta")){
  fprintf(stderr,"cursor forward traversal after reverse step failed\n");goto done;}
 SkipDBCursor_release(cursor);cursor=NULL;
 if(!value_is(source,"alpha","1")||!value_is(source,"alpine","2")){
  fprintf(stderr,"cache eviction/lazy reload failed\n");goto done;}
 SkipDB_beginTransaction(source);
 if(!SkipDB_at_put_(source,Datum_FromCString_("alpha"),Datum_FromCString_("updated"))){
  fprintf(stderr,"update pipeline failed\n");goto done;}
 SkipDB_removeAt_(source,Datum_FromCString_("beta"));SkipDB_commitTransaction(source);
 if(!SkipDB_isOpen(source)||SkipDB_count(source)!=3||!value_is(source,"alpha","updated")||
  SkipDB_exists(source,Datum_FromCString_("beta"))){fprintf(stderr,"update/removal transaction failed\n");goto done;}
 SkipDB_beginTransaction(target);
 if(!SkipDB_mergeInto_(source,target)||!SkipDB_isOpen(source)||!SkipDB_isOpen(target)){
  fprintf(stderr,"merge pipeline failed\n");goto done;}
 SkipDB_commitTransaction(target);
 if(!SkipDB_isOpen(target)||SkipDB_count(target)!=3||!value_is(target,"alpha","updated")||
  !value_is(target,"alpine","2")||!value_is(target,"gamma","4")){
  fprintf(stderr,"merge persistence failed\n");goto done;}
 /* Cache age-list notifications must not splice a record owned by another DB. */
 {
  SkipDBRecord *foreign_record=SkipDB_recordAt_(target,Datum_FromCString_("alpha"));
  SkipDBRecord *source_youngest=source->youngestRecord;
  SkipDBRecord *target_youngest=target->youngestRecord;
  SkipDBRecord *foreign_older, *foreign_younger;
  size_t source_count=source->cachedRecordCount;
  size_t target_count=target->cachedRecordCount;

  if(!foreign_record||foreign_record->sdb!=target){
   fprintf(stderr,"foreign cache-owner fixture failed\n");goto done;
  }
  foreign_older=foreign_record->olderRecord;
  foreign_younger=foreign_record->youngerRecord;
  SkipDB_noteAccessedRecord_(source,foreign_record);
  SkipDB_noteNewRecord_(source,foreign_record);
  if(source->youngestRecord!=source_youngest||
     target->youngestRecord!=target_youngest||
     source->cachedRecordCount!=source_count||
     target->cachedRecordCount!=target_count||
     foreign_record->olderRecord!=foreign_older||
     foreign_record->youngerRecord!=foreign_younger){
   fprintf(stderr,"foreign record corrupted cache age-list ownership\n");goto done;
  }
 }
 /* A cached forward pointer may not cross database ownership. */
 {
  SkipDBRecord *source_alpha=SkipDB_recordAt_(source,Datum_FromCString_("alpha"));
  SkipDBRecord *target_alpha=SkipDB_recordAt_(target,Datum_FromCString_("alpha"));
  if(!source_alpha||!target_alpha||
     !key_is(SkipDBRecord_recordAtLevel_(source_alpha,0),"alpine")){
   fprintf(stderr,"cached forward-link ownership fixture failed\n");goto done;
  }
  source_alpha->pointers[0].record=target_alpha;
  if(SkipDBRecord_recordAtLevel_(source_alpha,0)!=NULL||SkipDB_isOpen(source)){
   fprintf(stderr,"cached cross-database forward pointer was accepted\n");goto done;
  }
  if(!SkipDB_open(source)||!SkipDB_isOpen(source)||SkipDB_count(source)!=3||
     !value_is(source,"alpha","updated")||!value_is(source,"gamma","4")){
   fprintf(stderr,"database did not recover after cross-database cache rejection\n");goto done;
  }
 }
 /* A cached pointer must also match its nonzero persistent PID. */
 {
  SkipDBRecord *source_alpha=SkipDB_recordAt_(source,Datum_FromCString_("alpha"));
  SkipDBRecord *source_gamma=SkipDB_recordAt_(source,Datum_FromCString_("gamma"));
  if(!source_alpha||!source_gamma||
     !key_is(SkipDBRecord_recordAtLevel_(source_alpha,0),"alpine")){
   fprintf(stderr,"cached forward-PID fixture failed\n");goto done;
  }
  source_alpha->pointers[0].pid=SkipDBRecord_pid(source_gamma);
  if(SkipDBRecord_recordAtLevel_(source_alpha,0)!=NULL||SkipDB_isOpen(source)){
   fprintf(stderr,"cached forward PID mismatch was accepted\n");goto done;
  }
  if(!SkipDB_open(source)||!SkipDB_isOpen(source)||SkipDB_count(source)!=3||
     !value_is(source,"alpine","2")||!value_is(source,"gamma","4")){
   fprintf(stderr,"database did not recover after cached PID rejection\n");goto done;
  }
 }
 /* A prefix cursor is owned by exactly one SkipDB instance. */
 foreignCursor=SkipDB_createCursor(target);
 if(!foreignCursor||!key_is(SkipDBCursor_goto_(foreignCursor,Datum_FromCString_("alpha")),"alpha")){
  fprintf(stderr,"foreign prefix cursor fixture failed\n");goto done;}
 if(SkipDB_list_next(source,prefix,foreignCursor)!=NULL||
    !key_is(SkipDBCursor_current(foreignCursor),"alpha")){
  fprintf(stderr,"SkipDB_list_next advanced a cursor owned by another database\n");goto done;}
 SkipDBCursor_release(foreignCursor);foreignCursor=NULL;
 if(!key_is(SkipDB_firstRecord(source),"alpha")||!key_is(SkipDB_lastRecord(source),"gamma")||
  !key_is(SkipDB_goto_(source,Datum_FromCString_("al")),"alpha")){
  fprintf(stderr,"first/last/lower-bound query failed\n");goto done;}

	/* A cyclic age chain must abort eviction before any node is freed. */
	{
		SkipDBRecord *youngest = source->youngestRecord;
		SkipDBRecord *oldest = youngest;
		size_t cache_steps = 1;

		if (NULL == youngest || source->cachedRecordCount < 2) {
			fprintf(stderr, "cache age-list cycle fixture is unavailable\n");
			goto done;
		}
		while (oldest->olderRecord != NULL &&
			cache_steps < source->cachedRecordCount) {
			oldest = oldest->olderRecord;
			cache_steps++;
		}
		if (oldest->olderRecord != NULL ||
			cache_steps != source->cachedRecordCount) {
			fprintf(stderr, "cache age-list fixture count is inconsistent\n");
			goto done;
		}
		oldest->olderRecord = youngest;
		youngest->youngerRecord = oldest;
		SkipDB_setCacheHighWaterMark_(source, 0);
		SkipDB_beginTransaction(source);
		SkipDB_commitTransaction(source);
		if (SkipDB_isOpen(source)) {
			fprintf(stderr, "cyclic cache age list was not rejected before eviction\n");
			goto done;
		}
		if (!SkipDB_open(source) || !SkipDB_isOpen(source) ||
			SkipDB_count(source) != 3 ||
			!value_is(source, "alpha", "updated") ||
			!value_is(source, "gamma", "4")) {
			fprintf(stderr, "cache age-list cycle was not repaired during reopen cleanup\n");
			goto done;
		}
	}
	/* A user object finalizer may reenter cache teardown. Its record must
	 * already be detached so nested cleanup cannot finalize/free it twice. */
	{
		int object_token = 0;
		SkipDBRecord *callback_record = SkipDB_recordAt_(
			source, Datum_FromCString_("alpha"));

		if (NULL == callback_record || callback_record == source->header) {
			fprintf(stderr, "reentrant callback record fixture failed\n");
			goto done;
		}
		SkipDB_noteAccessedRecord_(source, callback_record);
		SkipDBRecord_object_(callback_record, &object_token);
		reentrant_callback_db = source;
		reentrant_callback_calls = 0;
		SkipDB_freeObjectCallback_(source, reentrant_object_free);
		SkipDB_clearCache(source);
		SkipDB_freeObjectCallback_(source, NULL);
		reentrant_callback_db = NULL;

		if (reentrant_callback_calls != 1 || !SkipDB_isOpen(source) ||
			SkipDB_count(source) != 3 || !value_is(source, "alpha", "updated") ||
			!value_is(source, "gamma", "4")) {
			fprintf(stderr, "cache teardown reentered object finalization or damaged the database\n");
			goto done;
		}
	}
 /*
 * Eviction finalizers can recursively clear the cache. The outer LRU walk
 * must not follow the next pointer after nested teardown has freed it.
 */
{
    int eviction_object_token = 0;
    SkipDBRecord *older_record = SkipDB_recordAt_(
        source, Datum_FromCString_("gamma"));
    SkipDBRecord *callback_record = SkipDB_recordAt_(
        source, Datum_FromCString_("alpha"));

    if (NULL == older_record || NULL == callback_record ||
        callback_record == source->header) {
        fprintf(stderr, "eviction callback record fixture failed\\n");
        goto done;
    }
    SkipDB_noteAccessedRecord_(source, older_record);
    SkipDB_noteAccessedRecord_(source, callback_record);
    if (callback_record->olderRecord != older_record) {
        fprintf(stderr, "eviction callback fixture did not establish adjacent age-list records\\n");
        goto done;
    }

    SkipDBRecord_object_(callback_record, &eviction_object_token);
    reentrant_callback_db = source;
    reentrant_callback_calls = 0;
    SkipDB_freeObjectCallback_(source, reentrant_object_free);
    SkipDB_setCacheHighWaterMark_(source, 1);
    SkipDB_setCacheLowWaterMark_(source, 0);
    SkipDB_beginTransaction(source);
    SkipDB_commitTransaction(source);
    SkipDB_freeObjectCallback_(source, NULL);
    reentrant_callback_db = NULL;

    if (reentrant_callback_calls != 1 || !SkipDB_isOpen(source) ||
        SkipDB_count(source) != 3 ||
        !value_is(source, "alpha", "updated") ||
        !value_is(source, "gamma", "4")) {
        fprintf(stderr, "cache eviction followed a stale age-list pointer after reentrant cleanup\\n");
        goto done;
    }
}
/* A failed sync must abort, not commit the still-open UDB transaction. */
 {
  SkipDBRecord *saved_header=source->header;
  SkipDB_beginTransaction(source);
  if(!SkipDB_at_put_(source,Datum_FromCString_("abort-on-missing-root"),
      Datum_FromCString_("must-not-commit"))){
   fprintf(stderr,"transaction abort fixture write failed\n");goto done;
  }
  source->header=NULL;
  SkipDB_commitTransaction(source);
  source->header=saved_header;
  if(SkipDB_isOpen(source)){
   fprintf(stderr,"commit proceeded with missing root record\n");goto done;
  }
 }
 result=EXIT_SUCCESS;puts("skipdb core layers test passed");
done:
 if(transferCursor)SkipDBCursor_release(transferCursor);
 if(foreignCursor)SkipDBCursor_release(foreignCursor);
 if(cursor)SkipDBCursor_release(cursor);
 if(source){SkipDB_delete(source);SkipDB_free(source);}
 if(target){SkipDB_delete(target);SkipDB_free(target);}
 return result;
}
