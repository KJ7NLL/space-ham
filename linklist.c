#include <stdlib.h>
#include <string.h>

struct linklist *add_node(struct linklist *l, char *s)
{
	struct linklist *node;

	node = malloc(sizeof(struct linklist));
	node->s = malloc(strlen(s)+1);
	node->next = l;

	return node;
}


