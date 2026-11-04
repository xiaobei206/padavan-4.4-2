/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <stdlib.h>
#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <sys/time.h>
#include <sys/sysinfo.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <syslog.h>
#include <stdarg.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <fcntl.h>

#include <ralink_priv.h>
#include <flash_mtd.h>

#include "rc.h"

#define XSTR(s) STR(s)
#define STR(s) #s

long
uptime(void)
{
	struct sysinfo info;
	sysinfo(&info);

	return info.uptime;
}

int
rand_seed_by_time(void)
{
	time_t atime;

	/* make a random number and set the top and bottom bits */
	time(&atime);
	srand((unsigned long)atime);

	return rand();
}

/* convert mac address format from XXXXXXXXXXXX to XX:XX:XX:XX:XX:XX */
char *
mac_conv(const char *mac_nvkey, int idx, char *buf)
{
	char *mac, name[32];
	int i, j;

	if (idx < 0)
		snprintf(name, sizeof(name), "%s", mac_nvkey);
	else
		snprintf(name, sizeof(name), "%s%d", mac_nvkey, idx);

	mac = nvram_safe_get(name);

	buf[0] = 0;
	if (strlen(mac) == 12) {
		for (i = 0, j = 0; i < 12; i++) {
			if (i != 0 && (i%2) == 0)
				buf[j++] = ':';
			buf[j++] = mac[i];
		}
		buf[j] = 0;	// oleg patch
	}

	if (strcasecmp(buf, "FF:FF:FF:FF:FF:FF") == 0 || strcmp(buf, "00:00:00:00:00:00") == 0)
		buf[0] = 0;

	return (buf);
}

/* convert mac address format from XX:XX:XX:XX:XX:XX to XXXXXXXXXXXX */
char *
mac_conv2(const char *mac_nvkey, int idx, char *buf)
{
	char *mac, name[32];
	int i, j;

	if (idx < 0)
		snprintf(name, sizeof(name), "%s", mac_nvkey);
	else
		snprintf(name, sizeof(name), "%s%d", mac_nvkey, idx);

	mac = nvram_safe_get(name);

	buf[0] = 0;
	if (strlen(mac) == 17 && strcasecmp(buf, "FF:FF:FF:FF:FF:FF") && strcmp(buf, "00:00:00:00:00:00")) {
		for(i = 0, j = 0; i < 17; ++i) {
			if (i%3 != 2){
				buf[j] = mac[i];
				++j;
			}
			buf[j] = 0;
		}
	}

	return (buf);
}

int
valid_subver(char subfs)
{
	printf("validate subfs: %c\n", subfs);	// tmp test
	if(((subfs >= 'a') && (subfs <= 'z' )) || ((subfs >= 'A') && (subfs <= 'Z' )))
		return 1;
	else
		return 0;
}

void
get_eeprom_params(void)
{
#if defined (VENDOR_ASUS)
	int i;
#endif
	int i_offset, i_ret;
	unsigned char buffer[32];
	unsigned char ea[ETHER_ADDR_LEN];
	char macaddr_wl[]  = "00:11:22:33:44:55";
	char macaddr_rt[]  = "00:11:22:33:44:56";
	char macaddr_lan[] = "00:11:22:33:44:55";
	char macaddr_wan[] = "00:11:22:33:44:56";
	char country_code[4];
	char regspec_code[8];
	char wps_pin[12];
	char productid[16];
	char fwver[16], fwver_sub[32];

#if (BOARD_5G_IN_SOC || !BOARD_HAS_5G_RADIO)
	i_offset = OFFSET_MAC_ADDR_WSOC;
#else
	i_offset = OFFSET_MAC_ADDR_INIC;
#endif
	memset(buffer, 0xff, ETHER_ADDR_LEN);
	i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, i_offset, buffer, ETHER_ADDR_LEN);
	if (i_ret >= 0 && !(buffer[0] & 0x01))
		ether_etoa(buffer, macaddr_wl);

