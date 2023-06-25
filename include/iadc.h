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
// Set CLK_ADC to 10MHz
#define CLK_SRC_ADC_FREQ          10000000	// CLK_SRC_ADC
#define CLK_ADC_FREQ              10000000	// CLK_ADC - 10MHz max in

// Number of scan channels
#define IADC_NUM_INPUTS 4

// Number of samples to average (must be a power of 2)
#define IADC_NUM_AVG_BITS 8
#define IADC_NUM_AVG (1<<IADC_NUM_AVG_BITS)
#define IADC_NUM_AVG_MASK (IADC_NUM_AVG-1)

// When changing GPIO port/pins below, make sure to change xBUSALLOC macro's
// accordingly.
#define IADC_INPUT_0_BUS          CDBUSALLOC
#define IADC_INPUT_0_BUSALLOC     GPIO_CDBUSALLOC_CDEVEN0_ADC0
#define IADC_INPUT_1_BUS          CDBUSALLOC
#define IADC_INPUT_1_BUSALLOC     GPIO_CDBUSALLOC_CDODD0_ADC0
#define IADC_INPUT_2_BUS          ABUSALLOC
#define IADC_INPUT_2_BUSALLOC     GPIO_ABUSALLOC_AEVEN0_ADC0
#define IADC_INPUT_3_BUS          ABUSALLOC
#define IADC_INPUT_3_BUSALLOC     GPIO_ABUSALLOC_AODD0_ADC0

#define IADC_INPUT_0              iadcPosInputPortDPin2
#define IADC_INPUT_1              iadcPosInputPortDPin3
#define IADC_INPUT_2              iadcPosInputPortAPin4
#define IADC_INPUT_3              iadcPosInputPortAPin0

void initIADC(void);
float iadc_get_result(int i);
