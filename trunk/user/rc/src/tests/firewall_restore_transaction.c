#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../firewall_restore_transaction.h"

#define CALL_LIMIT 64
#define CALL_SIZE 160
typedef struct {
	char calls[CALL_LIMIT][CALL_SIZE];
	int call_count, snapshot_count, apply_count, rollback_count, remove_count, path_sequence;
	char fail_snapshot_family[16], fail_snapshot_table[16];
	char fail_apply_family[16], fail_apply_table[16];
	char fail_rollback_family[16], fail_rollback_table[16];
} test_context_t;

static void
record_call(test_context_t *t, const char *action, const char *family,
	    const char *table, const char *path)
{
	int n;
	assert(t->call_count < CALL_LIMIT);
	n = snprintf(t->calls[t->call_count], sizeof(t->calls[t->call_count]),
		     "%s:%s:%s:%s", action, family, table, path ? path : "");
	assert(n > 0 && (size_t)n < sizeof(t->calls[t->call_count]));
	++t->call_count;
}

static int
test_snapshot(const char *family, const char *table, char *path,
	      size_t path_size, void *context)
{
	test_context_t *t = (test_context_t *)context;
	int n;
	if (strcmp(t->fail_snapshot_family, family) == 0 &&
	    strcmp(t->fail_snapshot_table, table) == 0) {
		record_call(t, "snapshot-fail", family, table, "");
		return -1;
	}
	n = snprintf(path, path_size, "/tmp/snapshot-%d", t->path_sequence++);
	assert(n > 0 && (size_t)n < path_size);
	record_call(t, "snapshot", family, table, path);
	++t->snapshot_count;
	return 0;
}

static int
test_command(const char *family, const char *table, const char *path,
	     int rollback, void *context)
{
	test_context_t *t = (test_context_t *)context;
	record_call(t, rollback ? "rollback" : "apply", family, table, path);
	if (rollback) {
		++t->rollback_count;
		if (strcmp(t->fail_rollback_family, family) == 0 &&
		    strcmp(t->fail_rollback_table, table) == 0)
			return -1;
	} else {
		++t->apply_count;
		if (strcmp(t->fail_apply_family, family) == 0 &&
		    strcmp(t->fail_apply_table, table) == 0)
			return -1;
	}
	return 0;
}

static void
test_remove(const char *path, void *context)
{
	test_context_t *t = (test_context_t *)context;
	assert(firewall_restore_transaction_path_is_safe(path));
	record_call(t, "remove", "", "", path);
	++t->remove_count;
}

static void
test_success(void)
{
	firewall_restore_transaction_t tx;
	test_context_t t = {0};
	firewall_restore_transaction_reset(&tx);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "filter", "/tmp/filter.rules",
		test_snapshot, test_command, test_remove, &t) == 0);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "filter", "/tmp/filter2.rules",
		test_snapshot, test_command, test_remove, &t) == 0);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "nat", "/tmp/nat.rules",
		test_snapshot, test_command, test_remove, &t) == 0);
	assert(tx.snapshot_count == 2 && t.snapshot_count == 2 && t.apply_count == 3);
	assert(t.rollback_count == 0);
	assert(firewall_restore_transaction_finish(&tx, test_remove, &t) == 0);
	assert(firewall_restore_transaction_ok(&tx) && tx.snapshot_count == 0);
	assert(t.remove_count == 2);
}

static void
test_apply_failure_reverse_rollback(void)
{
	firewall_restore_transaction_t tx;
	test_context_t t = {0};
	strcpy(t.fail_apply_family, "iptables");
	strcpy(t.fail_apply_table, "nat");
	firewall_restore_transaction_reset(&tx);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "filter", "/tmp/filter.rules",
		test_snapshot, test_command, test_remove, &t) == 0);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "nat", "/tmp/nat.rules",
		test_snapshot, test_command, test_remove, &t) == -1);
	assert(!firewall_restore_transaction_ok(&tx));
	assert(!firewall_restore_transaction_rollback_failed(&tx));
	assert(t.rollback_count == 2 && t.remove_count == 2 && tx.snapshot_count == 0);
	assert(strcmp(t.calls[4], "rollback:iptables:nat:/tmp/snapshot-1") == 0);
	assert(strcmp(t.calls[5], "rollback:iptables:filter:/tmp/snapshot-0") == 0);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "raw", "/tmp/raw.rules",
		test_snapshot, test_command, test_remove, &t) == -1);
	assert(t.apply_count == 2);
	assert(firewall_restore_transaction_finish(&tx, test_remove, &t) == -1);
}

