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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "platform.h"

#include "flash.h"

void flash_init()
{
	MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
	MSC_ExecConfigSet(&execConfig);

	MSC_Init();
}

char *flash_status(int status)
{
	switch (status)
	{
		case mscReturnOk: return "mscReturnOk";
		case -1: return "mscReturnInvalidAddr";
		case -2: return "flashReturnLocked";
		case -3: return "flashReturnTimeOut";
		case -4: return "mscReturnUnaligned";

		default: return "Unkown flash status";
	}
}

uint32_t checksum(void *buf, uint32_t len)
{
	uint32_t csum, i;
	unsigned char *s = (unsigned char *)buf;
	
	for (csum = i = 0; i < len; i++)
	{
		csum += s[i];
	}
	
	return csum;
}

int flash_write(struct flash_entry **entries, uint32_t location)
{
	struct flash_header header;
	uint32_t offset, total_size;

	int i, status, pages;

	flash_init();

	printf("Flash base: %08lx, size=%ld\r\n", FLASH_BASE, FLASH_SIZE);
	printf("User base: %08lx, size=%ld\r\n", USERDATA_BASE, FLASH_SIZE);


	for (i = total_size = 0; entries[i]; i++)
	{
		total_size += entries[i]->len;
	}


	pages = total_size / FLASH_PAGE_SIZE;
	if (total_size % FLASH_PAGE_SIZE)
		pages++;

	printf("Preparing to write %ld bytes (%ld pages)\r\n",
		total_size, total_size / FLASH_PAGE_SIZE);

	offset = location;
	for (i = 0; i < pages; i++)
	{
		status = MSC_ErasePage((uint32_t *)offset);
		printf("Erasing page %d at %p, %ld bytes: %s\r\n", 
			i, (uint32_t *)offset, FLASH_PAGE_SIZE, flash_status(status));
		offset += FLASH_PAGE_SIZE;
	}

	offset = location;
	for (i = 0; entries[i] != NULL; i++)
	{
		// First write the header for this entry.
		header.len = entries[i]->len;
		header.csum = checksum(entries[i]->ptr, entries[i]->len);
		
		status = MSC_WriteWord((uint32_t *)offset, &header, sizeof(struct flash_header));
		printf("Flashed header[%d] at %p, %d bytes to %p; len=%ld, csum=%ld: %s\r\n",
			i, &header, sizeof(struct flash_header), (void *)offset,
			header.len, header.csum,
			flash_status(status)
			);
		offset += sizeof(struct flash_header);

		if (status != mscReturnOk)
		{
			printf("Flash Error.\r\n");

			return 0;
		}

		// Then write the memory specified by the entry.
		status = MSC_WriteWord((uint32_t *)offset, entries[i]->ptr, entries[i]->len);
		printf("\t%s: %p, %ld bytes to %p: %s\r\n",
			entries[i]->name, entries[i]->ptr, entries[i]->len, (void *)offset, flash_status(status));
		offset += entries[i]->len;

		if (status != mscReturnOk)
		{
			printf("Flash Error.\r\n");

			return 0;
		}
	}

	return 1;
}

int flash_read(struct flash_entry **entries, uint32_t location)
{
	void *data;

	struct flash_header *header;

	uint32_t offset = location;

	int i;

	for (i = 0; entries[i] != NULL; i++)
	{
		header = (struct flash_header *) offset;
		offset += sizeof(struct flash_header);
		data = (void *) offset;

		printf("Reading flash entry %s (%d): header=%p, data=%p\r\n", entries[i]->name, i, header, data);
		if (header->len != entries[i]->len)
		{
			printf("Error reading flash entry %s (%d) at %p: length mismatch, %ld != %ld\r\n",
				entries[i]->name, i, header, header->len, entries[i]->len);

			return 0;
		}
		else if (header->csum != checksum(data, header->len))
		{
			printf("Error reading flash entry %s (%d) at %p: checksum mismatch, %ld != %ld\r\n",
				entries[i]->name, i, data, header->csum, checksum(data, header->len));

			return 0;
		}

		memcpy(entries[i]->ptr, data, entries[i]->len);
	}

	return 1;
}
