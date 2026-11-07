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
#include <limits.h>
#include <stddef.h>
#include <string.h>

/* Base-64 decoding.  This represents binary data as printable ASCII
** characters.  Three 8-bit binary bytes are turned into four 6-bit
** values, like so:
**
**   [11111111]  [22222222]  [33333333]
**
**   [111111] [112222] [222233] [333333]
**
** Then the 6-bit values are represented using the characters "A-Za-z0-9+/".
*/

static const signed char b64_decode_table[256] = {
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 00-0F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 10-1F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,62,-1,-1,-1,63,  /* 20-2F */
    52,53,54,55,56,57,58,59,60,61,-1,-1,-1,-1,-1,-1,  /* 30-3F */
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,10,11,12,13,14,  /* 40-4F */
    15,16,17,18,19,20,21,22,23,24,25,-1,-1,-1,-1,-1,  /* 50-5F */
    -1,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,  /* 60-6F */
    41,42,43,44,45,46,47,48,49,50,51,-1,-1,-1,-1,-1,  /* 70-7F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 80-8F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* 90-9F */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* A0-AF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* B0-BF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* C0-CF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* D0-DF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,  /* E0-EF */
    -1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1   /* F0-FF */
};

/* Do base-64 decoding on a string.  Ignore any non-base64 bytes.
** Return the actual number of bytes generated.  The decoded size will
** be at most 3/4 the size of the encoded, and may be smaller if there
** are padding characters (blanks, newlines).
*/
int
b64_decode(const char* str, unsigned char* space, int size )
{
	const char* cp;
	int d, prev_d = 0;
	int space_idx, phase;
	unsigned char c;

	if (!str || !space || size <= 0)
		return -1;

	space_idx = 0;
	phase = 0;
	for ( cp = str; *cp != '\0'; ++cp ) {
		d = (int)b64_decode_table[(unsigned char)*cp];
		if ( d != -1 ) {
			switch ( phase )
			{
			case 0:
				++phase;
				break;
			case 1:
				c = ( ( prev_d << 2 ) | ( ( d & 0x30 ) >> 4 ) );
				if (space_idx >= size)
					return -1;
				space[space_idx++] = c;
				++phase;
				break;
			case 2:
				c = ( ( ( prev_d & 0xf ) << 4 ) | ( ( d & 0x3c ) >> 2 ) );
				if (space_idx >= size)
					return -1;
				space[space_idx++] = c;
				++phase;
				break;
			case 3:
				c = ( ( ( prev_d & 0x03 ) << 6 ) | d );
				if (space_idx >= size)
					return -1;
				space[space_idx++] = c;
				phase = 0;
				break;
			}
			prev_d = d;
		}
	}

	return space_idx;
}


/*
 * Strict RFC 4648 decoder for HTTP authentication. Unlike the legacy
 * decoder above, this rejects ignored bytes, misplaced padding, missing
 * padding, and non-zero unused pad bits. Validate the complete input before
 * writing any output so malformed credentials cannot leave partial data.
 */
int
b64_decode_strict(const char *str, unsigned char *space, int size)
{
	size_t input_len, groups, output_len, i, output_idx;
	int padding, d0, d1, d2, d3;

	if (!str || !space || size < 0)
		return -1;

	input_len = strlen(str);
	if (input_len == 0)
		return 0;
	if ((input_len % 4) != 0)
		return -1;

	groups = input_len / 4;
	if (groups > (size_t)INT_MAX / 3)
		return -1;

	padding = 0;
	if (str[input_len - 1] == '=') {
		padding = 1;
		if (input_len > 1 && str[input_len - 2] == '=')
			padding = 2;
	}
	output_len = groups * 3 - (size_t)padding;
	if (output_len > (size_t)size)
		return -1;

	/* Validate alphabet, padding placement, and canonical unused bits. */
	for (i = 0; i < input_len; i += 4) {
		d0 = (int)b64_decode_table[(unsigned char)str[i]];
		d1 = (int)b64_decode_table[(unsigned char)str[i + 1]];
		if (d0 < 0 || d1 < 0)
			return -1;

		if (i + 4 < input_len) {
			d2 = (int)b64_decode_table[(unsigned char)str[i + 2]];
			d3 = (int)b64_decode_table[(unsigned char)str[i + 3]];
			if (d2 < 0 || d3 < 0)
				return -1;
		} else if (padding == 2) {
			if (str[i + 2] != '=' || str[i + 3] != '=' ||
			    (d1 & 0x0f) != 0)
				return -1;
		} else if (padding == 1) {
			d2 = (int)b64_decode_table[(unsigned char)str[i + 2]];
			if (d2 < 0 || str[i + 3] != '=' ||
			    (d2 & 0x03) != 0)
				return -1;
		} else {
			d2 = (int)b64_decode_table[(unsigned char)str[i + 2]];
			d3 = (int)b64_decode_table[(unsigned char)str[i + 3]];
			if (d2 < 0 || d3 < 0)
				return -1;
		}
	}

	/* The second pass cannot fail after the validation above. */
	output_idx = 0;
	for (i = 0; i < input_len; i += 4) {
		d0 = (int)b64_decode_table[(unsigned char)str[i]];
		d1 = (int)b64_decode_table[(unsigned char)str[i + 1]];
		d2 = (int)b64_decode_table[(unsigned char)str[i + 2]];
		d3 = (int)b64_decode_table[(unsigned char)str[i + 3]];

		space[output_idx++] = (unsigned char)((d0 << 2) | (d1 >> 4));
		if (str[i + 2] != '=')
			space[output_idx++] = (unsigned char)(((d1 & 0x0f) << 4) | (d2 >> 2));
		if (str[i + 3] != '=')
			space[output_idx++] = (unsigned char)(((d2 & 0x03) << 6) | d3);
	}

	return (int)output_idx;
}
