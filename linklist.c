//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 3 of the License, or
//  (at your option) any later version.
// 
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU Library General Public License for more details.
// 
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// 
//  Copyright (C) 2022- by Ezekiel Wheeler, KJ7NLL and Eric Wheeler, KJ7LNW.
//  All rights reserved.
//
//  The official website and doumentation for space-ham is available here:
//    https://www.kj7nll.radio/
//
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


