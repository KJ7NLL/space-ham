#include <string.h>
#include <ctype.h>

// Returns 1 if a and b match.
int match(char *a, char *b)
{
	int i = 0;

	if (strlen(a) != strlen(b))
	{
		return 0;
	}

	for (i = 0; a[i]; i++)
	{
		if (a[i] != b[i])
		{
			return 0;
		}
	}

	return 1;
}

// Return the length of the longest string in the array of strings (not the index).
int longest_length(char **a)
{
	int length = 0, i, n;

	for (i = 0; a[i] != NULL; i++)
	{
		n = strlen(a[i]);
		if (n > length)
		{
			length = n;
		}
	}

	return length;
}

// Shift all of the characters in s to the left at position pos.
// The character at position pos is deleted
void shift_left(char *s, int pos)
{
	unsigned int i;

	for (i = pos; i < strlen(s) - 1; i++)
	{
		s[i] = s[i+1];
	}
}

// Shift all of the characters in s to the right at position pos.
// The character at position pos is unmodified
void shift_right(char *s, int pos)
{
	int i;

	for (i = strlen(s); i > pos; i--)
	{
		s[i] = s[i-1];
	}
}

/*
 * Parse s on spaces into \0 delimited values and point argc at each value.
 * argc is the maximum number of arguments that can fit in args.
 * Note: s is modified in place, whitespace is replaced with \0
 */
int parse_args(char *s, char **args, int argc)
{
	int i, len, n = 0;

	len = strlen(s);
	args[0] = NULL;
	for (i = 0; i < len; i++)
	{
		if (isspace((int)s[i]))
		{
			s[i] = 0;
			if (n + 1 < argc && args[n] != NULL)
			{
				n++;
				args[n] = NULL;
			}
		}
		else if (args[n] == NULL)
			args[n] = &s[i];
	}

	if (args[n] != NULL)
		n++;

	return n;
}

// https://stackoverflow.com/a/18858248
char *lltoa(long long val, int base)
{
	static char buf[64] = { 0 };

	int i = 62;
	int sign = (val < 0);

	if (sign)
		val = -val;

	if (val == 0)
		return "0";

	for (; val && i; --i, val /= base)
	{
		buf[i] = "0123456789abcdef"[val % base];
	}

	if (sign)
	{
		buf[i--] = '-';
	}
	return &buf[i + 1];
}
