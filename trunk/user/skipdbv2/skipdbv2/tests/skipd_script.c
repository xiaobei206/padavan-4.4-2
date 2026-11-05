#include "skipd_internal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int log_calls;

void skipd_log(int filter, const char *format, ...)
{
    (void)filter;
    (void)format;
    log_calls++;
}

int main(void)
{
    char long_name[DELAY_KEY_LEN];
    const char injected[] = "scripts/boot.sh;reboot";
    const char malformed[] = {'s', 'c', 'r', 'i', 'p', 't', '\0', 'x', '\0'};

    assert(is_valid_script_name("daily_01.sh", sizeof("daily_01.sh") - 1));
    assert(!is_valid_script_name(NULL, 1));
    assert(!is_valid_script_name("", 0));
    assert(!is_valid_script_name(".", 1));
    assert(!is_valid_script_name("..", 2));
    assert(!is_valid_script_name("../boot.sh", sizeof("../boot.sh") - 1));
    assert(!is_valid_script_name("bad/name", sizeof("bad/name") - 1));

    memset(long_name, 'a', sizeof(long_name));
    assert(!is_valid_script_name(long_name, sizeof(long_name)));

    assert(is_safe_script_command("/etc/storage/scripts/daily.sh",
        sizeof("/etc/storage/scripts/daily.sh") - 1));
    assert(is_safe_script_command("./scripts/boot_1.sh",
        sizeof("./scripts/boot_1.sh") - 1));
    assert(!is_safe_script_command("", 0));
    assert(!is_safe_script_command("scripts/../boot.sh",
        sizeof("scripts/../boot.sh") - 1));
    assert(!is_safe_script_command("scripts//boot.sh",
        sizeof("scripts//boot.sh") - 1));
    assert(!is_safe_script_command("scripts/boot.sh;reboot",
        sizeof("scripts/boot.sh;reboot") - 1));
    assert(!is_safe_script_command("scripts/boot.sh reboot",
        sizeof("scripts/boot.sh reboot") - 1));
    assert(!is_safe_script_command("scripts/boot.sh/",
        sizeof("scripts/boot.sh/") - 1));

    /* Invalid/malformed inputs must be rejected before system() is reached. */
    sys_script(injected, sizeof(injected));
    sys_script(malformed, sizeof(malformed));
    assert(log_calls == 2);

    puts("SkipDB script safety tests passed.");
    return 0;
}
