#ifndef PADAVAN_FIREWALL_RESTORE_TRANSACTION_H
#define PADAVAN_FIREWALL_RESTORE_TRANSACTION_H
#include <stddef.h>
#include <string.h>

#define FIREWALL_RESTORE_TX_MAX_SNAPSHOTS 8
#define FIREWALL_RESTORE_TX_FAMILY_SIZE 12
#define FIREWALL_RESTORE_TX_TABLE_SIZE 16
#define FIREWALL_RESTORE_TX_PATH_SIZE 256

typedef struct {
	char family[FIREWALL_RESTORE_TX_FAMILY_SIZE];
	char table[FIREWALL_RESTORE_TX_TABLE_SIZE];
	char path[FIREWALL_RESTORE_TX_PATH_SIZE];
} firewall_restore_snapshot_t;

typedef struct {
	int failed;
	int rollback_failed;
	int rollback_attempted;
	int cleanup_failed;
	size_t snapshot_count;
	firewall_restore_snapshot_t snapshots[FIREWALL_RESTORE_TX_MAX_SNAPSHOTS];
} firewall_restore_transaction_t;

typedef int (*firewall_restore_snapshot_fn)(const char *, const char *, char *, size_t, void *);
typedef int (*firewall_restore_command_fn)(const char *, const char *, const char *, int, void *);
typedef int (*firewall_restore_remove_fn)(const char *, void *);

static int
firewall_restore_transaction_family_is_safe(const char *family)
{
	return family &&
	       (strcmp(family, "iptables") == 0 || strcmp(family, "ip6tables") == 0);
}

static int
firewall_restore_transaction_table_is_safe(const char *table)
{
	return table && (strcmp(table, "filter") == 0 ||
			 strcmp(table, "nat") == 0 ||
			 strcmp(table, "mangle") == 0 ||
			 strcmp(table, "raw") == 0);
}

