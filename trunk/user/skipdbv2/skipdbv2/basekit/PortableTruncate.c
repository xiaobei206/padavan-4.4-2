
#include "PortableTruncate.h"


int PortableTruncate_justHereToAvoidRanlibWarning(void) { return 0; }

#ifdef _WIN32

#include <errno.h>
#include <windows.h>

/* Translate Win32 failures into stable errno values for POSIX callers. */
static int PortableTruncate_setErrnoFromWindowsError_(DWORD error)
{
	switch (error) {
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
	case ERROR_INVALID_DRIVE:
		errno = ENOENT;
		break;
	case ERROR_ACCESS_DENIED:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
		errno = EACCES;
		break;
	case ERROR_INVALID_HANDLE:
		errno = EBADF;
		break;
	case ERROR_NOT_ENOUGH_MEMORY:
	case ERROR_OUTOFMEMORY:
		errno = ENOMEM;
		break;
	case ERROR_INVALID_PARAMETER:
		errno = EINVAL;
		break;
	case ERROR_DISK_FULL:
	case ERROR_HANDLE_DISK_FULL:
		errno = ENOSPC;
		break;
	default:
		errno = EIO;
		break;
	}
	return -1;
}

/* Win32 adapter for the POSIX truncate contract. */
int truncate(const char *path, long length)
{
	HANDLE file;
	DWORD error;
	if (NULL == path || length < 0)
	{
		errno = EINVAL;
		return -1;
	}

	file = CreateFileA(path, GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE) {
		error = GetLastError();
		return PortableTruncate_setErrnoFromWindowsError_(error);
	}

	/* A nonnegative long cannot produce INVALID_SET_FILE_POINTER on Win32. */
	if (SetFilePointer(file, length, NULL, FILE_BEGIN) ==
			INVALID_SET_FILE_POINTER) {
		error = GetLastError();
		(void)CloseHandle(file);
		return PortableTruncate_setErrnoFromWindowsError_(error);
	}
	if (!SetEndOfFile(file)) {
		error = GetLastError();
		(void)CloseHandle(file);
		return PortableTruncate_setErrnoFromWindowsError_(error);
	}
	if (!CloseHandle(file)) {
		error = GetLastError();
		return PortableTruncate_setErrnoFromWindowsError_(error);
	}
	return 0;
}

#endif

#if defined(__SYMBIAN32__)
int truncate(const char* path, long length)
{
	// TODO: Implement for Symbian
	return -1;
}
#endif
