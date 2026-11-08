#include <assert.h>
#include <signal.h>

#include "pid1_signal_policy.h"

int
main(void)
{
	pid1_signal_state_t state = { 0, 0, 0, 0 };

	pid1_signal_record_event(&state, SIGUSR1);
	assert(state.usr1_pending == 1);
	assert(state.alarm_pending == 0);

	pid1_signal_record_event(&state, SIGALRM);
	assert(state.usr1_pending == 1);
	assert(state.alarm_pending == 1);

	pid1_signal_record_shutdown(&state, SIGTERM);
	assert(state.shutdown_pending == 1);
	assert(state.shutdown_signal == SIGTERM);
	assert(state.usr1_pending == 0);
	assert(state.alarm_pending == 0);

	/* Repeated shutdown requests preserve the first selected action. */
	pid1_signal_record_shutdown(&state, SIGQUIT);
	assert(state.shutdown_signal == SIGTERM);

	/* Events arriving after shutdown cannot restart deferred work. */
	pid1_signal_record_event(&state, SIGUSR1);
	pid1_signal_record_event(&state, SIGALRM);
	assert(state.usr1_pending == 0);
	assert(state.alarm_pending == 0);

	return 0;
}
