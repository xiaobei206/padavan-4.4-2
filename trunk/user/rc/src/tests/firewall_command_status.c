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

	return 0;
}
