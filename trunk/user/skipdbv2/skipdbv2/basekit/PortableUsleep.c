int PortableUsleep_justHereToAvoidRanlibWarning(void) { return 0; }

#if defined(ON_WINDOWS) || defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(_MSC_VER) || defined(__MINGW32__)
#include <limits.h>
#include <windows.h>

/* Round up without overflowing when converting the widest microsecond delay. */
static DWORD PortableUsleep_sleepMilliseconds_(unsigned int us)
{
	return (DWORD)(us / 1000U + (us % 1000U != 0U));
}

int usleep(unsigned int us)
{
	LARGE_INTEGER freq, start, now;
	LONGLONG required_ticks;
	double ticks;

	if (us == 0)
		return 0;

	/* Fall back to a coarse sleep if the high-resolution clock is unavailable. */
	if (!QueryPerformanceFrequency(&freq) || freq.QuadPart <= 0 ||
		!QueryPerformanceCounter(&start)) {
		Sleep(PortableUsleep_sleepMilliseconds_(us));
		return 0;
	}

	ticks = (double)freq.QuadPart * ((double)us / 1000000.0);
	if (ticks >= (double)LLONG_MAX) {
		Sleep(PortableUsleep_sleepMilliseconds_(us));
		return 0;
	}
	required_ticks = (LONGLONG)ticks;
	if ((double)required_ticks < ticks)
		++required_ticks;
	/* One extra counter tick avoids finishing early after floating-point rounding. */
	if (required_ticks < LLONG_MAX)
		++required_ticks;
	if (required_ticks < 1)
		required_ticks = 1;

	/* Let the OS sleep for all but the last < 2 ms, rather than spin for the
	 * entire requested interval. The elapsed-time check absorbs timer oversleep. */
	if (us >= 2000U)
		Sleep((DWORD)(us / 1000U - 1U));

	for (;;) {
		if (!QueryPerformanceCounter(&now)) {
			/* At most 2 ms remained after the coarse sleep. */
			Sleep(2);
			return 0;
		}
		if (now.QuadPart - start.QuadPart >= required_ticks)
			break;
	}
	return 0;
}

#endif
