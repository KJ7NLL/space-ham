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

#include "lvgl.h"

typedef enum {
	BUTTON_NONE,
	BUTTON_UP,
	BUTTON_RIGHT,
	BUTTON_DOWN,
	BUTTON_LEFT,
	BUTTON_OK,
	BUTTON_6,
	BUTTON_7,

	// Button combinations for secret things, :)
	BUTTON_LEFT_RIGHT = 6,
	BUTTON_LEFT_UP_RIGHT = 7,
	BUTTON_OK_RIGHT = 7,
	BUTTON_DOWN_LEFT = 7,
} button_status_enum_t;

esp_err_t init_lcd();
void lvgl_menu();
button_status_enum_t get_button_status();
static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);
