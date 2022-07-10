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
int timer_idx(TIMER_TypeDef *timer);

void timer_init_pwm(TIMER_TypeDef *timer, int cc, int port, int pin, int pwm_Hz, float duty_cycle);

void timer_enable(TIMER_TypeDef *timer);
void timer_disable(TIMER_TypeDef *timer);

void timer_irq_enable(TIMER_TypeDef *timer);
void timer_irq_disable(TIMER_TypeDef *timer);

int timer_cmu_clock(TIMER_TypeDef *timer);

void timer_cc_route(TIMER_TypeDef *timer, int cc, int port, int pin);
void timer_cc_route_clear(TIMER_TypeDef *timer, int cc);
void timer_cc_duty_cycle(TIMER_TypeDef *timer, int cc, float duty_cycle);

extern TIMER_TypeDef *TIMERS[4];
