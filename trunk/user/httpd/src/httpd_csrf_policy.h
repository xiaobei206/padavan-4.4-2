#ifndef HTTPD_CSRF_POLICY_H
#define HTTPD_CSRF_POLICY_H

#include <ctype.h>
#include <stddef.h>
#include <string.h>

#define HTTPD_CSRF_ORIGIN_MAX 512
#define HTTPD_CSRF_REFERER_MAX 2048
#define HTTPD_CSRF_HOST_MAX 256

static int
httpd_csrf_copy_header_value(char *destination, size_t destination_size,
                             const char *value)
{
	const char *start;
	size_t length, i;

	if (!destination || destination_size == 0 || !value)
		return 0;

	start = value;
	while (*start == ' ' || *start == '\t')
		start++;

	length = strcspn(start, "\r\n");
	while (length > 0 && (start[length - 1] == ' ' ||
	                      start[length - 1] == '\t'))
		length--;
	if (length == 0 || length >= destination_size)
		return 0;

	for (i = 0; i < length; ++i) {
		unsigned char ch = (unsigned char)start[i];
		if (ch < 0x20 || ch == 0x7f)
			return 0;
	}

	memcpy(destination, start, length);
	destination[length] = '\0';
	return 1;
}

static int
httpd_csrf_prefix_equal(const char *value, const char *prefix)
{
	while (*prefix) {
		if (tolower((unsigned char)*value) !=
		    tolower((unsigned char)*prefix))
			return 0;
		value++;
		prefix++;
	}
	return 1;
}

/* Split a URL authority into a case-folded host and numeric port. */
static int
httpd_csrf_parse_authority(const char *authority, size_t length, int is_https,
                           char *host_out, size_t host_out_size,
                           unsigned int *port_out)
{
	size_t host_length = 0, i, colon_position = 0, colon_count = 0;
	const char *port_text = NULL;
	size_t port_length = 0;
	unsigned long port;
	int bracketed = 0;

	if (!authority || length == 0 || !host_out || host_out_size == 0 ||
	    !port_out)
		return 0;

	for (i = 0; i < length; ++i) {
		unsigned char ch = (unsigned char)authority[i];
		if (ch <= 0x20 || ch >= 0x7f || ch == '@' || ch == ',' || ch == '\\')
			return 0;
	}

	if (authority[0] == '[') {
		size_t close_position = length;
		bracketed = 1;
		for (i = 1; i < length; ++i) {
			if (authority[i] == ']') {
				close_position = i;
				break;
			}
		}
		if (close_position == length || close_position == 1)
			return 0;
		host_length = close_position + 1;
		if (host_length < length) {
			if (authority[host_length] != ':')
				return 0;
			port_text = authority + host_length + 1;
			port_length = length - host_length - 1;
		}
	} else {
		for (i = 0; i < length; ++i) {
			if (authority[i] == ':') {
				colon_position = i;
				colon_count++;
			}
		}
		if (colon_count > 1)
			return 0; /* IPv6 literals must be bracketed in an authority. */
		if (colon_count == 1) {
			host_length = colon_position;
			port_text = authority + colon_position + 1;
			port_length = length - colon_position - 1;
		} else {
			host_length = length;
		}
	}

	if (host_length == 0 || host_length >= host_out_size)
		return 0;

	for (i = 0; i < host_length; ++i) {
		unsigned char ch = (unsigned char)authority[i];
		if (bracketed && (i == 0 || i == host_length - 1)) {
			if (ch != '[' && ch != ']')
				return 0;
		} else if (!(isalnum(ch) || ch == '.' || ch == '-' || ch == '_' ||
		             (bracketed && (ch == ':' || ch == '%')))) {
			return 0;
		}
		host_out[i] = (char)tolower(ch);
	}
	host_out[host_length] = '\0';

	if (!port_text) {
		port = is_https ? 443UL : 80UL;
	} else {
		if (port_length == 0)
			return 0;
		port = 0;
		for (i = 0; i < port_length; ++i) {
			unsigned char ch = (unsigned char)port_text[i];
			if (ch < '0' || ch > '9')
				return 0;
			port = port * 10UL + (unsigned long)(ch - '0');
			if (port > 65535UL)
				return 0;
		}
		if (port == 0)
			return 0;
	}

	*port_out = (unsigned int)port;
	return 1;
}

static int
httpd_csrf_authorities_match(const char *url_authority, size_t url_length,
                             const char *request_host, int is_https)
{
	char url_host[HTTPD_CSRF_HOST_MAX];
	char header_host[HTTPD_CSRF_HOST_MAX];
	unsigned int url_port, header_port;

	if (!request_host || !*request_host)
		return 0;
	if (!httpd_csrf_parse_authority(url_authority, url_length, is_https,
	                                url_host, sizeof(url_host), &url_port))
		return 0;
	if (!httpd_csrf_parse_authority(request_host, strlen(request_host),
	                                is_https, header_host,
	                                sizeof(header_host), &header_port))
		return 0;

	return url_port == header_port && strcmp(url_host, header_host) == 0;
}

/* Origin has no path by definition; Referer may contain one. */
static int
httpd_csrf_url_matches_host(const char *url, const char *request_host,
                            int is_https, int origin_header)
{
	const char *authority, *end;
	size_t scheme_length, authority_length;
	int url_is_https;

	if (!url || !*url)
		return 0;
	if (httpd_csrf_prefix_equal(url, "http://")) {
		url_is_https = 0;
		scheme_length = 7;
	} else if (httpd_csrf_prefix_equal(url, "https://")) {
		url_is_https = 1;
		scheme_length = 8;
	} else {
		return 0;
	}

	if (url_is_https != !!is_https)
		return 0;

	authority = url + scheme_length;
	end = strpbrk(authority, "/?#");
	if (origin_header && end != NULL)
		return 0;
	authority_length = end ? (size_t)(end - authority) : strlen(authority);

	return httpd_csrf_authorities_match(authority, authority_length,
	                                    request_host, is_https);
}

/*
 * Best-effort browser CSRF mitigation for authenticated POSTs. Explicit
 * Origin takes precedence over Referer. A present but malformed, duplicate,
 * null, or cross-origin value fails closed. When both headers are absent,
 * preserve compatibility with legacy non-browser clients; this is not a
 * substitute for a synchronizer token on APIs that require complete CSRF
 * protection.
 */
static int
httpd_csrf_post_allowed(const char *origin, const char *referer,
                        const char *request_host, int is_https,
                        int origin_invalid, int referer_invalid,
                        int host_invalid)
{
	if (origin_invalid)
		return 0;

	if (origin && *origin) {
		if (host_invalid)
			return 0;
		return httpd_csrf_url_matches_host(origin, request_host,
		                                   is_https, 1);
	}

	if (referer_invalid)
		return 0;
	if (referer && *referer) {
		if (host_invalid)
			return 0;
		return httpd_csrf_url_matches_host(referer, request_host,
		                                   is_https, 0);
	}

	return 1; /* compatibility path: neither metadata header was sent */
}

#endif /* HTTPD_CSRF_POLICY_H */
