struct linklist
{
	char *s;
	struct linklist *next;
	struct linklist *prev;
};

struct linklist *add_node(struct linklist *l, char *s);