#if BOARD_2G_AS_WSOC
	i_offset = OFFSET_MAC_ADDR_WSOC;
#else
	i_offset = OFFSET_MAC_ADDR_INIC;
#endif
	memset(buffer, 0xff, ETHER_ADDR_LEN);
	i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, i_offset, buffer, ETHER_ADDR_LEN);
	if (i_ret >= 0 && !(buffer[0] & 0x01))
		ether_etoa(buffer, macaddr_rt);

	i_offset = get_wired_mac_e2p_offset(0);
	memset(buffer, 0xff, ETHER_ADDR_LEN);
	i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, i_offset, buffer, ETHER_ADDR_LEN);
	if (buffer[0] & 0x01) {
		if (ether_atoe(macaddr_wl, ea)) {
			memcpy(buffer, ea, ETHER_ADDR_LEN);
			strcpy(macaddr_lan, macaddr_wl);
			if (i_ret >= 0)
				flash_mtd_write(MTD_PART_NAME_FACTORY, i_offset, ea, ETHER_ADDR_LEN);
		}
	} else {
		ether_etoa(buffer, macaddr_lan);
	}

	/* store wired LAN MAC */
	memcpy(buffer+ETHER_ADDR_LEN, buffer, ETHER_ADDR_LEN);

	if (get_wired_mac_is_single()) {
		buffer[5] |= 0x03;	// last 2 bits reserved for MBSSID, use 0x03 for WAN (ra1: 0x01, apcli0: 0x02)
		ether_etoa(buffer, macaddr_wan);
	} else {
		i_offset = get_wired_mac_e2p_offset(1);
		memset(buffer, 0xff, ETHER_ADDR_LEN);
		i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, i_offset, buffer, ETHER_ADDR_LEN);
		if ((buffer[0] & 0x01)
#if defined (USE_SINGLE_MAC)
		    /* compare LAN/WAN MAC OID */
		    || memcmp(buffer, buffer+ETHER_ADDR_LEN, 3) != 0
#endif
		    ) {
			memcpy(buffer, buffer+ETHER_ADDR_LEN, ETHER_ADDR_LEN);
			buffer[5] |= 0x03;
			ether_etoa(buffer, macaddr_wan);
#if !defined (USE_SINGLE_MAC)
			if (i_ret >= 0)
				flash_mtd_write(MTD_PART_NAME_FACTORY, i_offset, buffer, ETHER_ADDR_LEN);
#endif
		} else {
			ether_etoa(buffer, macaddr_wan);
		}
	}

	nvram_set_temp("il0macaddr", macaddr_lan); // LAN
	nvram_set_temp("il1macaddr", macaddr_wan); // WAN
	nvram_set_temp("wl_macaddr", macaddr_wl);  // 5GHz
	nvram_set_temp("rt_macaddr", macaddr_rt);  // 2.4GHz

#if defined (VENDOR_ASUS)
	/* reserved for Ralink. used as ASUS country code. */
	memset(country_code, 0, sizeof(country_code));
	i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, OFFSET_COUNTRY_CODE, country_code, 2);
	if (i_ret < 0) {
		strcpy(country_code, "GB");
	} else {
		country_code[2] = 0;
		for (i = 1; i >= 0; i--) {
			if ((unsigned char)country_code[i] > 0x7f)
				country_code[i] = 0;
		}
		if (country_code[0] == 0)
			strcpy(country_code, "GB");
	}
#else
	strcpy(country_code, "GB");
#endif

	if (strlen(nvram_wlan_get(0, "country_code")) == 0)
		nvram_wlan_set(0, "country_code", country_code);

	if (strlen(nvram_wlan_get(1, "country_code")) == 0)
		nvram_wlan_set(1, "country_code", country_code);

