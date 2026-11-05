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
int main(void)
{
 SkipDB *source=NULL,*target=NULL;SkipDBCursor *cursor=NULL;SkipDBRecord *record;
 Datum prefix=Datum_FromCString_("al");char sp[128],tp[128];int sl,tl,result=EXIT_FAILURE;
 source=SkipDB_new();target=SkipDB_new();
 if(!source||!target){fprintf(stderr,"SkipDB allocation failed\n");goto done;}
 sl=snprintf(sp,sizeof(sp),"skipdb_core_layers_source_%ld",(long)getpid());
 tl=snprintf(tp,sizeof(tp),"skipdb_core_layers_target_%ld",(long)getpid());
 if(sl<0||(size_t)sl>=sizeof(sp)||tl<0||(size_t)tl>=sizeof(tp)){fprintf(stderr,"test path overflow\n");goto done;}
 SkipDB_setPath_(source,sp);SkipDB_setPath_(target,tp);SkipDB_delete(source);SkipDB_delete(target);
 if(!SkipDB_open(source)||!SkipDB_isOpen(source)||!SkipDB_open(target)||!SkipDB_isOpen(target)){fprintf(stderr,"lifecycle open failed\n");goto done;}
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
 if(!key_is(SkipDB_firstRecord(source),"alpha")||!key_is(SkipDB_lastRecord(source),"gamma")||
  !key_is(SkipDB_goto_(source,Datum_FromCString_("al")),"alpha")){
  fprintf(stderr,"first/last/lower-bound query failed\n");goto done;}
 result=EXIT_SUCCESS;puts("skipdb core layers test passed");
done:
 if(cursor)SkipDBCursor_release(cursor);
 if(source){SkipDB_delete(source);SkipDB_free(source);}
 if(target){SkipDB_delete(target);SkipDB_free(target);}
 return result;
}
