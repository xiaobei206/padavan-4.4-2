#ifndef PID1_SIGNAL_POLICY_H
#define PID1_SIGNAL_POLICY_H

#include <signal.h>

typedef struct {
	volatile sig_atomic_t shutdown_pending;
	volatile sig_atomic_t shutdown_signal;
	volatile sig_atomic_t usr1_pending;
	volatile sig_atomic_t alarm_pending;
} pid1_signal_state_t;

/* First shutdown request wins; discard lower-priority queued events. */
static inline void
pid1_signal_record_shutdown(pid1_signal_state_t *state, int sig)
{
	if (!state->shutdown_pending) {
		state->shutdown_signal = (sig_atomic_t)sig;
		state->shutdown_pending = 1;
	}
	state->usr1_pending = 0;
	state->alarm_pending = 0;
}

/* This routine is signal-context safe: it only reads/writes sig_atomic_t. */
static inline void
pid1_signal_record_event(pid1_signal_state_t *state, int sig)
{
	if (state->shutdown_pending)
		return;

	if (sig == SIGUSR1)
		state->usr1_pending = 1;
	else if (sig == SIGALRM)
		state->alarm_pending = 1;
}

#endif
