#ifndef FIREWALL_ADBYBY_RESTORE_POLICY_H
#define FIREWALL_ADBYBY_RESTORE_POLICY_H

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define FIREWALL_ADBYBY_RESTORE_MAX_SNAPSHOTS 2
#define FIREWALL_ADBYBY_RESTORE_TABLE_SIZE 16
#define FIREWALL_ADBYBY_RESTORE_PATH_SIZE 256

#define FIREWALL_ADBYBY_RESTORE_OK 0
#define FIREWALL_ADBYBY_RESTORE_FAILED -1
#define FIREWALL_ADBYBY_RESTORE_RECOVERY_FAILED -2

typedef struct {
	char table[FIREWALL_ADBYBY_RESTORE_TABLE_SIZE];
	char path[FIREWALL_ADBYBY_RESTORE_PATH_SIZE];
} firewall_adbyby_restore_snapshot_t;

typedef struct {
	int rollback_failed;
	int cleanup_failed;
	size_t snapshot_count;
	firewall_adbyby_restore_snapshot_t snapshots[FIREWALL_ADBYBY_RESTORE_MAX_SNAPSHOTS];
} firewall_adbyby_restore_transaction_t;

typedef int (*firewall_adbyby_restore_command_fn)(void *context);
typedef int (*firewall_adbyby_restore_snapshot_fn)(const char *table,
						   char *path,
						   size_t path_size,
						   void *context);
typedef int (*firewall_adbyby_restore_table_fn)(const char *table,
						 const char *path,
						 int rollback,
						 void *context);
typedef int (*firewall_adbyby_restore_remove_fn)(const char *path, void *context);
typedef void (*firewall_adbyby_restore_report_fn)(const char *operation,
						   int status,
						   void *context);

static int
firewall_adbyby_restore_policy_path_is_safe(const char *path)
{
	size_t i;

	if (!path || strncmp(path, "/tmp/", 5) != 0 || path[5] == '\0')
		return 0;

	for (i = 5; path[i]; ++i) {
		unsigned char ch = (unsigned char)path[i];
		if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
		      (ch >= '0' && ch <= '9') || ch == '.' || ch == '_' || ch == '-'))
			return 0;
		if (ch == '.' && path[i + 1] == '.')
			return 0;
	}
	return 1;
}

/* Keep failed removals in the transaction so a later firewall rebuild retries. */
static int
firewall_adbyby_restore_policy_cleanup(
	firewall_adbyby_restore_transaction_t *tx,
	firewall_adbyby_restore_remove_fn remove_snapshot,
	void *context)
{
	size_t i, retained;
	int failed;

	if (!tx)
		return -1;
	if (tx->snapshot_count && !remove_snapshot) {
		tx->cleanup_failed = 1;
		return -1;
	}

	retained = 0;
	failed = 0;
	for (i = 0; i < tx->snapshot_count; ++i) {
		if (remove_snapshot(tx->snapshots[i].path, context) != 0) {
			if (retained != i)
				tx->snapshots[retained] = tx->snapshots[i];
			++retained;
			failed = 1;
		}
	}
	tx->snapshot_count = retained;
	tx->cleanup_failed = failed;
	return failed ? -1 : 0;
}

/*
 * A failed rollback is an operator-recovery condition: never discard those
 * snapshots or start another extension restore. Otherwise retry cleanup.
 */
static int
firewall_adbyby_restore_policy_prepare_reset(
	firewall_adbyby_restore_transaction_t *tx,
	firewall_adbyby_restore_remove_fn remove_snapshot,
	void *context)
{
	if (!tx || tx->rollback_failed)
		return -1;
	if (tx->snapshot_count &&
	    firewall_adbyby_restore_policy_cleanup(tx, remove_snapshot, context) != 0)
		return -1;
	if (tx->snapshot_count)
		return -1;

	memset(tx, 0, sizeof(*tx));
	return 0;
}

/*
 * The saved extension file contains Adbyby rules in the IPv4 filter and nat
 * tables. Snapshot both complete tables before running the one multi-table
 * iptables-restore -n command. If a later table fails after an earlier table
 * committed, restore both complete pre-change tables (without --noflush).
 */
