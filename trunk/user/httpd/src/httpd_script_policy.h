/*
 * Server-side policy for HTTPD-managed executable script files.
 *
 * A script uploaded through the management interface is stored executable
 * and may later run with root privileges. It must only be writable from a
 * trusted management session.
 */
#ifndef HTTPD_SCRIPT_POLICY_H
#define HTTPD_SCRIPT_POLICY_H

#define HTTPD_FILE_TYPE_SCRIPT 1

static int
httpd_script_file_write_allowed(int file_type, int trusted_session)
{
	return (file_type != HTTPD_FILE_TYPE_SCRIPT || trusted_session);
}

#endif
