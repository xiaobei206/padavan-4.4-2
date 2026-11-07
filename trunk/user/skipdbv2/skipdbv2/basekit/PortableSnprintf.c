/*
 * Copyright Patrick Powell 1995
 * This code is based on code written by Patrick Powell (papowell@astart.com)
 * It may be used for any purpose as long as this notice remains intact
 * on all source code distributions
 */

/*
 * The host C library supplies the C99 snprintf/vsnprintf implementation.
 * The historical fallback was unreachable because the capability macros
 * were defined unconditionally in this translation unit.
 *
 * Keep this object marker for the existing source aggregation and retain the
 * optional TEST_SNPRINTF harness as a smoke test for the host formatter.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef TEST_SNPRINTF
#include <math.h>
#endif

void dummy_snprintf(void) {}

#ifdef TEST_SNPRINTF

int sprintf(char *str,const char *fmt,...);

int main (void)
{
	char buf1[1024];
	char buf2[1024];
	char *fp_fmt[] = {
		"%1.1f",
		"%-1.5f",
		"%1.5f",
		"%123.9f",
		"%10.5f",
		"% 10.5f",
		"%+22.9f",
		"%+4.9f",
		"%01.3f",
		"%4f",
		"%3.1f",
		"%3.2f",
		"%.0f",
		"%f",
		"-16.16f",
		NULL
	};
	double fp_nums[] = { 6442452944.1234, -1.5, 134.21, 91340.2, 341.1234, 0203.9, 0.96, 0.996,
	0.9996, 1.996, 4.136,  0};
	char *int_fmt[] = {
		"%-1.5d",
		"%1.5d",
		"%123.9d",
		"%5.5d",
		"%10.5d",
		"% 10.5d",
		"%+22.33d",
		"%01.3d",
		"%4d",
		"%d",
		NULL
	};
	long int_nums[] = { -1, 134, 91340, 341, 0203, 0};
	char *str_fmt[] = {
		"10.5s",
		"5.10s",
		"10.1s",
		"0.10s",
		"10.0s",
		"1.10s",
		"%s",
		"%.1s",
		"%.10s",
		"%10s",
		NULL
	};
	char *str_vals[] = {"hello", "a", "", "a longer string", NULL};
	int x, y;
	int fail = 0;
	int num = 0;

	printf ("Testing snprintf format codes against system sprintf...\n");

	for (x = 0; fp_fmt[x] ; x++) {
		for (y = 0; fp_nums[y] != 0 ; y++) {
			int l1 = snprintf(NULL, 0, fp_fmt[x], fp_nums[y]);
			int l2 = snprintf(buf1, sizeof(buf1), fp_fmt[x], fp_nums[y]);
			sprintf (buf2, fp_fmt[x], fp_nums[y]);
			if (strcmp (buf1, buf2)) {
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n",
					   fp_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2) {
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, fp_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	for (x = 0; int_fmt[x] ; x++) {
		for (y = 0; int_nums[y] != 0 ; y++) {
			int l1 = snprintf(NULL, 0, int_fmt[x], int_nums[y]);
			int l2 = snprintf(buf1, sizeof(buf1), int_fmt[x], int_nums[y]);
			sprintf (buf2, int_fmt[x], int_nums[y]);
			if (strcmp (buf1, buf2)) {
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n",
					   int_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2) {
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, int_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	for (x = 0; str_fmt[x] ; x++) {
		for (y = 0; str_vals[y] != 0 ; y++) {
			int l1 = snprintf(NULL, 0, str_fmt[x], str_vals[y]);
			int l2 = snprintf(buf1, sizeof(buf1), str_fmt[x], str_vals[y]);
			sprintf (buf2, str_fmt[x], str_vals[y]);
			if (strcmp (buf1, buf2)) {
				printf("snprintf doesn't match Format: %s\n\tsnprintf = [%s]\n\t sprintf = [%s]\n",
					   str_fmt[x], buf1, buf2);
				fail++;
			}
			if (l1 != l2) {
				printf("snprintf l1 != l2 (%d %d) %s\n", l1, l2, str_fmt[x]);
				fail++;
			}
			num++;
		}
	}

	printf ("%d tests failed out of %d.\n", fail, num);

	printf("seeing how many digits we support\n");
	{
		double v0 = 0.12345678901234567890123456789012345678901;
		for (x=0; x<100; x++) {
			snprintf(buf1, sizeof(buf1), "%1.1f", v0*pow(10, x));
			sprintf(buf2,                "%1.1f", v0*pow(10, x));
			if (strcmp(buf1, buf2)) {
				printf("we seem to support %d digits\n", x-1);
				break;
			}
		}
	}

	return 0;
}
#endif /* SNPRINTF_TEST */