#if defined (VENDOR_ASUS)
	/* reserved for Ralink. used as ASUS RegSpec code. */
	memset(regspec_code, 0, sizeof(regspec_code));
	i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, OFFSET_REGSPEC_CODE, regspec_code, 4);
	if (i_ret < 0) {
		strcpy(regspec_code, "CE");
	} else {
		regspec_code[4] = 0;
		for (i = 3; i >= 0; i--) {
			if ((unsigned char)regspec_code[i] > 0x7f)
				regspec_code[i] = 0;
		}
		if (!check_regspec_code(regspec_code))
			strcpy(regspec_code, "CE");
	}
#else
	strcpy(regspec_code, "CE");
#endif
	nvram_set_temp("regspec_code", regspec_code);

#if defined (VENDOR_ASUS)
	/* reserved for Ralink. used as ASUS pin code. */
	memset(wps_pin, 0, sizeof(wps_pin));
	i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, OFFSET_PIN_CODE, wps_pin, 8);
	if (i_ret < 0) {
		strcpy(wps_pin, "12345670");
	} else {
		wps_pin[8] = 0;
		for (i = 7; i >= 0; i--) {
			if ((unsigned char)wps_pin[i] < 0x30 ||
			    (unsigned char)wps_pin[i] > 0x39)
				wps_pin[i] = 0;
		}
		if (wps_pin[0] == 0)
			strcpy(wps_pin, "12345670");
	}
#else
	strcpy(wps_pin, "12345670");
#endif
	nvram_set_temp("secret_code", wps_pin);

#if defined(USE_RT3352_MII)
 #define EEPROM_INIC_SIZE (512)
 #define EEPROM_INIC_ADDR 0x8000
	{
		char eeprom[EEPROM_INIC_SIZE];
		i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, EEPROM_INIC_ADDR, eeprom, sizeof(eeprom));
		if (i_ret < 0) {
			dbg("READ iNIC EEPROM: Out of scope!\n");
		} else {
			FILE *fp;
			if((fp = fopen("/etc/Wireless/iNIC/iNIC_e2p.bin", "w"))) {
				fwrite(eeprom, sizeof(eeprom), 1, fp);
				fclose(fp);
			}
		}
	}
#endif

	/* read firmware header */
	strcpy(fwver, "3.0.0.0");
	strcpy(fwver_sub, fwver);
	snprintf(productid, sizeof(productid), "%s", BOARD_PID);
	memset(buffer, 0, sizeof(buffer));
	i_ret = flash_mtd_read(MTD_PART_NAME_KERNEL, 0x20, buffer, 32);
	if (i_ret < 0) {
		nvram_set_temp("productid", "unknown");
		nvram_set_temp("firmver", "unknown");
	} else {
		strncpy(productid, buffer + 4, 12);
		productid[12] = 0;
		
		if(valid_subver(buffer[27]))
			sprintf(fwver_sub, "%d.%d.%d.%d%c", buffer[0], buffer[1], buffer[2], buffer[3], buffer[27]);
		else
			sprintf(fwver_sub, "%d.%d.%d.%d", buffer[0], buffer[1], buffer[2], buffer[3]);
		
		sprintf(fwver, "%d.%d.%d.%d", buffer[0], buffer[1], buffer[2], buffer[3]);
	}

#if defined(FWBLDSTR)
	if (strlen(FWBLDSTR) > 0 && strlen(FWBLDSTR) <= 4) {
		strcat(fwver_sub, "-");
		strcat(fwver_sub, FWBLDSTR);
	}
#endif
#if defined(FWREVSTR)
	if (strlen(FWREVSTR) > 0 && strlen(FWREVSTR) <= 8) {
		strcat(fwver_sub, "_");
		strcat(fwver_sub, FWREVSTR);
	}
#endif
	nvram_set_temp("productid", trim_r(productid));
	nvram_set_temp("firmver", trim_r(fwver));
	nvram_set_temp("firmver_sub", trim_r(fwver_sub));

