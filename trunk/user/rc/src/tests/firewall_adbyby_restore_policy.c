#include <stdio.h>
#include <string.h>

#include "firewall_adbyby_restore_policy.h"

typedef struct {
	int preflight_status;
	int restore_status;
	int preflight_calls;
	int restore_calls;
	int report_calls;
	int reported_status;
	const char *reported_operation;
} adbyby_restore_fixture_t;

static int
fake_blockip_preflight(void *context)
{
	adbyby_restore_fixture_t *fixture = (adbyby_restore_fixture_t *)context;
	fixture->preflight_calls++;
	return fixture->preflight_status;
}

static int
fake_restore_snapshot(void *context)
{
	adbyby_restore_fixture_t *fixture = (adbyby_restore_fixture_t *)context;
	fixture->restore_calls++;
	return fixture->restore_status;
}

static void
fake_report_failure(const char *operation, int status, void *context)
{
	adbyby_restore_fixture_t *fixture = (adbyby_restore_fixture_t *)context;
	fixture->report_calls++;
	fixture->reported_status = status;
	fixture->reported_operation = operation;
}

static int
test_missing_snapshot_is_noop(void)
{
	adbyby_restore_fixture_t fixture = {0};

	if (firewall_adbyby_restore_policy_apply(0, fake_blockip_preflight,
		fake_restore_snapshot, fake_report_failure, &fixture) != 0)
		return 1;
	return fixture.preflight_calls == 0 && fixture.restore_calls == 0 &&
		fixture.report_calls == 0 ? 0 : 1;
}

static int
test_missing_ipset_skips_restore(void)
{
	adbyby_restore_fixture_t fixture = {0};

	fixture.preflight_status = 256; /* system(3): command returned failure */
	if (firewall_adbyby_restore_policy_apply(1, fake_blockip_preflight,
		fake_restore_snapshot, fake_report_failure, &fixture) == 0)
		return 1;
	return fixture.preflight_calls == 1 && fixture.restore_calls == 0 &&
		fixture.report_calls == 1 && fixture.reported_status == 256 &&
		fixture.reported_operation &&
		strcmp(fixture.reported_operation, "Adbyby blockip ipset preflight") == 0
		? 0 : 1;
}

static int
test_failed_extension_restore_is_reported(void)
{
	adbyby_restore_fixture_t fixture = {0};

	fixture.restore_status = 256;
	if (firewall_adbyby_restore_policy_apply(1, fake_blockip_preflight,
		fake_restore_snapshot, fake_report_failure, &fixture) == 0)
		return 1;
	return fixture.preflight_calls == 1 && fixture.restore_calls == 1 &&
		fixture.report_calls == 1 && fixture.reported_status == 256 &&
		fixture.reported_operation &&
		strcmp(fixture.reported_operation, "Adbyby iptables restore") == 0
		? 0 : 1;
}

static int
test_successful_restore(void)
{
	adbyby_restore_fixture_t fixture = {0};

	if (firewall_adbyby_restore_policy_apply(1, fake_blockip_preflight,
		fake_restore_snapshot, fake_report_failure, &fixture) != 0)
		return 1;
	return fixture.preflight_calls == 1 && fixture.restore_calls == 1 &&
		fixture.report_calls == 0 ? 0 : 1;
}

int
main(void)
{
	if (test_missing_snapshot_is_noop() != 0 ||
	    test_missing_ipset_skips_restore() != 0 ||
	    test_failed_extension_restore_is_reported() != 0 ||
	    test_successful_restore() != 0) {
		fprintf(stderr, "Adbyby restore failure-path regression failed\n");
		return 1;
	}
	puts("Adbyby restore failure-path regression: OK");
	return 0;
}
