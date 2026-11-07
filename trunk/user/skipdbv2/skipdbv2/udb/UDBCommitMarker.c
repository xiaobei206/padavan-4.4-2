#include "UDBCommitMarker.h"
#include "JFileInternal.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

int UDBCommitMarker_setPath_(UDBCommitMarker *self, const char *path)
{
    size_t n; char *copy;
    if (!self || !path || self->file) { errno=EINVAL; return -1; }
    n=strlen(path);
    if (n==(size_t)-1) { errno=ENAMETOOLONG; return -1; }
    copy=(char *)malloc(n+1);
    if (!copy) return -1;
    memcpy(copy,path,n+1); free(self->path); self->path=copy;
    return 0;
}
void UDBCommitMarker_close(UDBCommitMarker *self)
{
    if (self && self->file) { fclose(self->file); self->file=NULL; }
}
void UDBCommitMarker_clear(UDBCommitMarker *self)
{
    if (!self) return;
    UDBCommitMarker_close(self); free(self->path); self->path=NULL;
}
/* Validate an opened path still names the expected regular, owned file. */
static int UDBCommitMarker_verifyOpenedFile_(int fd, const struct stat *before, int existed)
{
    struct stat after;
    if (fstat(fd,&after)!=0) { int e=errno; close(fd); errno=e; return -1; }
    if (!S_ISREG(after.st_mode) || after.st_nlink!=1 || after.st_uid!=geteuid() ||
        (existed && (!before || before->st_dev!=after.st_dev || before->st_ino!=after.st_ino))) {
        close(fd); errno=EPERM; return -1;
    }
    if (fchmod(fd,0600)!=0) { int e=errno; close(fd); errno=e; return -1; }
    return 0;
}

int UDBCommitMarker_open(UDBCommitMarker *self, int create)
{
    struct stat before; int fd=-1,exists=0; FILE *file;
    if (!self || !self->path || !self->path[0] || self->file) { errno=EINVAL; return -1; }
#ifdef O_NOFOLLOW
    if (lstat(self->path,&before)==0) {
        exists=1;
        if (!S_ISREG(before.st_mode)) { errno=EINVAL; return -1; }
        fd=open(self->path,O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    } else if (errno==ENOENT && create) {
        fd=open(self->path,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
    } else return -1;
#else
    (void)create; errno=ENOTSUP; return -1;
#endif
    if (fd<0) return -1;
    if (UDBCommitMarker_verifyOpenedFile_(fd, exists ? &before : NULL, exists)!=0)
        return -1;
    file=fdopen(fd,"r+");
    if (!file) { int e=errno; close(fd); errno=e; return -1; }
    self->file=file; return 0;
}
int UDBCommitMarker_read(UDBCommitMarker *self)
{
    int flag,trailing;
    if (!self || !self->file || fseek(self->file,0,SEEK_SET)!=0) return -2;
    flag=fgetc(self->file);
    if (flag==EOF) return ferror(self->file)?-2:-1;
    if (flag!=0 && flag!=1) return -2;
    trailing=fgetc(self->file);
    return (trailing==EOF && !ferror(self->file))?flag:-2;
}
int UDBCommitMarker_write(UDBCommitMarker *self, int flag)
{
    int fd;
    if (!self || !self->file || (flag!=0 && flag!=1) ||
        fseek(self->file,0,SEEK_SET)!=0 || fputc(flag,self->file)==EOF ||
        fflush(self->file)!=0) return -1;
    fd=fileno(self->file); if(fd<0)return -1;
    /* Share the EINTR-safe platform durability barrier used by JFile. */
    return JFile_hardSyncFileDescriptor_(NULL,fd);
}
static FILE *UDBCommitMarker_openJournal_(const char *path)
{
    struct stat before; int fd; FILE *file;
    if (!path || !path[0]) { errno=EINVAL; return NULL; }
#ifdef O_NOFOLLOW
    if (lstat(path,&before)!=0)return NULL;
    if (!S_ISREG(before.st_mode)) { errno=EINVAL; return NULL; }
    fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
#else
    errno=ENOTSUP; return NULL;
#endif
    if(fd<0)return NULL;
    if(UDBCommitMarker_verifyOpenedFile_(fd,&before,1)!=0)return NULL;
    file=fdopen(fd,"rb");
    if(!file){int e=errno;close(fd);errno=e;}
    return file;
}
int UDBCommitMarker_verifyJournal(const char *path)
{
    FILE *log=UDBCommitMarker_openJournal_(path); JFile journal; int valid;
    if(!log)return 0;
    memset(&journal,0,sizeof(journal)); journal.log=log;
    valid=JFile_verifyLog(&journal);
    if(fclose(log)!=0)valid=0;
    return valid;
}
int UDBCommitMarker_syncParentDirectory(const char *path)
{
    char *dir,*slash; size_t n; int fd,result,saved_errno;
    if(!path||!path[0]){errno=EINVAL;return -1;}
    n=strlen(path);dir=(char *)malloc(n+1);if(!dir)return -1;
    memcpy(dir,path,n+1);slash=strrchr(dir,'/');
    if(!slash){dir[0]='.';dir[1]='\0';}
    else if(slash==dir)slash[1]='\0';
    else *slash='\0';
    fd=open(dir,O_RDONLY|O_CLOEXEC);free(dir);if(fd<0)return -1;
    result=JFile_hardSyncFileDescriptor_(NULL,fd);saved_errno=errno;
    if(close(fd)!=0&&result==0){result=-1;saved_errno=errno;}
    errno=saved_errno;return result;
}
static int UDBCommitMarker_pathStatus_(const char *path,struct stat *st)
{
    if(!path||!path[0]||!st){errno=EINVAL;return -1;}
    if(lstat(path,st)==0)return 1;
    return errno==ENOENT?0:-1;
}
int UDBCommitMarker_artifactsExist(const char *const paths[],size_t count)
{
    struct stat st;size_t i;
    if(!paths&&count){errno=EINVAL;return -1;}
    for(i=0;i<count;++i){
        int state=UDBCommitMarker_pathStatus_(paths[i],&st);
        if(state<0)return -1;
        if(state>0)return 1;
    }
    return 0;
}
int UDBCommitMarker_artifactsAreEmpty(const char *const paths[],size_t count)
{
    struct stat st;size_t i;
    if(!paths&&count)return 0;
    for(i=0;i<count;++i){
        int state=UDBCommitMarker_pathStatus_(paths[i],&st);
        if(state<0||(state>0&&(!S_ISREG(st.st_mode)||st.st_size!=0)))return 0;
    }
    return 1;
}