#if 0
#if defined (VENDOR_ASUS)
	memset(buffer, 0, 4);
	i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, OFFSET_BOOT_VER, buffer, 4);
	if (i_ret == 0) {
		char blver[32];
		snprintf(blver, sizeof(blver), "%s-0%c-0%c-0%c-0%c", trim_r(productid), buffer[0], buffer[1], buffer[2], buffer[3]);
		nvram_set_temp("blver", trim_r(blver));
	}
#endif
#endif

	// TXBF, not used yet
	{
		int i, count_0xff = 0;
		unsigned char txbf_para[33];
		memset(txbf_para, 0, sizeof(txbf_para));
		i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, OFFSET_TXBF_PARA, txbf_para, 33);
		if (i_ret < 0) {
			dbg("READ TXBF PARA address: Out of scope\n");
		} else {
			for (i = 0; i < 33; i++) {
				if (txbf_para[i] == 0xff)
					count_0xff++;
			}
		}
		nvram_wlan_set_int(1, "txbf_en", (count_0xff == 33) ? 0 : 1);
		if(count_0xff !=33)
		{
		 count_0xff = 0;
		i_ret = flash_mtd_read(MTD_PART_NAME_FACTORY, 0x81A0, txbf_para, 33);
		if (i_ret < 0) {
			dbg("READ TXBF PARA address: Out of scope\n");
		} else {
			for (i = 0; i < 33; i++) {
				if (txbf_para[i] == 0xff)
					count_0xff++;
			}
		}
		nvram_wlan_set_int(1, "txbf_en", (count_0xff == 33) ? 0 : 1);
		}
	}
}

void
update_router_mode(void)
{
	if (nvram_get_int("sw_mode") != 3) {
		if (nvram_get_int("wan_nat_x") == 0)
			nvram_set_int("sw_mode", 4);	// Gateway mode
		else
			nvram_set_int("sw_mode", 1);	// Router mode
	}
}

void
set_pagecache_reclaim(void)
{
	int pagecache_ratio = 100;
	int pagecache_reclaim = nvram_get_int("pcache_reclaim");

	switch (pagecache_reclaim)
	{
	case 1:
		pagecache_ratio = 30;
		break;
	case 2:
		pagecache_ratio = 50;
		break;
	case 3:
		pagecache_ratio = 70;
		break;
	case 4:
		pagecache_ratio = 85;
		break;
	}

	fput_int("/proc/sys/vm/pagecache_ratio", pagecache_ratio);
}

void
restart_all_sysctl(void)
{
	if (!get_ap_mode()) {
		set_nf_conntrack();
		set_tcp_syncookies();
		set_igmp_mld_version();
		set_passthrough_pppoe(1);
	}

#if defined(APP_SMBD)
	config_smb_fastpath(1);
#endif
}

void
char_to_ascii(char *output, char *input)
{
	int i;
	char tmp[10];
	char *ptr;

	ptr = output;
	for (i = 0; i < strlen(input); i++) {
		if ((input[i] >= '0' && input[i] <= '9')
			||(input[i] >= 'A' && input[i] <= 'Z')
			||(input[i] >= 'a' && input[i] <= 'z')
			|| input[i] == '!' || input[i] == '*'
			|| input[i] == '(' || input[i] == ')'
			|| input[i] == '_' || input[i] == '-'
			|| input[i] == '\'' || input[i] == '.') {
			*ptr = input[i];
			ptr ++;
		} else {
			sprintf(tmp, "%%%.02X", input[i]);
			strcpy(ptr, tmp);
			ptr += 3;
		}
	}
	*(ptr) = '\0';
}

int
fput_string(const char *name, const char *value)
{
	FILE *fp;

	fp = fopen(name, "w");
	if (fp) {
		fputs(value, fp);
		fclose(fp);
		return 0;
	} else {
		return errno;
	}
}

int
fput_int(const char *name, int value)
{
	char svalue[32];
	sprintf(svalue, "%d", value);
	return fput_string(name, svalue);
}

