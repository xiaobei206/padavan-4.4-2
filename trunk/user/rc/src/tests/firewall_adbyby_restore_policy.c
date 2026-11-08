#include <stdio.h>
#include <string.h>

#include "firewall_adbyby_restore_policy.h"

typedef struct {
	int preflight_status;
	int restore_status;
	char fail_snapshot_table[FIREWALL_ADBYBY_RESTORE_TABLE_SIZE];
	char fail_rollback_table[FIREWALL_ADBYBY_RESTORE_TABLE_SIZE];
	char fail_remove_path[FIREWALL_ADBYBY_RESTORE_PATH_SIZE];
	int preflight_calls;
	int restore_calls;
	int snapshot_calls;
	int rollback_calls;
	int remove_attempt_calls;
	int remove_calls;
	int report_calls;
	char rollback_order[2][FIREWALL_ADBYBY_RESTORE_TABLE_SIZE];
	char reported_operation[80];
	int reported_status;
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

static int
fake_snapshot_table(const char *table, char *path, size_t path_size, void *context)
{
	adbyby_restore_fixture_t *fixture = (adbyby_restore_fixture_t *)context;
	fixture->snapshot_calls++;
	if (snprintf(path, path_size, "/tmp/adbyby-%s.snapshot", table) < 0)
		return -1;
	return strcmp(fixture->fail_snapshot_table, table) == 0 ? -1 : 0;
}

static int
fake_restore_table(const char *table, const char *path, int rollback, void *context)
{
	adbyby_restore_fixture_t *fixture = (adbyby_restore_fixture_t *)context;
	int index = fixture->rollback_calls;

	(void)path;
	if (!rollback)
		return -1;
	if (index < 2)
		snprintf(fixture->rollback_order[index],
			 sizeof(fixture->rollback_order[index]), "%s", table);
	fixture->rollback_calls++;
	return strcmp(fixture->fail_rollback_table, table) == 0 ? -1 : 0;
}

static int
fake_remove_snapshot(const char *path, void *context)
{
	adbyby_restore_fixture_t *fixture = (adbyby_restore_fixture_t *)context;
	fixture->remove_attempt_calls++;
	if (strcmp(fixture->fail_remove_path, path) == 0)
		return -1;
	fixture->remove_calls++;
	return 0;
}

static void
fake_report_failure(const char *operation, int status, void *context)
{
	adbyby_restore_fixture_t *fixture = (adbyby_restore_fixture_t *)context;
	fixture->report_calls++;
	fixture->reported_status = status;
	snprintf(fixture->reported_operation, sizeof(fixture->reported_operation),
		 "%s", operation);
}

static int
apply_policy(int snapshot_exists, firewall_adbyby_restore_transaction_t *tx,
	     adbyby_restore_fixture_t *fixture)
{
	return firewall_adbyby_restore_policy_apply(snapshot_exists, tx,
		fake_blockip_preflight, fake_restore_snapshot, fake_snapshot_table,
		fake_restore_table, fake_remove_snapshot, fake_report_failure, fixture);
}

static int
test_missing_snapshot_is_noop(void)
{
	firewall_adbyby_restore_transaction_t tx = {0};
	adbyby_restore_fixture_t fixture = {0};

	if (apply_policy(0, &tx, &fixture) != FIREWALL_ADBYBY_RESTORE_OK)
		return 1;
	return fixture.preflight_calls == 0 && fixture.snapshot_calls == 0 &&
		fixture.restore_calls == 0 && fixture.rollback_calls == 0 &&
		fixture.remove_attempt_calls == 0 && fixture.report_calls == 0 ? 0 : 1;
}

static int
test_missing_ipset_skips_restore(void)
{
	firewall_adbyby_restore_transaction_t tx = {0};
	adbyby_restore_fixture_t fixture = {0};

	fixture.preflight_status = 256;
	if (apply_policy(1, &tx, &fixture) != FIREWALL_ADBYBY_RESTORE_FAILED)
		return 1;
	return fixture.preflight_calls == 1 && fixture.snapshot_calls == 0 &&
		fixture.restore_calls == 0 && fixture.report_calls == 1 &&
		strcmp(fixture.reported_operation, "Adbyby blockip ipset preflight") == 0
		? 0 : 1;
}

static int
test_snapshot_failure_cleans_staged_snapshot(void)
{
	firewall_adbyby_restore_transaction_t tx = {0};
	adbyby_restore_fixture_t fixture = {0};

	snprintf(fixture.fail_snapshot_table, sizeof(fixture.fail_snapshot_table), "nat");
	if (apply_policy(1, &tx, &fixture) != FIREWALL_ADBYBY_RESTORE_FAILED)
		return 1;
	return fixture.snapshot_calls == 2 && fixture.restore_calls == 0 &&
		fixture.rollback_calls == 0 && fixture.remove_attempt_calls == 2 &&
		fixture.remove_calls == 2 && tx.snapshot_count == 0 &&
		strcmp(fixture.reported_operation, "Adbyby iptables nat snapshot") == 0
		? 0 : 1;
}

static int
test_partial_extension_restore_rolls_back_tables(void)
{
	firewall_adbyby_restore_transaction_t tx = {0};
	adbyby_restore_fixture_t fixture = {0};

	fixture.restore_status = 256;
	if (apply_policy(1, &tx, &fixture) != FIREWALL_ADBYBY_RESTORE_FAILED)
		return 1;
	return fixture.snapshot_calls == 2 && fixture.restore_calls == 1 &&
		fixture.rollback_calls == 2 && fixture.remove_calls == 2 &&
		tx.snapshot_count == 0 && !tx.rollback_failed &&
		strcmp(fixture.rollback_order[0], "nat") == 0 &&
		strcmp(fixture.rollback_order[1], "filter") == 0 &&
		strcmp(fixture.reported_operation, "Adbyby iptables restore") == 0
		? 0 : 1;
}

static int
test_rollback_failure_retains_recovery_snapshots(void)
{
	firewall_adbyby_restore_transaction_t tx = {0};
	adbyby_restore_fixture_t fixture = {0};
	int snapshots_before;

	fixture.restore_status = 256;
	snprintf(fixture.fail_rollback_table, sizeof(fixture.fail_rollback_table), "nat");
	if (apply_policy(1, &tx, &fixture) !=
	    FIREWALL_ADBYBY_RESTORE_RECOVERY_FAILED)
		return 1;
	if (!tx.rollback_failed || tx.snapshot_count != 2 ||
	    fixture.rollback_calls != 2 || fixture.remove_attempt_calls != 0 ||
	    strcmp(tx.snapshots[0].table, "filter") != 0 ||
	    strcmp(tx.snapshots[1].table, "nat") != 0 ||
	    strcmp(fixture.rollback_order[0], "nat") != 0 ||
	    strcmp(fixture.rollback_order[1], "filter") != 0)
		return 1;

	snapshots_before = fixture.snapshot_calls;
	if (apply_policy(1, &tx, &fixture) !=
	    FIREWALL_ADBYBY_RESTORE_RECOVERY_FAILED)
		return 1;
	return fixture.snapshot_calls == snapshots_before &&
		fixture.restore_calls == 1 && tx.snapshot_count == 2 ? 0 : 1;
}

static int
test_cleanup_failure_retries_before_next_apply(void)
{
	firewall_adbyby_restore_transaction_t tx = {0};
	adbyby_restore_fixture_t fixture = {0};

	snprintf(fixture.fail_remove_path, sizeof(fixture.fail_remove_path),
		 "/tmp/adbyby-filter.snapshot");
	if (apply_policy(1, &tx, &fixture) != FIREWALL_ADBYBY_RESTORE_FAILED)
		return 1;
	if (tx.snapshot_count != 1 || !tx.cleanup_failed ||
	    strcmp(tx.snapshots[0].table, "filter") != 0 ||
	    fixture.restore_calls != 1)
		return 1;

	fixture.fail_remove_path[0] = '\0';
	if (apply_policy(1, &tx, &fixture) != FIREWALL_ADBYBY_RESTORE_OK)
		return 1;
	return tx.snapshot_count == 0 && !tx.cleanup_failed &&
		fixture.restore_calls == 2 && fixture.remove_attempt_calls == 5
		? 0 : 1;
}

static int
test_successful_restore(void)
{
	firewall_adbyby_restore_transaction_t tx = {0};
	adbyby_restore_fixture_t fixture = {0};

	if (apply_policy(1, &tx, &fixture) != FIREWALL_ADBYBY_RESTORE_OK)
		return 1;
	return fixture.preflight_calls == 1 && fixture.snapshot_calls == 2 &&
		fixture.restore_calls == 1 && fixture.rollback_calls == 0 &&
		fixture.remove_calls == 2 && fixture.report_calls == 0 &&
		tx.snapshot_count == 0 ? 0 : 1;
}

int
main(void)
{
	if (test_missing_snapshot_is_noop() != 0 ||
	    test_missing_ipset_skips_restore() != 0 ||
	    test_snapshot_failure_cleans_staged_snapshot() != 0 ||
	    test_partial_extension_restore_rolls_back_tables() != 0 ||
	    test_rollback_failure_retains_recovery_snapshots() != 0 ||
	    test_cleanup_failure_retries_before_next_apply() != 0 ||
	    test_successful_restore() != 0) {
		fprintf(stderr, "Adbyby transactional restore regression failed\n");
		return 1;
	}
	puts("Adbyby transactional restore regression: OK");
	return 0;
}
