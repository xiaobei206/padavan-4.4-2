#ifndef FIREWALL_ADBYBY_RESTORE_POLICY_H
#define FIREWALL_ADBYBY_RESTORE_POLICY_H

/*
 * Adbyby's saved rules reference the blockip ipset. Do not replay that
 * snapshot without checking the prerequisite first. Keep command execution
 * and logging in the caller so this policy can be tested without a router.
 */
typedef int (*firewall_adbyby_restore_command_fn)(void *context);
typedef void (*firewall_adbyby_restore_report_fn)(const char *operation,
						  int status,
						  void *context);

/*
 * A missing snapshot is a no-op. Nonzero command status means the prerequisite
 * or restore failed; report the failing operation and do not continue.
 */
static int
firewall_adbyby_restore_policy_apply(
	int snapshot_exists,
	firewall_adbyby_restore_command_fn blockip_preflight,
	firewall_adbyby_restore_command_fn restore_snapshot,
	firewall_adbyby_restore_report_fn report_failure,
	void *context)
{
	int status;

	if (!snapshot_exists)
		return 0;
	if (!blockip_preflight || !restore_snapshot || !report_failure)
		return -1;

	status = blockip_preflight(context);
	if (status != 0) {
		report_failure("Adbyby blockip ipset preflight", status, context);
		return -1;
	}

	status = restore_snapshot(context);
	if (status != 0) {
		report_failure("Adbyby iptables restore", status, context);
		return -1;
	}
	return 0;
}

#endif
