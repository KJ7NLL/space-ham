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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include "platform.h"

// fatfs includes
#include "ff.h"
#include "diskio.h"

FRESULT scan_files(char *path	/* Start node to be scanned (***also used as
				   work area***) */
	)
{
	FRESULT res;
#ifdef __ESP32__
	FF_DIR dir;
#else
	DIR dir;
#endif
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
