
#include "PortableTruncate.h"


int PortableTruncate_justHereToAvoidRanlibWarning(void) { return 0; }

#ifdef _WIN32

#include <errno.h>
#include <windows.h>

/* Win32 adapter for the POSIX truncate contract. */
int truncate(const char *path, long length)
{
	HANDLE file;
	if (NULL == path || length < 0)
	{
		errno = EINVAL;
		return -1;
	}

	file = CreateFileA(path, GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
		return -1;

	/* A nonnegative long cannot produce INVALID_SET_FILE_POINTER on Win32. */
	if (SetFilePointer(file, length, NULL, FILE_BEGIN) ==
			INVALID_SET_FILE_POINTER || !SetEndOfFile(file))
	{
		CloseHandle(file);
		return -1;
	}

	CloseHandle(file);
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
