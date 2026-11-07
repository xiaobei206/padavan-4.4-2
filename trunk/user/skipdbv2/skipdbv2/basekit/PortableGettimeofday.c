#include <time.h>
#include <errno.h>
#include <limits.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include "PortableGettimeofday.h"

#if defined(__WIN32__) || defined(WIN32) || defined(_WIN32) || defined(_MSC_VER)

	#if defined(__MINGW32__) && (3 < __MINGW32_MAJOR_VERSION || 3 == __MINGW32_MAJOR_VERSION && 9 < __MINGW32_MINOR_VERSION)
	#else

		#ifndef IO_ADDON_Sockets
			int gettimeofday(struct timeval *tv, struct timezone *tz)
			{
				TIME_ZONE_INFORMATION zoneInfo;
				struct _timeb timeb;
				DWORD zoneStatus;
				LONG bias;
				int status;

				if (NULL == tv)
				{
					errno = EINVAL;
					return -1;
				}
				status = (int)_ftime_s(&timeb);
				if (status != 0)
				{
					errno = status;
					return -1;
				}

				/* timeval.tv_sec is long on Windows; reject 64-bit time_t overflow. */
				if (timeb.time > (time_t)LONG_MAX || timeb.time < (time_t)LONG_MIN)
				{
					errno = EOVERFLOW;
					return -1;
				}
				tv->tv_sec = (long)timeb.time;
				tv->tv_usec = (long)(timeb.millitm * 1000);

				if (tz)
				{
					zoneStatus = GetTimeZoneInformation(&zoneInfo);
					if (zoneStatus != TIME_ZONE_ID_INVALID)
					{
						bias = zoneInfo.Bias;
						if (zoneStatus == TIME_ZONE_ID_STANDARD)
							bias += zoneInfo.StandardBias;
						else if (zoneStatus == TIME_ZONE_ID_DAYLIGHT)
							bias += zoneInfo.DaylightBias;
						tz->tz_minuteswest = (int)bias;
						tz->tz_dsttime = 0;
					}
					else
					{
						tz->tz_minuteswest = 0;
						tz->tz_dsttime = 0;
					}
				}
				return 0;
			}
		#endif
	#endif
#else

	/* just to make compiler happy */
	void PortableGettimeOfday(void)
	{
	}

#endif

double secondsSince1970(void)
{
	struct timeval tv;
	struct timezone tz;
	if (gettimeofday(&tv, &tz) != 0)
		return 0.0;
	return (double)tv.tv_sec + ((double)tv.tv_usec / 1000000.0);
}




