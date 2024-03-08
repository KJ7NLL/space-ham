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
#include "esp_lcd_panel_vendor.h"

#include "i2c.h"
#include "lcd.h"

#include "astronomy.h"
#include "stars.h"

#define LCD_PIXEL_CLOCK_HZ    (400 * 1000)
#define PIN_NUM_RST           -1
#define LCD_HW_ADDR           0x3C

// The pixel number in horizontal and vertical
#define LCD_H_RES              128
#define LCD_V_RES              64
// Bit number used to represent command and parameter
#define LCD_CMD_BITS           8
#define LCD_PARAM_BITS         8

// Button bits for LCD screen
#define BUTTON_BIT_0_GPIO      20
#define BUTTON_BIT_1_GPIO      21
#define BUTTON_BIT_2_GPIO      22

lv_indev_t *indev_keypad;
lv_indev_drv_t indev_drv;
lv_disp_t *disp;

void init_lcd()
{
	static lv_indev_drv_t indev_drv;
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

	disp = lvgl_port_add_disp(&disp_cfg);

	// Rotation of the screen
	lv_disp_set_rotation(disp, LV_DISP_ROT_180);

	// setup keypad
	lv_indev_drv_init(&indev_drv);
	indev_drv.type = LV_INDEV_TYPE_KEYPAD;
	indev_drv.read_cb = keypad_read;
	indev_keypad = lv_indev_drv_register(&indev_drv);

	lvgl_menu();
}

button_status_enum_t get_button_status()
{
	button_status_enum_t button_status;

	button_status = (gpio_get_level(BUTTON_BIT_0_GPIO) << 0) |
		(gpio_get_level(BUTTON_BIT_1_GPIO) << 1) |
		(gpio_get_level(BUTTON_BIT_2_GPIO) << 2);

	return button_status;
}

static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
	static uint32_t last_key = 0;

	// Get the current x and y coordinates
	data->point.x = 0;
	data->point.y = 0;

	// Get whether the a key is pressed and save the pressed key
	uint32_t act_key = get_button_status();

	if (act_key != 0)
	{
		data->state = LV_INDEV_STATE_PRESSED;

		// Translate the keys to LVGL control characters according to
		// your key definitions
		switch (act_key)
		{
			case BUTTON_UP:
				act_key = LV_KEY_PREV;
				break;

			case BUTTON_DOWN:
				act_key = LV_KEY_NEXT;
				break;

			case BUTTON_LEFT:
				act_key = LV_KEY_LEFT;
				break;

			case BUTTON_RIGHT:
				act_key = LV_KEY_RIGHT;
				break;

			case BUTTON_OK:
				act_key = LV_KEY_ENTER;
				break;
		}
		last_key = act_key;
		printf("button num: %d\r\n", get_button_status());
	}
	else
	{
		data->state = LV_INDEV_STATE_RELEASED;
	}
        data->key = last_key;
}

void ev_label_scroll_cb(lv_event_t * e)
{
	//star_t *star = lv_event_get_user_data(e);
	lv_event_code_t code = lv_event_get_code(e);
	lv_obj_t *menu_cont = lv_event_get_target(e);
	lv_obj_t *label = lv_obj_get_child(menu_cont, 0);

	if (code == LV_EVENT_FOCUSED)
	{
		lv_label_set_long_mode(label, LV_LABEL_LONG_SCROLL_CIRCULAR);
		lv_obj_set_style_anim_speed(label, 25, LV_PART_MAIN);
		lv_obj_set_width(label, 128);
	}
	else if (code == LV_EVENT_DEFOCUSED)
	{
		lv_label_set_long_mode(label, LV_LABEL_LONG_CLIP);
	}
}

void menu_item(lv_group_t *group, lv_obj_t *menu, lv_obj_t *page, lv_obj_t *sub_page,
	lv_style_t *style, char *menu_name)
{
	lv_obj_t *cont;
	lv_obj_t *label;

	// Main menu init
	cont = lv_menu_cont_create(page);
	lv_obj_add_style(cont, style, LV_STATE_DEFAULT);
	lv_obj_add_style(cont, style, LV_STATE_FOCUS_KEY);

	lv_group_add_obj(group, cont);
	lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
	lv_obj_add_flag(cont, LV_OBJ_FLAG_SCROLL_ON_FOCUS);

	label = lv_label_create(cont);

	lv_label_set_text(label, menu_name);
	lv_obj_add_event_cb(cont, ev_label_scroll_cb, LV_EVENT_FOCUSED, NULL);
	lv_obj_add_event_cb(cont, ev_label_scroll_cb, LV_EVENT_DEFOCUSED, NULL);

	const lv_font_t *font = lv_obj_get_style_text_font(page,
		LV_PART_ITEMS); // or LV_PART_MAIN?
	lv_coord_t menuList_h = lv_font_get_line_height(font);

	lv_obj_set_size(page, LV_PCT(100), menuList_h);

	if (sub_page != NULL)
		lv_menu_set_load_page_event(menu, cont, sub_page);
}