/* Shell-safe paths must be direct children of /tmp. */
static int
firewall_restore_transaction_path_is_safe(const char *path)
{
	size_t i;

	if (!path || path[0] != '/' || path[1] != 't' ||
	    path[2] != 'm' || path[3] != 'p' || path[4] != '/' ||
	    path[5] == '\0')
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

static int
firewall_restore_transaction_find(const firewall_restore_transaction_t *tx,
				  const char *family, const char *table)
{
	size_t i;
	if (!tx || !family || !table)
		return -1;
	for (i = 0; i < tx->snapshot_count; ++i)
		if (strcmp(tx->snapshots[i].family, family) == 0 &&
		    strcmp(tx->snapshots[i].table, table) == 0)
			return (int)i;
	return -1;
}

static void
firewall_restore_transaction_reset(firewall_restore_transaction_t *tx)
{
	if (tx)
		memset(tx, 0, sizeof(*tx));
}

static int
firewall_restore_transaction_ok(const firewall_restore_transaction_t *tx)
{
	return tx && !tx->failed;
}

static int
firewall_restore_transaction_rollback_failed(const firewall_restore_transaction_t *tx)
{
	return tx && tx->rollback_failed;
}

/*
 * Roll back every known table in reverse application order. If any rollback
 * fails, retain all original snapshots so the mixed state remains recoverable.
 */
/*
 * Remove completed snapshots without forgetting paths that could not be
 * unlinked. Failed rollback snapshots never enter this cleanup path.
 */
static int
firewall_restore_transaction_cleanup_snapshots(firewall_restore_transaction_t *tx,
						firewall_restore_remove_fn remove_fn,
						void *context)
{
	size_t i, retained;
	int failed;

	if (!tx)
		return -1;
	if (tx->snapshot_count && !remove_fn) {
		tx->cleanup_failed = 1;
		return -1;
	}
	retained = 0;
	failed = 0;
	for (i = 0; i < tx->snapshot_count; ++i) {
		if (remove_fn(tx->snapshots[i].path, context) != 0) {
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

static int
firewall_restore_transaction_abort(firewall_restore_transaction_t *tx,
				   firewall_restore_command_fn command_fn,
				   firewall_restore_remove_fn remove_fn,
				   void *context)
{
	size_t i;

	if (!tx)
		return -1;
	tx->failed = 1;
	if (tx->rollback_attempted)
		return tx->rollback_failed ? -1 :
			firewall_restore_transaction_cleanup_snapshots(tx, remove_fn, context);

	tx->rollback_attempted = 1;
	if (tx->snapshot_count && (!command_fn || !remove_fn))
		tx->rollback_failed = 1;

	if (command_fn) {
		for (i = tx->snapshot_count; i > 0; --i) {
			firewall_restore_snapshot_t *s = &tx->snapshots[i - 1];
			if (command_fn(s->family, s->table, s->path, 1, context) != 0)
				tx->rollback_failed = 1;
		}
	} else if (tx->snapshot_count) {
		tx->rollback_failed = 1;
	}

	if (tx->rollback_failed)
		return -1;

	return firewall_restore_transaction_cleanup_snapshots(tx, remove_fn, context);
}

/*
 * Save each family/table once before its first mutation. A failed restore also
 * rolls back that same table because restore may partially apply before exit.
 */
static int
firewall_restore_transaction_apply(firewall_restore_transaction_t *tx,
				   const char *family, const char *table,
				   const char *rule_path,
				   firewall_restore_snapshot_fn snapshot_fn,
				   firewall_restore_command_fn command_fn,
				   firewall_restore_remove_fn remove_fn,
				   void *context)
{
	int index;
	char snapshot_path[FIREWALL_RESTORE_TX_PATH_SIZE];
	size_t i;

	if (!tx)
		return -1;
	if (tx->failed)
		return -1;
	if (!firewall_restore_transaction_family_is_safe(family) ||
	    !firewall_restore_transaction_table_is_safe(table) ||
	    !firewall_restore_transaction_path_is_safe(rule_path) ||
	    !snapshot_fn || !command_fn || !remove_fn) {
		(void)firewall_restore_transaction_abort(tx, command_fn, remove_fn, context);
		return -1;
	}

	index = firewall_restore_transaction_find(tx, family, table);
	if (index < 0) {
		if (tx->snapshot_count >= FIREWALL_RESTORE_TX_MAX_SNAPSHOTS) {
			(void)firewall_restore_transaction_abort(tx, command_fn, remove_fn, context);
			return -1;
		}
		snapshot_path[0] = '\0';
		if (snapshot_fn(family, table, snapshot_path, sizeof(snapshot_path), context) != 0 ||
		    !firewall_restore_transaction_path_is_safe(snapshot_path)) {
			if (firewall_restore_transaction_path_is_safe(snapshot_path)) {
				for (i = 0; i < tx->snapshot_count; ++i)
					if (strcmp(tx->snapshots[i].path, snapshot_path) == 0)
						break;
				if (i == tx->snapshot_count)
					remove_fn(snapshot_path, context);
			}
			(void)firewall_restore_transaction_abort(tx, command_fn, remove_fn, context);
			return -1;
		}
		for (i = 0; i < tx->snapshot_count; ++i) {
			if (strcmp(tx->snapshots[i].path, snapshot_path) == 0) {
				(void)firewall_restore_transaction_abort(tx, command_fn, remove_fn, context);
				return -1;
			}
		}

		index = (int)tx->snapshot_count;
		strncpy(tx->snapshots[index].family, family, sizeof(tx->snapshots[index].family) - 1);
		tx->snapshots[index].family[sizeof(tx->snapshots[index].family) - 1] = '\0';
		strncpy(tx->snapshots[index].table, table, sizeof(tx->snapshots[index].table) - 1);
		tx->snapshots[index].table[sizeof(tx->snapshots[index].table) - 1] = '\0';
		strncpy(tx->snapshots[index].path, snapshot_path, sizeof(tx->snapshots[index].path) - 1);
		tx->snapshots[index].path[sizeof(tx->snapshots[index].path) - 1] = '\0';
		++tx->snapshot_count;
	}

	if (command_fn(family, table, rule_path, 0, context) != 0) {
		(void)firewall_restore_transaction_abort(tx, command_fn, remove_fn, context);
		return -1;
	}
	return 0;
}

/* A successful transaction keeps the new rules and removes its snapshots. */
static int
firewall_restore_transaction_finish(firewall_restore_transaction_t *tx,
				    firewall_restore_remove_fn remove_fn,
				    void *context)
{
	if (!tx || tx->failed)
		return -1;
	return firewall_restore_transaction_cleanup_snapshots(tx, remove_fn, context);
}
#endif