static int
firewall_adbyby_restore_policy_apply(
	int snapshot_exists,
	firewall_adbyby_restore_transaction_t *tx,
	firewall_adbyby_restore_command_fn blockip_preflight,
	firewall_adbyby_restore_command_fn restore_snapshot,
	firewall_adbyby_restore_snapshot_fn snapshot_table,
	firewall_adbyby_restore_table_fn restore_table,
	firewall_adbyby_restore_remove_fn remove_snapshot,
	firewall_adbyby_restore_report_fn report_failure,
	void *context)
{
	static const char *tables[FIREWALL_ADBYBY_RESTORE_MAX_SNAPSHOTS] = {
		"filter", "nat"
	};
	char path[FIREWALL_ADBYBY_RESTORE_PATH_SIZE];
	char operation[80];
	size_t i, j;
	int status, duplicate;

	if (!tx)
		return FIREWALL_ADBYBY_RESTORE_FAILED;
	if (tx->rollback_failed)
		return FIREWALL_ADBYBY_RESTORE_RECOVERY_FAILED;
	if (firewall_adbyby_restore_policy_prepare_reset(tx, remove_snapshot,
							 context) != 0) {
		if (report_failure)
			report_failure("Adbyby firewall snapshot cleanup", -1, context);
		return FIREWALL_ADBYBY_RESTORE_FAILED;
	}
	if (!snapshot_exists)
		return FIREWALL_ADBYBY_RESTORE_OK;
	if (!blockip_preflight || !restore_snapshot || !snapshot_table ||
	    !restore_table || !remove_snapshot || !report_failure)
		return FIREWALL_ADBYBY_RESTORE_FAILED;

	status = blockip_preflight(context);
	if (status != 0) {
		report_failure("Adbyby blockip ipset preflight", status, context);
		return FIREWALL_ADBYBY_RESTORE_FAILED;
	}

	for (i = 0; i < FIREWALL_ADBYBY_RESTORE_MAX_SNAPSHOTS; ++i) {
		path[0] = '\0';
		status = snapshot_table(tables[i], path, sizeof(path), context);
		duplicate = 0;
		if (firewall_adbyby_restore_policy_path_is_safe(path)) {
			for (j = 0; j < tx->snapshot_count; ++j) {
				if (strcmp(tx->snapshots[j].path, path) == 0) {
					duplicate = 1;
					break;
				}
			}
			if (!duplicate &&
			    tx->snapshot_count < FIREWALL_ADBYBY_RESTORE_MAX_SNAPSHOTS) {
				strncpy(tx->snapshots[tx->snapshot_count].table, tables[i],
					sizeof(tx->snapshots[tx->snapshot_count].table) - 1);
				tx->snapshots[tx->snapshot_count].table[
					sizeof(tx->snapshots[tx->snapshot_count].table) - 1] = '\0';
				strncpy(tx->snapshots[tx->snapshot_count].path, path,
					sizeof(tx->snapshots[tx->snapshot_count].path) - 1);
				tx->snapshots[tx->snapshot_count].path[
					sizeof(tx->snapshots[tx->snapshot_count].path) - 1] = '\0';
				++tx->snapshot_count;
			}
		}

		if (status != 0 ||
		    !firewall_adbyby_restore_policy_path_is_safe(path) || duplicate) {
			snprintf(operation, sizeof(operation),
				 "Adbyby iptables %s snapshot", tables[i]);
			report_failure(operation, status ? status : -1, context);
			if (firewall_adbyby_restore_policy_cleanup(tx, remove_snapshot,
								   context) != 0)
				report_failure("Adbyby firewall snapshot cleanup", -1, context);
			return FIREWALL_ADBYBY_RESTORE_FAILED;
		}
	}

	status = restore_snapshot(context);
	if (status == 0) {
		if (firewall_adbyby_restore_policy_cleanup(tx, remove_snapshot,
							   context) != 0) {
			report_failure("Adbyby firewall snapshot cleanup", -1, context);
			return FIREWALL_ADBYBY_RESTORE_FAILED;
		}
		return FIREWALL_ADBYBY_RESTORE_OK;
	}

	report_failure("Adbyby iptables restore", status, context);

	/* Continue after an individual failure so every affected table is tried. */
	for (i = tx->snapshot_count; i > 0; --i) {
		firewall_adbyby_restore_snapshot_t *snapshot = &tx->snapshots[i - 1];
		status = restore_table(snapshot->table, snapshot->path, 1, context);
		if (status != 0) {
			tx->rollback_failed = 1;
			snprintf(operation, sizeof(operation),
				 "Adbyby iptables %s rollback", snapshot->table);
			report_failure(operation, status, context);
		}
	}

	if (tx->rollback_failed)
		return FIREWALL_ADBYBY_RESTORE_RECOVERY_FAILED;

	if (firewall_adbyby_restore_policy_cleanup(tx, remove_snapshot, context) != 0)
		report_failure("Adbyby firewall snapshot cleanup", -1, context);
	return FIREWALL_ADBYBY_RESTORE_FAILED;
}

#endif
