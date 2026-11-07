#ifndef HTTPD_ACCESS_POLICY_H
#define HTTPD_ACCESS_POLICY_H

/*
 * Private-key disclosure requires both an active authenticated login and a
 * trusted LAN/VPN source. Transport encryption is deliberately not an input:
 * HTTPS does not turn an untrusted network into a trusted one.
 */
static int
httpd_private_key_access_allowed(int login_active, int trusted_network)
{
	return login_active && trusted_network;
}

#endif /* HTTPD_ACCESS_POLICY_H */