static void
test_snapshot_failure_rolls_back_prior_tables(void)
{
	firewall_restore_transaction_t tx;
	test_context_t t = {0};
	strcpy(t.fail_snapshot_family, "ip6tables");
	strcpy(t.fail_snapshot_table, "nat");
	firewall_restore_transaction_reset(&tx);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "filter", "/tmp/filter.rules",
		test_snapshot, test_command, test_remove, &t) == 0);
	assert(firewall_restore_transaction_apply(&tx, "ip6tables", "nat", "/tmp/ip6-nat.rules",
		test_snapshot, test_command, test_remove, &t) == -1);
	assert(t.apply_count == 1 && t.rollback_count == 1 && t.remove_count == 1);
	assert(strcmp(t.calls[3], "rollback:iptables:filter:/tmp/snapshot-0") == 0);
	assert(tx.snapshot_count == 0);
}

static void
test_rollback_failure_retains_snapshots(void)
{
	firewall_restore_transaction_t tx;
	test_context_t t = {0};
	strcpy(t.fail_apply_family, "iptables");
	strcpy(t.fail_apply_table, "nat");
	strcpy(t.fail_rollback_family, "iptables");
	strcpy(t.fail_rollback_table, "nat");
	firewall_restore_transaction_reset(&tx);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "filter", "/tmp/filter.rules",
		test_snapshot, test_command, test_remove, &t) == 0);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "nat", "/tmp/nat.rules",
		test_snapshot, test_command, test_remove, &t) == -1);
	assert(firewall_restore_transaction_rollback_failed(&tx));
	assert(t.rollback_count == 2 && t.remove_count == 0 && tx.snapshot_count == 2);
	assert(strcmp(t.calls[4], "rollback:iptables:nat:/tmp/snapshot-1") == 0);
	assert(strcmp(t.calls[5], "rollback:iptables:filter:/tmp/snapshot-0") == 0);
	assert(strcmp(tx.snapshots[0].path, "/tmp/snapshot-0") == 0);
	assert(firewall_restore_transaction_finish(&tx, test_remove, &t) == -1);
	assert(t.remove_count == 0);
}

static void
test_abort_and_invalid_tokens(void)
{
	firewall_restore_transaction_t tx;
	test_context_t t = {0};
	firewall_restore_transaction_reset(&tx);
	assert(firewall_restore_transaction_apply(&tx, "iptables", "raw", "/tmp/raw.rules",
		test_snapshot, test_command, test_remove, &t) == 0);
	assert(firewall_restore_transaction_abort(&tx, test_command, test_remove, &t) == 0);
	assert(!firewall_restore_transaction_ok(&tx) && t.rollback_count == 1 && t.remove_count == 1);

	firewall_restore_transaction_reset(&tx);
	memset(&t, 0, sizeof(t));
	assert(firewall_restore_transaction_apply(&tx, "iptables;touch", "filter", "/tmp/filter.rules",
		test_snapshot, test_command, test_remove, &t) == -1);
	assert(!firewall_restore_transaction_ok(&tx));
	assert(t.snapshot_count == 0 && t.apply_count == 0 && t.rollback_count == 0 && t.remove_count == 0);
	assert(firewall_restore_transaction_path_is_safe("/tmp/ipt_filter.ABC123"));
	assert(!firewall_restore_transaction_path_is_safe("/tmp/filter.rules;touch"));
	assert(!firewall_restore_transaction_path_is_safe("/tmp/../outside"));
	assert(!firewall_restore_transaction_path_is_safe("/var/tmp/filter.rules"));
}

int
main(void)
{
	test_success();
	test_apply_failure_reverse_rollback();
	test_snapshot_failure_rolls_back_prior_tables();
	test_rollback_failure_retains_snapshots();
	test_abort_and_invalid_tokens();
	return 0;
}