unsigned int
get_param_int_hex(const char *param)
{
	unsigned int retVal = 0;

	/* check HEX */
	if (strlen(param) > 2 && param[0] == '0' && (param[1] == 'x' || param[1] == 'X'))
		retVal = strtoul(param+2, NULL, 16);
	else
		retVal = atoi(param);

	return retVal;
}

static int
is_param_forbidden(const char *line, const char **forbid_list)
{
	while (*forbid_list) {
		if (strncmp(line, *forbid_list, strlen(*forbid_list)) == 0)
			return 1;
		forbid_list++;
	}
	return 0;
}

void
load_user_config(FILE *fp, const char *dir_name, const char *file_name, const char **forbid_list)
{
	FILE *fp_user;
	char line[256], real_path[128];
	int path_len;

	if (!fp || !dir_name || !file_name)
		return;

	path_len = snprintf(real_path, sizeof(real_path), "%s/%s", dir_name, file_name);
	if (path_len < 0 || path_len >= (int)sizeof(real_path))
		return;

	fp_user = fopen(real_path, "r");
	if (fp_user) {
		while (fgets(line, sizeof(line), fp_user)) {
			size_t len = strlen(line);

			if (len == 0 || line[0] == '\r' || line[0] == '\n' ||
			    line[0] == '#' || line[0] == ';')
				continue;

			if (forbid_list && is_param_forbidden(line, forbid_list))
				continue;

			if (line[len - 1] != '\n') {
				if (len + 1 >= sizeof(line))
					continue;
				line[len++] = '\n';
				line[len] = '\0';
			}

			fputs(line, fp);
		}

		fclose(fp_user);
	}
}

static int
is_safe_module_name(const char *name)
{
	const unsigned char *p = (const unsigned char *)name;

	if (!p || !*p)
		return 0;
	for (; *p; p++) {
		if (!isalnum(*p) && *p != '_' && *p != '-')
			return 0;
	}
	return 1;
}

static int
module_parameter_path(char *path, size_t path_size,
		      const char *module_name, const char *module_param)
{
	int path_len;

	if (!is_safe_module_name(module_name) ||
	    !is_safe_module_name(module_param))
		return -1;
	path_len = snprintf(path, path_size, "/sys/module/%s/parameters/%s",
			    module_name, module_param);
	if (path_len < 0 || path_len >= (int)path_size)
		return -1;
	return 0;
}

int
is_module_loaded(const char *module_name)
{
	DIR *dir_to_open;
	char mod_path[64];
	int path_len;

	if (!is_safe_module_name(module_name))
		return 0;
	path_len = snprintf(mod_path, sizeof(mod_path), "/sys/module/%s", module_name);
	if (path_len < 0 || path_len >= (int)sizeof(mod_path))
		return 0;
	dir_to_open = opendir(mod_path);
	if (dir_to_open) {
		closedir(dir_to_open);
		return 1;
	}

	return 0;
}

