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

#include <stdio.h>

#include "platform.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lvgl_port.h"
#include "lvgl.h"
#include "esp_lcd_panel_vendor.h"

#include "i2c.h"

#define LCD_PIXEL_CLOCK_HZ    (400 * 1000)
#define PIN_NUM_RST           -1
#define LCD_HW_ADDR           0x3C

// The pixel number in horizontal and vertical
#define LCD_H_RES              128
#define LCD_V_RES              64
// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

void init_lcd()
{
	esp_lcd_panel_io_handle_t io_handle = NULL;

	esp_lcd_panel_io_i2c_config_t io_config = {
		.dev_addr = LCD_HW_ADDR,
		.scl_speed_hz = LCD_PIXEL_CLOCK_HZ,
		// According to SSD1306 datasheet
		.control_phase_bytes = 1,
		.lcd_cmd_bits = LCD_CMD_BITS,
		.lcd_param_bits = LCD_CMD_BITS,
		.dc_bit_offset = 6,
	};
	ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c
			(i2c_get_bus_handle(), &io_config, &io_handle));

	esp_lcd_panel_handle_t panel_handle = NULL;

	esp_lcd_panel_dev_config_t panel_config = {
		.bits_per_pixel = 1,
		.reset_gpio_num = PIN_NUM_RST,
	};
	esp_lcd_panel_ssd1306_config_t ssd1306_config = {
		.height = LCD_V_RES,
	};

	panel_config.vendor_config = &ssd1306_config;
	ESP_ERROR_CHECK(esp_lcd_new_panel_ssd1306
			(io_handle, &panel_config, &panel_handle));

	ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
	ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
	ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));

	const lvgl_port_cfg_t lvgl_cfg = ESP_LVGL_PORT_INIT_CONFIG();

	lvgl_port_init(&lvgl_cfg);

	const lvgl_port_display_cfg_t disp_cfg = {
		.io_handle = io_handle,
		.panel_handle = panel_handle,
		.buffer_size = LCD_H_RES * LCD_V_RES,
		.double_buffer = true,
		.hres = LCD_H_RES,
		.vres = LCD_V_RES,
		.monochrome = true,
		.rotation = {
			     .swap_xy = false,
			     .mirror_x = false,
			     .mirror_y = false,
			     }
	};
	lv_disp_t *disp = lvgl_port_add_disp(&disp_cfg);

	/*
	   Rotation of the screen 
	 */
	lv_disp_set_rotation(disp, LV_DISP_ROT_180);

	// Lock the mutex due to the LVGL APIs are not thread-safe
	if (lvgl_port_lock(0))
	{
		lv_obj_t *scr = lv_disp_get_scr_act(disp);
		lv_obj_t *label = lv_label_create(scr);

		lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);	/* Circular 
										   scroll 
										 */
		lv_label_set_text(label, "Hello Espressif, Hello LVGL.");
		/*
		   Size of the screen (if you use rotation 90 or 270, please
		   set disp->driver->ver_res) 
		 */
		lv_obj_set_width(label, disp->driver->hor_res);
		lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
		// Release the mutex
		lvgl_port_unlock();
	}
}
