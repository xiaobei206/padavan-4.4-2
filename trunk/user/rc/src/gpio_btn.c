#include <sys/socket.h>
#include <linux/types.h>
#include <linux/netlink.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <time.h>

#include "rc.h"
#include "gpio_pins.h"
#include <gpioutils.h>

#ifdef GPIO_BTN_DBG
#define btn_dbg(fmt, args...) logmessage("gpio_btn", fmt, ## args)
#else
#define btn_dbg(fmt, args...) do {} while(0)
#endif

#define UEVENT_MSG_LEN	2048

#define BTN_PRESSED		1
#define BTN_RELEASED	2

#define RECV_PERIOD_US		100 * 1000	/* 100ms */
#define CANCEL_COUNT		8			/* 800ms */
#define LONG_COUNT		30			/* 3s */
#define RESET_COUNT		50			/* 5s */

static volatile sig_atomic_t running = 1;

typedef struct _btn_event
{
	char event;
	int btn_id;
} btn_event;

static struct btn_entrys_t
{
	const int btn_id;
	const char *btn_name;
	int count;
	int pressed;
	struct timespec pressed_at;
} btn_entrys[] = {
	{ BTN_RESET, "reset", 0, 0, {0, 0} },
	{ BTN_WPS, "wps", 0, 0, {0, 0} },
	{ BTN_FN1, "fn1", 0, 0, {0, 0} },
	{ BTN_FN2, "fn2", 0, 0, {0, 0} },
	{ 0, NULL, 0 },
};

int get_state_led_pwr(void)
{
	int i_led;

	if (nvram_get_int("front_led_pwr") == 0) {
		// POWER always OFF
		i_led = LED_ON;
	} else {
		// POWER always ON
		i_led = LED_OFF;
	}

	return i_led;
}

static int btn_name_to_id(const char* name)
{
	const struct btn_entrys_t *btn;

	if (name) {
		for (btn = &btn_entrys[0]; btn->btn_id; btn++)
			if (!strcmp(name, btn->btn_name))
				return btn->btn_id;
	}

	return 0;
}

static int btn_uevent_init(void)
{
	struct sockaddr_nl nls;
	struct timeval tv;
	int nlbufsize = 512 * 1024;
	int fd;

	tv.tv_sec = 0;
	tv.tv_usec = RECV_PERIOD_US;

	memset(&nls, 0, sizeof(struct sockaddr_nl));
	nls.nl_family = AF_NETLINK;
	nls.nl_pid = getpid();
	nls.nl_groups = -1;

	if ((fd = socket(PF_NETLINK, SOCK_DGRAM | SOCK_CLOEXEC, NETLINK_KOBJECT_UEVENT)) == -1) {
		btn_dbg("Failed to open hotplug socket: %s\n", strerror(errno));
		exit(1);
	}
	if (bind(fd, (void *)&nls, sizeof(struct sockaddr_nl))) {
		btn_dbg("Failed to bind hotplug socket: %s\n", strerror(errno));
		exit(1);
	}

	/* The receive timeout drives the 100ms hold-duration update loop. */
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUFFORCE,
		&nlbufsize, sizeof(nlbufsize)) != 0) {
		/* SO_RCVBUF is unprivileged and still provides a useful burst queue. */
		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			&nlbufsize, sizeof(nlbufsize)) != 0)
			logmessage("gpio_btn", "Failed to enlarge hotplug receive buffer: %s",
				strerror(errno));
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) != 0) {
		int saved_errno = errno;
		logmessage("gpio_btn", "Failed to configure hotplug receive timeout: %s",
			strerror(saved_errno));
		close(fd);
		errno = saved_errno;
		
		exit(EXIT_FAILURE);
	}

	return fd;
}

static void nlmsg_handle(const char *msg, int len, btn_event *ev)
{
	int i = 0;
	int btn_id = 0;
	char parsed_event = 0;

	if (NULL == msg || NULL == ev || len <= 0)
		return;

	/* Uevents are NUL-delimited; validate the complete datagram before
	 * interpreting any field so truncated messages cannot read past len. */
	while (i < len) {
		const char *end = memchr(msg + i, '\0', (size_t)(len - i));
		if (NULL == end)
			return;
		i = (int)(end - msg) + 1;
	}

	i = 0;
	while (i < len) {
		const char *field = msg + i;
		const char *end = memchr(field, '\0', (size_t)(len - i));
		size_t field_len = (size_t)(end - field);
		const char *eq = memchr(field, '=', field_len);

		if (eq) {
			size_t name_len = (size_t)(eq - field);
			const char *value = eq + 1;

			if (name_len == sizeof("ACTION") - 1 &&
			    memcmp(field, "ACTION", sizeof("ACTION") - 1) == 0) {
				if (!strcmp(value, "pressed"))
					parsed_event = BTN_PRESSED;
				else if (!strcmp(value, "released"))
					parsed_event = BTN_RELEASED;
				else
					return;
			}
			if (parsed_event &&
			    name_len == sizeof("BUTTON") - 1 &&
			    memcmp(field, "BUTTON", sizeof("BUTTON") - 1) == 0) {
				btn_id = btn_name_to_id(value);
				if (btn_id) {
					ev->event = parsed_event;
					ev->btn_id = btn_id;
				}
				return;
			}
		}
		i = (int)(end - msg) + 1;
	}
}

