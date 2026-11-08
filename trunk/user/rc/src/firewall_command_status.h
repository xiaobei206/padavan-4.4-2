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

typedef struct {
	int failed;
} firewall_restore_state_t;

/* Preserve the first core-table staging/restore failure for this rebuild. */
static void
firewall_restore_state_reset(firewall_restore_state_t *state)
{
	if (state)
		state->failed = 0;
}

static void
firewall_restore_state_fail(firewall_restore_state_t *state)
{
	if (state)
		state->failed = 1;
}

static void
firewall_restore_state_record(firewall_restore_state_t *state, int status)
{
	if (state && !firewall_command_status_ok(status))
		state->failed = 1;
}

static int
firewall_restore_state_ok(const firewall_restore_state_t *state)
{
	return state && !state->failed;
}

#endif