int
get_module_refcount(const char *module_name)
{
	FILE *fp;
	char mod_path[64], mod_refval[16], *end;
	long refcount;
	int path_len;

	if (!is_safe_module_name(module_name))
		return -1;
	path_len = snprintf(mod_path, sizeof(mod_path), "/sys/module/%s/refcnt", module_name);
	if (path_len < 0 || path_len >= (int)sizeof(mod_path))
		return -1;
	fp = fopen(mod_path, "r");
	if (!fp)
		return -1;

	if (!fgets(mod_refval, sizeof(mod_refval), fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	errno = 0;
	refcount = strtol(mod_refval, &end, 10);
		if (errno == ERANGE || end == mod_refval || (*end && *end != '\n') ||
	    refcount < 0 || refcount > INT_MAX)
		return -1;

	return (int)refcount;
}

int
module_smart_load(const char *module_name, const char *module_param)
{
	char *param_copy = NULL, *cursor, *token;
	char *argv[32];
	int argc = 0, ret;

	if (!is_safe_module_name(module_name))
		return 0;
	if (is_module_loaded(module_name))
		return 0;

	argv[argc++] = "modprobe";
	argv[argc++] = "-q";
	argv[argc++] = (char *)module_name;

	if (module_param && *module_param) {
		param_copy = strdup(module_param);
		if (!param_copy)
			return 0;
		cursor = param_copy;
		while ((token = strsep(&cursor, " \t\r\n")) != NULL) {
			const unsigned char *p;
			if (!*token)
				continue;
			if (token[0] == '-' || argc >= (int)(sizeof(argv) / sizeof(argv[0])) - 1) {
				free(param_copy);
				return 0;
			}
			for (p = (const unsigned char *)token; *p; p++) {
				if (*p < 32 || *p == 127) {
					free(param_copy);
					return 0;
				}
			}
			argv[argc++] = token;
		}
	}
	argv[argc] = NULL;
	ret = _eval(argv, NULL, 0, NULL);
	free(param_copy);

	return (ret == 0) ? 1 : 0;
}

int
module_smart_unload(const char *module_name, int recurse_unload)
{
	int ret, refcount;
	char *modprobe_argv[] = { "modprobe", "-r", (char *)module_name, NULL };
	char *rmmod_argv[] = { "rmmod", (char *)module_name, NULL };

	if (!is_safe_module_name(module_name))
		return 0;
	refcount = get_module_refcount(module_name);

	/* check module not loaded */
	if (refcount < 0)
		return 0;

	/* check module is used */
	if (refcount > 0)
		return 1;

	ret = recurse_unload ? _eval(modprobe_argv, NULL, 0, NULL) :
		_eval(rmmod_argv, NULL, 0, NULL);

	return (ret == 0) ? 1 : 0;
}

int
module_param_get(const char *module_name, const char *module_param, char *param_value, size_t param_value_size)
{
	FILE *fp;
	char mod_path[256];
	size_t len;

	if (!param_value || param_value_size < 2 ||
	    module_parameter_path(mod_path, sizeof(mod_path), module_name, module_param) < 0)
		return -1;

	fp = fopen(mod_path, "r");
	if (!fp)
		return -1;

	if (!fgets(param_value, param_value_size, fp)) {
		fclose(fp);
		return -1;
	}
	fclose(fp);

	len = strlen(param_value);
		param_value[len - 1] = '\0';
		param_value[len - 1] = '\\0';

	return 0;
}

int
module_param_set_int(const char *module_name, const char *module_param, int param_value)
{
	char mod_path[256];

	if (module_parameter_path(mod_path, sizeof(mod_path), module_name, module_param) < 0)
		return -1;

	return fput_int(mod_path, param_value);
}

void
oom_score_adjust(pid_t pid, int oom_score_adj)
{
	char proc_path[40];
	int path_len;

	if (pid <= 0)
		return;
	path_len = snprintf(proc_path, sizeof(proc_path), "/proc/%u/oom_score_adj", (unsigned int)pid);
	if (path_len < 0 || path_len >= (int)sizeof(proc_path))
		return;
	fput_int(proc_path, oom_score_adj);
}

#if defined (USE_NAND_FLASH)
void mount_rwfs_partition(void)
{
	int mtd_rwfs;
	char dev_ubi[16];
	const char *mp_rwfs = MP_MTD_RWFS;

	if (nvram_get_int("mtd_rwfs_mount") == 0)
		return;

	mtd_rwfs = mtd_dev_idx(MTD_PART_NAME_RWFS);
	if (mtd_rwfs < 3)
		return;

	module_smart_load("ubi", NULL);

	if (!is_module_loaded("ubi"))
		return;

	if (doSystem("ubiattach -p /dev/mtd%d -d %d", mtd_rwfs, mtd_rwfs) == 0) {
		mkdir(mp_rwfs, 0755);
		snprintf(dev_ubi, sizeof(dev_ubi), "/dev/ubi%d_0", mtd_rwfs);
		mount(dev_ubi, mp_rwfs, "ubifs", 0, NULL);
	}
}

void umount_rwfs_partition(void)
{
	int mtd_rwfs;
	const char *mp_rwfs = MP_MTD_RWFS;

	if (check_if_dir_exist(mp_rwfs)) {
		doSystem("/usr/bin/opt-umount.sh %s %s", "/dev/ubi", mp_rwfs);
		
		if (umount(mp_rwfs) == 0)
			rmdir(mp_rwfs);
	}

	mtd_rwfs = mtd_dev_idx(MTD_PART_NAME_RWFS);
	if (mtd_rwfs < 3)
		return;

	if (!is_module_loaded("ubi"))
		return;

	doSystem("ubidetach -p /dev/mtd%d 2>/dev/null", mtd_rwfs);
}

void start_rwfs_optware(void)
{
	int mtd_rwfs;
	char dev_ubi[16];
	const char *mp_rwfs = MP_MTD_RWFS;

	if (!check_if_dir_exist(mp_rwfs))
		return;

	mtd_rwfs = mtd_dev_idx(MTD_PART_NAME_RWFS);
	if (mtd_rwfs < 3)
		return;

	snprintf(dev_ubi, sizeof(dev_ubi), "/dev/ubi%d_0", mtd_rwfs);
	doSystem("/usr/bin/opt-mount.sh %s %s &", dev_ubi, mp_rwfs);
}
#else
inline void mount_rwfs_partition(void) {}
inline void umount_rwfs_partition(void) {}
inline void start_rwfs_optware(void) {}
#endif

void
kill_services(char* svc_name[], int wtimeout, int forcekill)
{
	int i, k, i_waited, i_killed;

	if (wtimeout < 1)
		wtimeout = 1;

	for (i=0;svc_name[i] && *svc_name[i];i++)
		doSystem("killall %s %s", "-q", svc_name[i]);

	for (k=0;k<wtimeout;k++) {
		i_waited = 0;
		for (i=0;svc_name[i] && *svc_name[i];i++) {
			if (pids(svc_name[i])) {
				i_waited = 1;
				break;
			}
		}
		
		if (!i_waited)
			break;
		
		sleep(1);
	}

	if (forcekill) {
		i_killed = 0;
		for (i=0;svc_name[i] && *svc_name[i];i++) {
			if (pids(svc_name[i])) {
				i_killed = 1;
				doSystem("killall %s %s", "-SIGKILL", svc_name[i]);
			}
		}
		if (i_killed)
			sleep(1);
	}
}

int
kill_process_pidfile(char *pidfile, int wtimeout, int forcekill)
{
	int i, result = -1;

	if (wtimeout < 1)
		wtimeout = 1;

	for (i=0; i<wtimeout; i++) {
		if (kill_pidfile(pidfile) != 0)
			break;
		result = 0; // process exist
		sleep(1);
	}

	if (i == wtimeout && forcekill) {
		if (kill_pidfile(pidfile) == 0)
			kill_pidfile_s(pidfile, SIGKILL);
	}

	return result;
}

int
create_file(const char *fn)
{
	int fd = open(fn, O_RDWR | O_CREAT, 0666);
	if (fd >= 0) {
		close(fd);
		return 0;
	}

	return 1;
}

int
check_if_dir_exist(const char *dirpath)
{
	struct stat stat_buf;

	if (!stat(dirpath, &stat_buf))
		return S_ISDIR(stat_buf.st_mode);
	else
		return 0;
}

int
check_if_file_exist(const char *filepath)
{
	struct stat stat_buf;

	if (!stat(filepath, &stat_buf))
		return S_ISREG(stat_buf.st_mode);
	else
		return 0;
}

int
check_if_dev_exist(const char *devpath)
{
	struct stat stat_buf;

	if (!stat(devpath, &stat_buf))
		return (S_ISCHR(stat_buf.st_mode) | S_ISBLK(stat_buf.st_mode));
	else
		return 0;
}

static int
mkdir_component_if_missing(const char *path, mode_t mode)
{
	struct stat st;

	if (mkdir(path, mode) == 0)
		return 0;
	if (errno != EEXIST)
		return -1;
	if (stat(path, &st) < 0)
		return -1;
	if (!S_ISDIR(st.st_mode)) {
		errno = ENOTDIR;
		return -1;
	}
	return 0;
}

int
mkdir_if_none(const char *dirpath, const char *mode)
{
	char *path, *p, *end;
	unsigned long parsed_mode;
	size_t len;
	int ret = 0;

	if (!dirpath || !*dirpath || !mode || !*mode)
		return 0;

	errno = 0;
	parsed_mode = strtoul(mode, &end, 8);
	if (errno == ERANGE || end == mode || *end || parsed_mode > 07777)
		return 0;

	path = strdup(dirpath);
	if (!path)
		return 0;

	len = strlen(path);
	while (len > 1 && path[len - 1] == '/')
		path[--len] = '\0';

	if (len == 1 && path[0] == '/') {
		free(path);
		return 1;
	}

	for (p = path + (path[0] == '/' ? 1 : 0); *p; p++) {
		if (*p == '/') {
			*p = '\0';
			if (*path && mkdir_component_if_missing(path, 0777) < 0)
				goto out;
			*p = '/';
		}
	}

	if (mkdir_component_if_missing(path, (mode_t)parsed_mode) == 0)
		ret = 1;

out:
	free(path);
	return ret;
}

int
rename_if_dir_exist(const char *dir, const char *subdir)
{
	DIR *dirp;
	struct dirent *direntp;
	char oldpath[257], newpath[257];
	int old_len, new_len, renamed;

	if (!dir || !subdir)
		return 0;

	dirp = opendir(dir);
	if (!dirp)
		return 0;

	while ((direntp = readdir(dirp))) {
		if (!strcasecmp(direntp->d_name, subdir) && strcmp(direntp->d_name, subdir)) {
			old_len = snprintf(oldpath, sizeof(oldpath), "%s/%s", dir, direntp->d_name);
			new_len = snprintf(newpath, sizeof(newpath), "%s/%s", dir, subdir);
			renamed = 0;
			if (old_len >= 0 && old_len < (int)sizeof(oldpath) &&
			    new_len >= 0 && new_len < (int)sizeof(newpath))
				renamed = (rename(oldpath, newpath) == 0);
			closedir(dirp);
			return renamed;
		}
	}

	closedir(dirp);
	return 0;
}

char *
if_dircase_exist(const char *dir, const char *subdir)
{
	DIR *dirp;
	struct dirent *direntp;
	char oldpath[257];
	int path_len;
	char *result;

	if (!dir || !subdir)
		return NULL;

	dirp = opendir(dir);
	if (!dirp)
		return NULL;

	while ((direntp = readdir(dirp))) {
		if (!strcasecmp(direntp->d_name, subdir) && strcmp(direntp->d_name, subdir)) {
			path_len = snprintf(oldpath, sizeof(oldpath), "%s/%s", dir, direntp->d_name);
			if (path_len < 0 || path_len >= (int)sizeof(oldpath)) {
				closedir(dirp);
				return NULL;
			}
			result = strdup(oldpath);
			closedir(dirp);
			return result;
		}
	}

	closedir(dirp);
	return NULL;
}

unsigned long
file_size(const char *filepath)
{
	struct stat stat_buf;

	if (!stat(filepath, &stat_buf) && S_ISREG(stat_buf.st_mode))
		return ((unsigned long) stat_buf.st_size);

	return 0;
}

// 1: add, 0: remove.
int
get_hotplug_action(const char *action)
{
	if (!strcmp(action, "remove"))
		return 0;

	return 1;
}
