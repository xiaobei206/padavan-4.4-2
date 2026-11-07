#ifndef HTTPD_LOGIN_POLICY_H
#define HTTPD_LOGIN_POLICY_H

/*
 * Use unsigned uptime arithmetic so subtraction remains well-defined across
 * the unsigned counter wrap boundary. The caller owns the timeout policy.
 */
static int
httpd_login_session_expired(unsigned long now, unsigned long last_activity,
			    unsigned long timeout)
{
	return (unsigned long)(now - last_activity) > timeout;
}

#endif /* HTTPD_LOGIN_POLICY_H */
