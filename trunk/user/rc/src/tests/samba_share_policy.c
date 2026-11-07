#include <assert.h>
#include <stdio.h>
#include "../samba_share_policy.h"

int main(void)
{
	/* Public/guest SMB sessions must never inherit the management UID 0. */
	assert(strcmp(SAMBA_GUEST_ACCOUNT, "nobody") == 0);
	assert(strcmp(SAMBA_GUEST_ACCOUNT, "admin") != 0);

	assert(samba_conf_text_is_safe("Padavan AX-3000"));
	assert(samba_conf_text_is_safe("/media/USB Disk"));
	assert(samba_conf_text_is_safe(""));
	assert(!samba_conf_text_is_safe(NULL));
	assert(!samba_conf_text_is_safe("model\r\nwriteable = yes"));
	assert(!samba_conf_text_is_safe("folder\n[global]"));
	assert(!samba_conf_text_is_safe("continued\\\ninclude = /tmp/evil.conf"));
	assert(!samba_conf_text_is_safe("share%U"));
	assert(!samba_conf_text_is_safe("tab\tvalue"));
	assert(!samba_conf_text_is_safe("del\177value"));

	assert(samba_share_name_is_safe("USB Disk"));
	assert(samba_share_name_is_safe("家庭照片"));
	assert(samba_share_name_is_safe("alpha(beta)"));
	assert(!samba_share_name_is_safe(NULL));
	assert(!samba_share_name_is_safe(""));
	assert(!samba_share_name_is_safe("."));
	assert(!samba_share_name_is_safe(".."));
	assert(!samba_share_name_is_safe("../outside"));
	assert(!samba_share_name_is_safe("disk] [global"));
	assert(!samba_share_name_is_safe("bad;name"));
	assert(!samba_share_name_is_safe("bad#name"));
	assert(!samba_share_name_is_safe("bad\n[global]"));
	{
		char too_long[257];
		int i;
		for (i = 0; i < 256; ++i)
			too_long[i] = 'a';
		too_long[256] = '\0';
		assert(!samba_share_name_is_safe(too_long));
	}

	assert(samba_mount_path_is_safe("/media/sda1"));
	assert(samba_mount_path_is_safe("/media/USB Disk"));
	assert(!samba_mount_path_is_safe(NULL));
	assert(!samba_mount_path_is_safe("/etc"));
	assert(!samba_mount_path_is_safe("/media/../etc"));
	assert(!samba_mount_path_is_safe("/media/disk/../etc"));
	assert(!samba_mount_path_is_safe("/media//disk"));
	assert(!samba_mount_path_is_safe("/media/disk\\payload"));
	assert(!samba_mount_path_is_safe("/media/disk%U"));
	assert(!samba_mount_path_is_safe("/media/disk\npath"));

	assert(samba_acl_user_is_safe("admin"));
	assert(samba_acl_user_is_safe("_svc-1.2"));
	assert(samba_acl_user_is_safe("user_2"));
	assert(!samba_acl_user_is_safe(NULL));
	assert(!samba_acl_user_is_safe(""));
	assert(!samba_acl_user_is_safe("-option"));
	assert(!samba_acl_user_is_safe("."));
	assert(!samba_acl_user_is_safe("user,other"));
	assert(!samba_acl_user_is_safe("user name"));
	assert(!samba_acl_user_is_safe("user\nread only = no"));
	assert(!samba_acl_user_is_safe("user%U"));
	assert(!samba_acl_user_is_safe("用户"));

	puts("Samba share configuration token policy: PASS");
	return 0;
}