void lvgl_menu()
{
	static const astro_body_t body[] = {
		BODY_SUN, BODY_MOON, BODY_MERCURY, BODY_VENUS, BODY_EARTH, BODY_MARS,
		BODY_JUPITER, BODY_SATURN, BODY_URANUS, BODY_NEPTUNE, BODY_PLUTO,
	};

	int num_bodies = sizeof(body) / sizeof(body[0]);

	// Lock the mutex due to the LVGL APIs are not thread-safe
	if (lvgl_port_lock(0))
	{
		lv_obj_t *scr = lv_disp_get_scr_act(NULL);
		lv_obj_set_size(scr, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));

		lv_group_t *group = lv_group_create();
		lv_group_set_default(group);
		lv_indev_set_group(indev_keypad, group);

		static lv_style_t style;
		lv_style_init(&style);
		lv_style_set_bg_color(&style, lv_color_hex(0x000000));
		lv_style_set_border_width(&style, 0);
		lv_style_set_border_color(&style, lv_color_hex(0xff0000));
		lv_style_set_pad_all(&style, 1);
		lv_style_set_radius(&style, 0);

		static lv_style_t no_border;
		lv_style_init(&no_border);

		lv_style_set_border_width(&no_border, 0);

		lv_style_set_pad_all(&no_border, 0);
		lv_style_set_radius(&no_border, 0);

		lv_obj_add_style(scr, &no_border, LV_STATE_DEFAULT);

		static lv_coord_t col[] = {LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
		static lv_coord_t row[] = {8, LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

		lv_obj_t *grid = lv_obj_create(scr);
		lv_obj_add_style(grid, &no_border, LV_STATE_DEFAULT);

		lv_obj_set_size(grid, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
		lv_obj_center(grid);
		lv_obj_set_grid_dsc_array(grid, col, row);

		lv_obj_t *status_bar_label = lv_label_create(grid);
		lv_label_set_text(status_bar_label, "bar");
		lv_obj_center(status_bar_label);
		lv_obj_add_style(status_bar_label, &no_border, LV_STATE_DEFAULT);

		lv_obj_add_style(status_bar_label, &no_border, LV_STATE_DEFAULT);
		lv_obj_set_grid_cell(status_bar_label, LV_GRID_ALIGN_STRETCH, 0, 1,
			LV_GRID_ALIGN_STRETCH, 0, 1);

		lv_obj_t *menu = lv_menu_create(grid);
		lv_obj_set_grid_cell(menu, LV_GRID_ALIGN_STRETCH, 0, 1,
			LV_GRID_ALIGN_STRETCH, 1, 1);
		lv_obj_add_style(menu, &no_border, LV_STATE_DEFAULT);

		lv_obj_center(menu);

		// Create a main page
		lv_obj_t *main_page = lv_menu_page_create(menu, NULL);
		// Create a sub page
		lv_obj_t *sub_page_sat = lv_menu_page_create(menu, NULL);
		lv_obj_t *sub_page_planet = lv_menu_page_create(menu, NULL);
		lv_obj_t *sub_page_star = lv_menu_page_create(menu, NULL);

		menu_item(group, menu, sub_page_sat, NULL, &style, "ISS");

		int i;

		for (i = 0; i < num_bodies; i++)
		{
			menu_item(group, menu, sub_page_planet, NULL, &style, Astronomy_BodyName(body[i]));
		}

		for (i = 0; i < 5; i++)
		{
			menu_item(group, menu, sub_page_star, NULL, &style, stars[i].name);
		}

		menu_item(group, menu, main_page, sub_page_sat, &style, "Satellite");
		menu_item(group, menu, main_page, sub_page_planet, &style, "Planet");
		menu_item(group, menu, main_page, sub_page_star, &style, "Star");

		// MUST BE LAST MENU LINE
		lv_menu_set_page(menu, main_page);

		// Release the mutex
		lvgl_port_unlock();
	}
}
