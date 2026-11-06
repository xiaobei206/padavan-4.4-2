#define _GNU_SOURCE
#include "UDBCommitMarker.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
static int fail(const char *s){fprintf(stderr,"commit_marker_test: %s\n",s);return 1;}
static int join(char *o,size_t n,const char *d,const char *f){int k=snprintf(o,n,"%s/%s",d,f);return k>0&&(size_t)k<n;}
static int journal(const char *p,int term){
 FILE *f=fopen(p,"wb");long pos=0;size_t n=1;char c='x';int ok;
 if(!f)return 0;
 ok=fwrite(&pos,sizeof(pos),1,f)==1&&fwrite(&n,sizeof(n),1,f)==1&&
 fwrite(&c,1,1,f)==1&&fputc(term,f)!=EOF;
 if(fclose(f)!=0)ok=0;
 return ok;
}
#define CHECK(x,m) do{if(!(x)){result=fail(m);goto done;}}while(0)
int main(void){
 char dir[]="/tmp/udb-marker.XXXXXX",mp[256]="",dp[256]="",jp[256]="",tp[256]="",lp[256]="",hp[256]="";
 const char *paths[2];UDBCommitMarker marker={NULL,NULL};int result=0,fd=-1;char c='d';
 CHECK(mkdtemp(dir)!=NULL,"mkdtemp");
 CHECK(join(mp,sizeof(mp),dir,"marker")&&join(dp,sizeof(dp),dir,"data")&&
 join(jp,sizeof(jp),dir,"journal")&&join(tp,sizeof(tp),dir,"target")&&
 join(lp,sizeof(lp),dir,"link")&&join(hp,sizeof(hp),dir,"hardlink"),"path setup");
 paths[0]=dp;paths[1]=jp;
 CHECK(UDBCommitMarker_artifactsExist(paths,2)==0&&UDBCommitMarker_artifactsAreEmpty(paths,2),"missing artifact classification");
 CHECK(UDBCommitMarker_setPath_(&marker,mp)==0,"set path");
 errno=0;CHECK(UDBCommitMarker_open(&marker,0)==-1&&errno==ENOENT,"missing marker open");
 CHECK(UDBCommitMarker_open(&marker,1)==0,"secure marker create");
 CHECK(UDBCommitMarker_write(&marker,0)==0&&UDBCommitMarker_read(&marker)==0,"clean marker roundtrip");
 CHECK(UDBCommitMarker_write(&marker,1)==0&&UDBCommitMarker_read(&marker)==1,"commit marker roundtrip");
 CHECK(UDBCommitMarker_write(&marker,2)==-1&&UDBCommitMarker_read(&marker)==1,"invalid marker state");
 CHECK(UDBCommitMarker_setPath_(&marker,dp)==-1,"path mutation while open");
 CHECK(UDBCommitMarker_syncParentDirectory(mp)==0,"directory sync");
 UDBCommitMarker_close(&marker);
 fd=open(mp,O_WRONLY|O_TRUNC);CHECK(fd>=0&&write(fd,"01",2)==2&&close(fd)==0,"corrupt marker fixture");fd=-1;
 CHECK(UDBCommitMarker_open(&marker,0)==0&&UDBCommitMarker_read(&marker)==-2,"multi-byte marker");
 UDBCommitMarker_close(&marker);
 fd=open(mp,O_WRONLY|O_TRUNC);CHECK(fd>=0&&close(fd)==0,"empty marker fixture");fd=-1;
 CHECK(UDBCommitMarker_open(&marker,0)==0&&UDBCommitMarker_read(&marker)==-1,"empty marker state");
 CHECK(UDBCommitMarker_artifactsAreEmpty(paths,2)&&UDBCommitMarker_write(&marker,0)==0,"empty marker recovery");
 UDBCommitMarker_close(&marker);
 fd=open(dp,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0&&close(fd)==0,"empty artifact create");fd=-1;
 CHECK(UDBCommitMarker_artifactsExist(paths,2)==1&&UDBCommitMarker_artifactsAreEmpty(paths,2),"empty artifact state");
 fd=open(dp,O_WRONLY);CHECK(fd>=0&&write(fd,&c,1)==1&&close(fd)==0,"nonempty artifact write");fd=-1;
 CHECK(!UDBCommitMarker_artifactsAreEmpty(paths,2)&&unlink(dp)==0,"nonempty artifact state");
 fd=open(tp,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0&&close(fd)==0,"symlink target");fd=-1;
 CHECK(symlink(tp,lp)==0&&UDBCommitMarker_setPath_(&marker,lp)==0,"symlink setup");
 CHECK(UDBCommitMarker_open(&marker,0)==-1,"symlink followed");
 CHECK(UDBCommitMarker_setPath_(&marker,mp)==0,"restore path");
 CHECK(link(mp,hp)==0,"hard-link marker");
 CHECK(UDBCommitMarker_setPath_(&marker,hp)==0,"hard-link marker path");
 errno=0;CHECK(UDBCommitMarker_open(&marker,0)==-1&&errno==EPERM,"hard-linked marker accepted");
 CHECK(UDBCommitMarker_setPath_(&marker,mp)==0&&unlink(hp)==0,"remove marker hard-link");
 CHECK(journal(jp,2)&&UDBCommitMarker_verifyJournal(jp)==1,"complete journal rejected");
 CHECK(link(jp,hp)==0,"hard-link journal");
 CHECK(UDBCommitMarker_verifyJournal(jp)==0&&UDBCommitMarker_verifyJournal(hp)==0,"hard-linked journal accepted");
 CHECK(unlink(hp)==0,"remove journal hard-link");
 CHECK(journal(jp,1)&&UDBCommitMarker_verifyJournal(jp)==0,"incomplete journal accepted");
done:
 if(fd>=0)close(fd);
 UDBCommitMarker_clear(&marker);
 if(mp[0])unlink(mp);
 if(dp[0])unlink(dp);
 if(jp[0])unlink(jp);
 if(hp[0])unlink(hp);
 if(tp[0])unlink(tp);
 if(lp[0])unlink(lp);
 rmdir(dir);
 if(result==0)puts("UDB commit marker tests passed");
 return result;
}
