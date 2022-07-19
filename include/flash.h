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
#define FLASH_DATA_BASE (1024*140)
#define FLASH_FAT_BASE (FLASH_DATA_BASE + 1024*4)
#define FLASH_FAT_SIZE (FLASH_SIZE - FLASH_FAT_BASE)
#define FLASH_FAT_LBA_SIZE 512
#define FLASH_FAT_LBA_COUNT (FLASH_FAT_SIZE/FLASH_FAT_LBA_SIZE)

struct flash_header
{
	uint32_t len;
	uint32_t csum;
};

struct flash_entry
{
	char *name;
	void *ptr;
	uint32_t len;
};

int flash_write(struct flash_entry **entries, uint32_t location);
int flash_read(struct flash_entry **entries, uint32_t location);
char *flash_status(int status);
void flash_init();
