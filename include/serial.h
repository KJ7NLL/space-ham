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
#include "linklist.h"

void initUsart0(void);
void USART1_RX_IRQHandler(void);
void USART1_TX_IRQHandler(void);
void serial_write(void *s, int len);
void serial_read(void *s, int len);
int serial_read_timeout(void *s, int len, float timeout);
int serial_read_line(char *s, int len);
int serial_read_done();
void serial_read_async(void *s, int len);
void serial_read_async_cancel();
void print(char *s);
void print_back(char *s);
void print_lots(char c, int n);
void print_bs(int n);
void print_back_more(char *s, char c, int n);
int esc_key(char **keys);
int input(char *buf, int len, struct linklist **history);
