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

// fatfs includes
#include "ff.h"
#include "diskio.h"

// Our includes
#include "flash.h"

#define lba_to_ptr(lba)		((void *)(FLASH_FAT_BASE + lba*FLASH_FAT_LBA_SIZE))
#define lba_page(lba)		((void *)((uint32_t)lba_to_ptr(lba) & (~(FLASH_PAGE_SIZE-1))))
#define lba_page_offset(lba)	((uint32_t)lba_to_ptr(lba) & (FLASH_PAGE_SIZE-1))

static int global_status = STA_NOINIT;


DSTATUS disk_status(BYTE pdrv)
{
	return global_status;
}

DSTATUS disk_initialize(BYTE pdrv)
{
#ifdef __EFR32__
	flash_init();
#endif
	global_status = 0;

	return global_status;
}

DRESULT disk_read(BYTE pdrv, BYTE *buf, LBA_t sector, UINT count)
{
#ifdef __EFR32__
	memcpy(buf, lba_to_ptr(sector), count*FLASH_FAT_LBA_SIZE);
#endif

	return RES_OK;
}

#if FF_FS_READONLY == 0

DRESULT msc_erase_write(uint32_t *page, unsigned char *buf)
{
#ifdef __EFR32__
	int status;

	status = MSC_ErasePage((uint32_t *)page);

	if (status != mscReturnOk)
	{
		printf("msc_erase_write: erase error: %s\r\n",
			flash_status(status));

		return RES_ERROR;
	}

	status = MSC_WriteWord((uint32_t *)page, buf, FLASH_PAGE_SIZE);

	if (status != mscReturnOk)
	{
		printf("msc_erase_write: write error: %s\r\n",
			flash_status(status));

		return RES_ERROR;
	}
#endif

	return RES_OK;
}

DRESULT disk_write(BYTE pdrv, const BYTE *buf, LBA_t sector, UINT count)
{
#ifdef __EFR32__
	DRESULT status;
	unsigned char tmp[FLASH_PAGE_SIZE];
	LBA_t i, offset;
	void *prev_page, *page;

	prev_page = page = lba_page(sector);
	memcpy(tmp, page, FLASH_PAGE_SIZE);

	// only erase on new page
	// write before erase when changing pages
	for (i = sector; i < sector+count; i++)
	{
		page = lba_page(i);

		if (page != prev_page)
		{
			status = msc_erase_write((uint32_t *)prev_page, tmp);
			if (status != RES_OK)
				return status;

			// Copy the new page into the tmp buffer
			memcpy(tmp, page, FLASH_PAGE_SIZE);
		}

		offset = (i-sector)*FLASH_FAT_LBA_SIZE;
		memcpy(tmp+lba_page_offset(i), buf+offset, FLASH_FAT_LBA_SIZE);
		prev_page = page;
	}

	status = msc_erase_write((uint32_t *)page, tmp);
	return status;
#else
	return RES_OK;
#endif
}

#endif

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buf)
{
#ifdef __EFR32__
	LBA_t *lba = buf;
	WORD *word = buf;
	DWORD *dword = buf;

	switch (cmd)
	{
		case CTRL_SYNC: return RES_OK;

		// Retrieves number of available sectors, the largest allowable LBA + 1
		case GET_SECTOR_COUNT: 
			*lba = FLASH_FAT_LBA_COUNT;
			printf("GET_SECTOR_COUNT: %lu\r\n", FLASH_FAT_LBA_COUNT);
			return RES_OK;

		// Valid sector sizes are 512, 1024, 2048 and 4096.
		case GET_SECTOR_SIZE:
			*word = FLASH_FAT_LBA_SIZE;
			printf("FLASH_FAT_LBA_SIZE: %u\r\n", FLASH_FAT_LBA_SIZE);
			return RES_OK;

		// Retrieves erase block size in unit of sector
		case GET_BLOCK_SIZE:
			*dword = FLASH_PAGE_SIZE / FLASH_FAT_LBA_SIZE;
			printf("FLASH_PAGE_SIZE: %lu\r\n", FLASH_PAGE_SIZE);
			return RES_OK;

		case CTRL_TRIM:
			// TODO: if lba[0] and lba[1] are page aligned, then erase the page
			printf("FLASH_TRIM: not implemented, lba=%lu\r\n", *lba);
			return RES_OK;
	}
#endif

	return RES_PARERR;
}

