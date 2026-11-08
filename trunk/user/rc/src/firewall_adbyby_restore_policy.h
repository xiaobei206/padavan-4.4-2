#ifndef FIREWALL_ADBYBY_RESTORE_POLICY_H
#define FIREWALL_ADBYBY_RESTORE_POLICY_H

/*
 * Adbyby's saved rules reference the blockip ipset. Refuse to replay a
 * snapshot unless both prerequisites are present; the caller remains
 * responsible for logging and preserving the core firewall transaction.
 */
static int
firewall_adbyby_restore_is_allowed(int snapshot_exists, int blockip_set_exists)
{
	return snapshot_exists && blockip_set_exists;
}

#endif
