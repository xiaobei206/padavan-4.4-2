/*
 * Decode system(3)/wait status values for firewall and extension commands.
 */
#ifndef FIREWALL_COMMAND_STATUS_H
#define FIREWALL_COMMAND_STATUS_H

#include <sys/wait.h>

static int
firewall_command_status_ok(int status)
{
	return status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static int
firewall_command_status_exit_code(int status)
{
	if (status < 0 || !WIFEXITED(status))
		return -1;

	return WEXITSTATUS(status);
}

static int
firewall_command_status_signal(int status)
{
	if (status < 0 || !WIFSIGNALED(status))
		return 0;

	return WTERMSIG(status);
}

#endif