FRESULT scan_files(char *path	/* Start node to be scanned (***also used as
				   work area***) */
	)
{
	FRESULT res;
	DIR dir;
	UINT i;
	static FILINFO fno;

	res = f_opendir(&dir, path);	/* Open the directory */
	if (res == FR_OK)
	{
		for (;;)
		{
			res = f_readdir(&dir, &fno);	/* Read a directory
							   item */
			if (res != FR_OK || fno.fname[0] == 0)
				break;	/* Break on error or end of dir */
			if (fno.fattrib & AM_DIR)
			{	/* It is a directory */
				i = strlen(path);
				sprintf(&path[i], "/%s", fno.fname);
				res = scan_files(path);	/* Enter the
							   directory */
				if (res != FR_OK)
					break;
				path[i] = 0;
			}
			else
			{	/* It is a file. */
				printf("%s/%s\r\n", path, fno.fname);
			}
		}
		f_closedir(&dir);
	}

	return res;
}

char *ff_strerror(FRESULT r)
{
	char *errors[] = {
		[FR_OK] = "FR_OK",
		[FR_DISK_ERR] = "FR_DISK_ERR",
		[FR_INT_ERR] = "FR_INT_ERR",
		[FR_NOT_READY] = "FR_NOT_READY",
		[FR_NO_FILE] = "FR_NO_FILE",
		[FR_NO_PATH] = "FR_NO_PATH",
		[FR_INVALID_NAME] = "FR_INVALID_NAME",
		[FR_DENIED] = "FR_DENIED",
		[FR_EXIST] = "FR_EXIST",
		[FR_INVALID_OBJECT] = "FR_INVALID_OBJECT",
		[FR_WRITE_PROTECTED] = "FR_WRITE_PROTECTED",
		[FR_INVALID_DRIVE] = "FR_INVALID_DRIVE",
		[FR_NOT_ENABLED] = "FR_NOT_ENABLED",
		[FR_NO_FILESYSTEM] = "FR_NO_FILESYSTEM",
		[FR_MKFS_ABORTED] = "FR_MKFS_ABORTED",
		[FR_TIMEOUT] = "FR_TIMEOUT",
		[FR_LOCKED] = "FR_LOCKED",
		[FR_NOT_ENOUGH_CORE] = "FR_NOT_ENOUGH_CORE",
		[FR_TOO_MANY_OPEN_FILES] = "FR_TOO_MANY_OPEN_FILES",
		[FR_INVALID_PARAMETER] = "FR_INVALID_PARAMETER",
	};

	if (r >= sizeof(errors)/sizeof(char *))
		return "unkown fresult error";

	return errors[r];
}

FRESULT f_read_file(char *filename, void *data, size_t len)
{
	FRESULT res = FR_OK;  /* API result code */
	FIL in;              /* File object */
	UINT br;          /* Bytes written */

	res = f_open(&in, filename, FA_READ);
	if (res != FR_OK)
	{
		printf("%s: open error %d: %s\r\n", filename, res, ff_strerror(res));
		return res;
	}

	res = f_read(&in, data, len, &br);
	if (res != FR_OK || br != len)
	{
		printf("%s: read error %d: %s (bytes read=%d/%ld)\r\n",
			filename, res, ff_strerror(res), br, (long)len);
	}

	f_close(&in);

	return res;
}

FRESULT f_write_file(char *filename, void *data, size_t len)
{
	FRESULT res = FR_OK;  /* API result code */
	FIL out;              /* File object */
	UINT bw;          /* Bytes written */

	res = f_open(&out, filename, FA_CREATE_ALWAYS | FA_WRITE);
	if (res != FR_OK)
	{
		printf("%s: open error %d: %s\r\n", filename, res, ff_strerror(res));
		return res;
	}

	res = f_write(&out, data, len, &bw);
	if (res != FR_OK || bw != len)
	{
		printf("%s: write error %d: %s (bytes written=%d/%ld)\r\n",
			filename, res, ff_strerror(res), bw, (long)len);
	}

	f_close(&out);

	return res;
}
