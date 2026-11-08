#include <assert.h>
#include <signal.h>
#include "../firewall_command_status.h"

int
main(void)
{
	int status;

	assert(firewall_command_status_ok(0));
	assert(!firewall_command_status_ok(-1));
	assert(!firewall_command_status_ok(1 << 8));

	assert(firewall_command_status_exit_code(0) == 0);
	assert(firewall_command_status_exit_code(7 << 8) == 7);
	assert(firewall_command_status_exit_code(SIGTERM) == -1);
	assert(firewall_command_status_exit_code(-1) == -1);

	assert(firewall_command_status_signal(SIGTERM) == SIGTERM);
	assert(firewall_command_status_signal(1 << 8) == 0);
	assert(firewall_command_status_signal(-1) == 0);

	status = 127 << 8;
	assert(!firewall_command_status_ok(status));
	assert(firewall_command_status_exit_code(status) == 127);

	{
		firewall_restore_state_t state;

		firewall_restore_state_reset(&state);
		assert(firewall_restore_state_ok(&state));

		firewall_restore_state_record(&state, 0);
		assert(firewall_restore_state_ok(&state));

		firewall_restore_state_record(&state, 7 << 8);
		assert(!firewall_restore_state_ok(&state));

		/* Later table success must not clear an earlier failure. */
		firewall_restore_state_record(&state, 0);
		assert(!firewall_restore_state_ok(&state));

		firewall_restore_state_fail(&state);
		assert(!firewall_restore_state_ok(&state));

		firewall_restore_state_reset(&state);
		assert(firewall_restore_state_ok(&state));
		assert(!firewall_restore_state_ok(NULL));
	}

	return 0;
}
