#include <string.h>

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


void shift_right(char *s, int pos)
{
	int i;

	for (i = strlen(s); i > pos; i--)
	{
		s[i] = s[i-1];
	}
}

