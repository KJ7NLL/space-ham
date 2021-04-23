#include <stdlib.h>
#include <string.h>

#include "linklist.h"

struct linklist *add_node(struct linklist *l, char *s)
{
	struct linklist *node = NULL;

	node = malloc(sizeof(struct linklist));
	if (node == NULL)
	{
		return NULL;
	}

	node->s = malloc(strlen(s)+1);
	if (node->s == NULL)
	{
		free(node);
		return NULL;
	}

	strcpy(node->s, s);
	node->next = l;
	node->prev = NULL;

	if (node->next != NULL)
	{
		node->next->prev = node;
	}

	return node;
}


