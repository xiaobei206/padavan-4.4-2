#include <assert.h>
#include <signal.h>
#include <stddef.h>
#include <string.h>
#include "service_lifecycle_policy.h"

struct fake_service { const char *name; int running; int stop_on_term; int stop_on_kill; };
struct fake_context {
	struct fake_service *services;
	unsigned int service_count, term_signals, kill_signals, sleeps;
	unsigned int post_kill_sleeps, stop_after_post_kill_sleep;
};

static struct fake_service *
find_service(struct fake_context *context, const char *name)
{
	unsigned int i;
	for (i = 0; i < context->service_count; i++)
		if (strcmp(context->services[i].name, name) == 0)
			return &context->services[i];
	return NULL;
}

static int
fake_is_running(void *opaque, const char *name)
{
	struct fake_context *context = (struct fake_context *)opaque;
	struct fake_service *service = find_service(context, name);
	return service ? service->running : 0;
}

static int
fake_send_signal(void *opaque, const char *name, int signal_number)
{
	struct fake_context *context = (struct fake_context *)opaque;
	struct fake_service *service = find_service(context, name);
	if (!service)
		return -1;
	if (signal_number == SIGTERM) {
		context->term_signals++;
		if (service->stop_on_term)
			service->running = 0;
	} else if (signal_number == SIGKILL) {
		context->kill_signals++;
		if (service->stop_on_kill)
			service->running = 0;
	}
	/* Simulate a killall status that races with process exit. */
	return -1;
}

static void
fake_sleep(void *opaque, unsigned int seconds)
{
	struct fake_context *context = (struct fake_context *)opaque;
	context->sleeps += seconds;
	if (context->kill_signals > 0) {
		context->post_kill_sleeps += seconds;
		if (context->stop_after_post_kill_sleep > 0 &&
		    context->post_kill_sleeps >= context->stop_after_post_kill_sleep) {
			unsigned int i;
			for (i = 0; i < context->service_count; i++)
				context->services[i].running = 0;
		}
	}
}

static const struct service_lifecycle_ops fake_ops = {
	fake_is_running, fake_send_signal, fake_sleep
};

static void test_graceful_stop_uses_inventory(void)
{
	struct fake_service service = { "pppd", 1, 1, 0 };
	struct fake_context context = { &service, 1, 0, 0, 0, 0, 0 };
	char *names[] = { "pppd", NULL };
	assert(service_lifecycle_stop(names, 6, 1, &fake_ops, &context) == 0);
	assert(context.term_signals == 1 && context.kill_signals == 0 && context.sleeps == 0);
}

static void test_force_kill_waits_for_exit(void)
{
	struct fake_service service = { "pppd", 1, 0, 1 };
	struct fake_context context = { &service, 1, 0, 0, 0, 0, 0 };
	char *names[] = { "pppd", NULL };
	assert(service_lifecycle_stop(names, 6, 1, &fake_ops, &context) == 0);
	assert(context.term_signals == 1 && context.kill_signals == 1 && context.sleeps == 6);
}

static void test_force_kill_accepts_delayed_exit(void)
{
	struct fake_service service = { "xl2tpd", 1, 0, 0 };
	struct fake_context context = { &service, 1, 0, 0, 0, 0, 2 };
	char *names[] = { "xl2tpd", NULL };
	assert(service_lifecycle_stop(names, 6, 1, &fake_ops, &context) == 0);
	assert(context.kill_signals == 1 && context.post_kill_sleeps == 2 && context.sleeps == 8);
}

static void test_stubborn_service_is_reported(void)
{
	struct fake_service service = { "pppd", 1, 0, 0 };
	struct fake_context context = { &service, 1, 0, 0, 0, 0, 0 };
	char *names[] = { "pppd", NULL };
	assert(service_lifecycle_stop(names, 6, 1, &fake_ops, &context) == -1);
	assert(context.kill_signals == 1);
	assert(context.post_kill_sleeps == SERVICE_LIFECYCLE_FORCE_WAIT_SECONDS);
	assert(context.sleeps == 6 + SERVICE_LIFECYCLE_FORCE_WAIT_SECONDS);
}

static void test_nonforced_timeout_is_reported(void)
{
	struct fake_service service = { "ntpd", 1, 0, 0 };
	struct fake_context context = { &service, 1, 0, 0, 0, 0, 0 };
	char *names[] = { "ntpd", NULL };
	assert(service_lifecycle_stop(names, 3, 0, &fake_ops, &context) == -1);
	assert(context.kill_signals == 0 && context.sleeps == 3);
}

static void test_empty_service_list_succeeds(void)
{
	char *names[] = { NULL };
	struct fake_context context = { NULL, 0, 0, 0, 0, 0, 0 };
	assert(service_lifecycle_stop(names, 1, 1, &fake_ops, &context) == 0);
	assert(context.sleeps == 0);
}

int main(void)
{
	test_graceful_stop_uses_inventory();
	test_force_kill_waits_for_exit();
	test_force_kill_accepts_delayed_exit();
	test_stubborn_service_is_reported();
	test_nonforced_timeout_is_reported();
	test_empty_service_list_succeeds();
	return 0;
}
