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
#ifndef LINKLIST_H
#define LINKLIST_H

struct llnode
{
	union
	{
		char *s;
		void *data;
	};

	struct llnode *next;
	struct llnode *prev;
};

struct linklist
{
	struct llnode *head, *tail;
};

struct llnode *add_head_node_str(struct linklist *l, char *s);
struct llnode *add_head_node(struct linklist *l, void *data);
struct llnode *add_tail_node(struct linklist *l, void *data);
void *delete_node(struct linklist *l, struct llnode *node);

#endif