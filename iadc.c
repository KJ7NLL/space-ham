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
#include <math.h>

#include "platform.h"

#include "iadc.h"
#include "rotor.h"

static volatile uint16_t scan_results[IADC_NUM_INPUTS][IADC_NUM_AVG];	// Raw value from ADC, 0-4095
static volatile uint16_t sample_position[IADC_NUM_INPUTS];
static volatile uint32_t scan_total[IADC_NUM_INPUTS];

void initIADC(void)
{
	// Declare init structs
	IADC_Init_t init = IADC_INIT_DEFAULT;
	IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
	IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;
	IADC_ScanTable_t initScanTable = IADC_SCANTABLE_DEFAULT;

	printf("IADC: scan_results is %d bytes, sample_position is %d bytes\r\n", 
		sizeof(scan_results), sizeof(sample_position));

	memset((void *)scan_results, 0, sizeof(scan_results));
	memset((void *)sample_position, 0, sizeof(sample_position));
	memset((void *)scan_total, 0, sizeof(scan_total));

	// Enable IADC0 and GPIO clock branches
	CMU_ClockEnable(cmuClock_IADC0, true);

	// Reset IADC to reset configuration in case it has been modified by
	// other code
	IADC_reset(IADC0);

	// FSRCO - 20MHz
	CMU_ClockSelectSet(cmuClock_IADCCLK, cmuSelect_FSRCO);

	// Modify init structs and initialize
	init.warmup = iadcWarmupKeepWarm;

	// Set the HFSCLK prescale value here
	init.srcClkPrescale = IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

	// Configuration 0 is used by both scan and single conversions by
	// default
	// Use unbuffered AVDD as reference
	initAllConfigs.configs[0].reference = iadcCfgReferenceVddx;

	// Divides CLK_SRC_ADC to set the CLK_ADC frequency
	initAllConfigs.configs[0].adcClkPrescale =
		IADC_calcAdcClkPrescale(IADC0, CLK_ADC_FREQ, 0,
					iadcCfgModeNormal,
					init.srcClkPrescale);

	// Scan initialization Set the SCANFIFODVL flag when 4 entries in the scan
	// FIFO are valid; Not used as an interrupt or to wake up LDMA; flag is ignored
	initScan.dataValidLevel = _IADC_SCANFIFOCFG_DVL_VALID2;

	// Tag FIFO entry with scan table entry id.
	initScan.showId = true;

	// Configure entries in scan table

	// Theta
	initScanTable.entries[0].posInput = IADC_INPUT_0;
	initScanTable.entries[0].includeInScan = true;
	initScanTable.entries[0].negInput = iadcNegInputGnd;

	// Phi
	initScanTable.entries[1].posInput = IADC_INPUT_1;
	initScanTable.entries[1].includeInScan = true;
	initScanTable.entries[1].negInput = iadcNegInputGnd;

	// UNUSED
	initScanTable.entries[2].posInput = IADC_INPUT_2;
	initScanTable.entries[2].includeInScan = true;
	initScanTable.entries[2].negInput = iadcNegInputGnd;

	// Telescope
	initScanTable.entries[3].posInput = IADC_INPUT_3;
	initScanTable.entries[3].includeInScan = true;
	initScanTable.entries[3].negInput = iadcNegInputGnd;

	// Initialize IADC
	IADC_init(IADC0, &init, &initAllConfigs);

	// Initialize Scan
	IADC_initScan(IADC0, &initScan, &initScanTable);

	// Allocate the analog bus for ADC0 inputs
	// Note that "IADC_INPUT_0_BUS" is a #define in iadc.h
	GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC;
	GPIO->IADC_INPUT_1_BUS |= IADC_INPUT_1_BUSALLOC;
	GPIO->IADC_INPUT_2_BUS |= IADC_INPUT_2_BUSALLOC;
	GPIO->IADC_INPUT_3_BUS |= IADC_INPUT_3_BUSALLOC;

	// Clear any previous interrupt flags
	IADC_clearInt(IADC0, _IADC_IF_MASK);

	// Enable Scan interrupts
	IADC_enableInt(IADC0, IADC_IEN_SCANTABLEDONE);

	// Enable ADC interrupts
	NVIC_ClearPendingIRQ(IADC_IRQn);
	NVIC_EnableIRQ(IADC_IRQn);

	IADC_command(IADC0, iadcCmdStartScan);
}

void IADC_IRQHandler(void)
{
	IADC_Result_t result = { 0, 0 };

	// Get ADC results
	int i = 0;
	while (IADC_getScanFifoCnt(IADC0))
	{
		// Read data from the scan FIFO
		result = IADC_pullScanFifoResult(IADC0);

		if (i < NUM_ROTORS &&
			(!rotor_online(&rotors[i]) ||
				rotors[i].adc_type != ADC_TYPE_INTERNAL))
		{
			i++;
			continue;
		}

		// Calculate input voltage: For single-ended the result range is 0 to +Vref, i.e.,
		// for Vref = AVDD = 3.30V, 12 bits represents 3.30V full scale IADC range.
		// Really we should use result.id instead of i, but for some reason, result.id is always 0.
		// Hopefully the scan results are always in order (they seem to be).

		int j = sample_position[i];
		int v = result.data & 0xFFF;

		scan_total[i] -= scan_results[i][j];
		scan_total[i] += v;
		scan_results[i][j] = v;
		sample_position[i]++;
		sample_position[i] &= IADC_NUM_AVG_MASK;
		i++;
	}

	// Start next IADC conversion
	IADC_clearInt(IADC0, IADC_IF_SCANTABLEDONE);

	IADC_command(IADC0, iadcCmdStartScan);
}

float iadc_get_result(int i)
{
	// Disable the interrupt to prevent the results from changing while we operate
	NVIC_DisableIRQ(IADC_IRQn);

	// Sum the results to get an average
	int result = scan_total[i] >> IADC_NUM_AVG_BITS;
	NVIC_EnableIRQ(IADC_IRQn);
	
	return (float)((double) 3.3 * result / 4095.0);
}
