#ifndef PADAVAN_FTP_SHARE_POLICY_H
#define PADAVAN_FTP_SHARE_POLICY_H

/*
 * The management UI exposes only modes 1..4. Treat corrupted, stale, or
 * unsupported NVRAM values as mode 2: authenticated local users only.
 */
static int
ftp_share_mode_normalize(int mode)
{
	switch (mode) {
	case 1:
	case 2:
	case 3:
	case 4:
		return mode;
	default:
		return 2;
	}
}

#endif /* PADAVAN_FTP_SHARE_POLICY_H */
