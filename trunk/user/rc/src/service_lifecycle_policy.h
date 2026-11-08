#ifndef SERVICE_LIFECYCLE_POLICY_H
#define SERVICE_LIFECYCLE_POLICY_H
#include <signal.h>

/* SIGKILL may remain pending while a task is in uninterruptible sleep. */
#define SERVICE_LIFECYCLE_FORCE_WAIT_SECONDS 3

struct service_lifecycle_ops {
	int (*is_running)(void *context, const char *service_name);
	int (*send_signal)(void *context, const char *service_name, int signal_number);
	void (*sleep_seconds)(void *context, unsigned int seconds);
};

static inline int
service_lifecycle_any_running(char *service_names[],
			      const struct service_lifecycle_ops *ops,
			      void *context)
{
	int i;
	for (i = 0; service_names[i] && *service_names[i]; i++) {
		if (ops->is_running(context, service_names[i]))
			return 1;
	}
	return 0;
}

static inline int
service_lifecycle_wait_for_exit(char *service_names[],
				const struct service_lifecycle_ops *ops,
				void *context, unsigned int timeout_seconds)
{
	unsigned int waited;
	for (waited = 0; waited < timeout_seconds; waited++) {
		if (!service_lifecycle_any_running(service_names, ops, context))
			return 0;
		ops->sleep_seconds(context, 1);
	}
	return service_lifecycle_any_running(service_names, ops, context) ? -1 : 0;
}

/* Process inventory is authoritative: killall may race a process that exits. */
static inline int
service_lifecycle_stop(char *service_names[], int wait_timeout, int forcekill,
		       const struct service_lifecycle_ops *ops, void *context)
{
	int i;
	if (!service_names || !ops || !ops->is_running ||
	    !ops->send_signal || !ops->sleep_seconds)
		return -1;
	if (wait_timeout < 1)
		wait_timeout = 1;

	for (i = 0; service_names[i] && *service_names[i]; i++)
		ops->send_signal(context, service_names[i], SIGTERM);
	if (service_lifecycle_wait_for_exit(service_names, ops, context,
					    (unsigned int)wait_timeout) == 0)
		return 0;
	if (!forcekill)
		return -1;

	for (i = 0; service_names[i] && *service_names[i]; i++) {
		if (ops->is_running(context, service_names[i]))
			ops->send_signal(context, service_names[i], SIGKILL);
	}
	return service_lifecycle_wait_for_exit(service_names, ops, context,
					       SERVICE_LIFECYCLE_FORCE_WAIT_SECONDS);
}
#endif