static void sig_handle(int signum)
{
	running = 0;
}

static void btn_released_handle(int btn_id, int count)
{
	btn_dbg("released_handle: btn=0x%x, count=%d\n", btn_id, count);

	if (btn_id == BTN_RESET) {
		if (count > RESET_COUNT) {
			btn_dbg("perform RESET!!\n");
			LED_CONTROL(LED_PWR, LED_OFF);
			btn_reset_action();
		} else if (count > 0) {
			btn_dbg("RESET pressed time too short!\n");
			LED_CONTROL(LED_PWR, LED_ON);
		}
	} else {
		if (count > LONG_COUNT) {
			btn_dbg("perform long event, btn=0x%x\n", btn_id);
			btn_event_long(btn_id);
		} else if (count > 0 && count < CANCEL_COUNT) {
			btn_dbg("perform short event, btn=0x%x\n", btn_id);
			btn_event_short(btn_id);
		}
	}
}

static void btn_update_press_count(struct btn_entrys_t *btn)
{
	struct timespec now;
	long long sec, nsec;
	unsigned long long ticks;
	int count, pwr_led;

	if (!btn->pressed || clock_gettime(CLOCK_MONOTONIC, &now) < 0)
		return;
	sec = (long long)now.tv_sec - (long long)btn->pressed_at.tv_sec;
	nsec = (long long)now.tv_nsec - (long long)btn->pressed_at.tv_nsec;
	if (nsec < 0) { sec--; nsec += 1000000000LL; }
	if (sec < 0) { sec = 0; nsec = 0; }
	ticks = (unsigned long long)sec * 10ULL +
		(unsigned long long)nsec / 100000000ULL + 1ULL;
	count = ticks > INT_MAX ? INT_MAX : (int)ticks;
	if (count <= btn->count)
		return;
	btn->count = count;
	if (!(LED_PWR & search_gpio_led()))
		return;
	pwr_led = get_state_led_pwr();
	if ((btn->btn_id == BTN_RESET && count > RESET_COUNT) ||
	    (btn->btn_id != BTN_RESET && count > LONG_COUNT)) {
		btn_dbg("you can release btn now!\n");
		gpio_led_set(LED_PWR, (count % 2) ? !pwr_led : pwr_led);
	}
}

static void btn_hold_tick(void)
{
	struct btn_entrys_t *btn;
	for (btn = &btn_entrys[0]; btn->btn_id; btn++)
		btn_update_press_count(btn);
}

static void btn_ev_handle(btn_event *ev)
{
	struct btn_entrys_t *btn;
	int cnt;
	for (btn = &btn_entrys[0]; btn->btn_id; btn++) {
		if (ev->btn_id != btn->btn_id)
			continue;
		if (ev->event == BTN_PRESSED) {
			if (!btn->pressed) {
				if (clock_gettime(CLOCK_MONOTONIC, &btn->pressed_at) < 0) {
					btn_dbg("failed to read monotonic clock: %s\n", strerror(errno));
				} else {
					btn->pressed = 1;
					btn->count = 1;
					if (btn->btn_id == BTN_RESET && (LED_PWR & search_gpio_led()))
						gpio_led_set(LED_PWR, get_state_led_pwr());
				}
			}
		} else if (ev->event == BTN_RELEASED) {
			if (btn->pressed) {
				btn_update_press_count(btn);
				cnt = btn->count;
				btn->pressed = 0;
				btn->count = 0;
				btn_released_handle(btn->btn_id, cnt);
			}
			ev->event = 0;
			ev->btn_id = 0;
		}
		break;
	}
}
int start_gpio_btn(void)
{
	if (pids("gpio_btn"))
		return 0;

	return eval("/sbin/gpio_btn");
}

int btn_main(int argc, char *argv[])
{
	char buf[UEVENT_MSG_LEN] = {0};
	int msglen = 0;
	int fd;
	int exit_status = EXIT_SUCCESS;
	btn_event btn_ev;
	struct sigaction sa;
	pid_t pid;

	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = sig_handle;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGTERM, &sa, NULL);
	sigaction(SIGINT, &sa, NULL);

	if (daemon(0, 0) < 0) {
		perror("daemon");
		exit(errno);
	}

	pid = getpid();

	/* never invoke oom killer */
	oom_score_adjust(pid, OOM_SCORE_ADJ_MIN);

	fd = btn_uevent_init();

	memset(&btn_ev, 0, sizeof(btn_ev));

	logmessage("gpio_btn", "service started.");

	while (running)
	{
		msglen = recv(fd, buf, sizeof(buf), 0);

		if (msglen > 0) {
			memset(&btn_ev, 0, sizeof(btn_ev));
			nlmsg_handle(buf, msglen, &btn_ev);
		} else if (msglen < 0 && errno != EINTR &&
			   errno != EAGAIN && errno != EWOULDBLOCK) {
			logmessage("gpio_btn", "Failed to receive hotplug event: %s", strerror(errno));
			exit_status = EXIT_FAILURE;
			break;
		}

		if (btn_ev.btn_id && btn_ev.event) {
			//btn_dbg("btn_ev: btn=0x%x, event=%d\n", btn_ev.btn_id, btn_ev.event);
			btn_ev_handle(&btn_ev);
		}
		btn_hold_tick();
	}

	close(fd);

	return exit_status;
}
