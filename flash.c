#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "em_msc.h"

#include "flash.h"

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

void flash_write(struct flash_entry **entries, uint32_t location)
{
	struct flash_header header;
	uint32_t offset, total_size;

	int i, status, pages;

	MSC_ExecConfig_TypeDef execConfig = MSC_EXECCONFIG_DEFAULT;
	MSC_ExecConfigSet(&execConfig);

	MSC_Init();

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

			return;
		}

		// Then write the memory specified by the entry.
		status = MSC_WriteWord((uint32_t *)offset, entries[i]->ptr, entries[i]->len);
		printf("\t%s: %p, %ld bytes to %p: %s\r\n",
			entries[i]->name, entries[i]->ptr, entries[i]->len, (void *)offset, flash_status(status));
		offset += entries[i]->len;

		if (status != mscReturnOk)
		{
			printf("Flash Error.\r\n");

			return;
		}
	}
}

void flash_read(struct flash_entry **entries, uint32_t location)
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

		printf("Reading flash index %d at header=%p, data=%p\r\n", i, header, data);
		if (header->len != entries[i]->len)
		{
			printf("Error reading flash entry %s (%d) at %p: length mismatch, %ld != %ld\r\n",
				entries[i]->name, i, header, header->len, entries[i]->len);

			return;
		}
		else if (header->csum != checksum(data, header->len))
		{
			printf("Error reading flash entry %s (%d) at %p: checksum mismatch, %ld != %ld\r\n",
				entries[i]->name, i, data, header->csum, checksum(data, header->len));

			return;
		}

		memcpy(entries[i]->ptr, data, entries[i]->len);
	}
}
