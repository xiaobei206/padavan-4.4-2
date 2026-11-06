#include "UArray.h"

#include <stdio.h>
#include <stdlib.h>

static int check(int condition, const char *message)
{
	if (condition)
		return 1;
	fprintf(stderr, "%s\n", message);
	return 0;
}

int main(void)
{
	if (!check(CTYPE_forName(NULL) == -1,
		"CTYPE_forName must reject NULL input")) 
		return EXIT_FAILURE;
	if (!check(CENCODING_forName(NULL) == -1,
		"CENCODING_forName must reject NULL input"))
		return EXIT_FAILURE;
	if (!check(CTYPE_forName("uint8") == CTYPE_uint8_t &&
		CTYPE_forName("float64") == CTYPE_float64_t &&
		CTYPE_forName("not-a-type") == -1,
		"CTYPE name lookup changed for valid or unknown names"))
		return EXIT_FAILURE;
	if (!check(CENCODING_forName("utf8") == CENCODING_UTF8 &&
		CENCODING_forName("number") == CENCODING_NUMBER &&
		CENCODING_forName("not-an-encoding") == -1,
		"CENCODING name lookup changed for valid or unknown names"))
		return EXIT_FAILURE;
	puts("UArray name lookup test passed");
	return EXIT_SUCCESS;
}
