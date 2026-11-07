
#if defined(ON_WINDOWS) || defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(_MSC_VER) || defined(__MINGW32__)
	#include <windows.h>
	int usleep(unsigned int us);
#else
	#include <unistd.h>
#endif

