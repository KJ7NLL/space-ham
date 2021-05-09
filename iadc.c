#include <stdio.h>
#include "em_device.h"
#include "em_chip.h"
#include "em_cmu.h"
#include "em_iadc.h"
#include "em_gpio.h"

#include "iadc.h"

static volatile double scan_result[IADC_NUM_INPUTS];	// Volts

void initIADC(void)
{
	int i = 0;

	// Declare init structs
	IADC_Init_t init = IADC_INIT_DEFAULT;
	IADC_AllConfigs_t initAllConfigs = IADC_ALLCONFIGS_DEFAULT;
	IADC_InitScan_t initScan = IADC_INITSCAN_DEFAULT;
	IADC_ScanTable_t initScanTable = IADC_SCANTABLE_DEFAULT;

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
	init.srcClkPrescale =
		IADC_calcSrcClkPrescale(IADC0, CLK_SRC_ADC_FREQ, 0);

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
	initScanTable.entries[0].posInput = iadcPosInputPortDPin2;
	initScanTable.entries[0].includeInScan = true;
	initScanTable.entries[0].negInput = iadcNegInputGnd;

	// Phi
	initScanTable.entries[1].posInput = iadcPosInputPortDPin3;
	initScanTable.entries[1].includeInScan = true;
	initScanTable.entries[1].negInput = iadcNegInputGnd;

	// Initialize IADC
	IADC_init(IADC0, &init, &initAllConfigs);

	// Initialize Scan
	IADC_initScan(IADC0, &initScan, &initScanTable);

	// Allocate the analog bus for ADC0 inputs
	GPIO->IADC_INPUT_0_BUS |= IADC_INPUT_0_BUSALLOC;
	GPIO->IADC_INPUT_1_BUS |= IADC_INPUT_1_BUSALLOC;

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

	GPIO_PinOutSet(gpioPortB, 0);
	// Get ADC results
	int i = 0;
	while (IADC_getScanFifoCnt(IADC0))
	{
		// Read data from the scan FIFO
		result = IADC_pullScanFifoResult(IADC0);

		// Calculate input voltage: For single-ended the result range is 0 to +Vref, i.e.,
		// for Vref = AVDD = 3.30V, 12 bits represents 3.30V full scale IADC range.
		// Really we should use result.id instead of i, but for some reason, result.id is always 0.
		// Hopefully the scan results are always in order (they seem to be).
		scan_result[i] = result.data * 3.3 / 0xFFF;
		i++;
	}

	// Start next IADC conversion
	IADC_clearInt(IADC0, IADC_IF_SCANTABLEDONE);

	IADC_command(IADC0, iadcCmdStartScan);
}

double iadc_get_result(int i)
{
	double r;

	NVIC_DisableIRQ(IADC_IRQn);
	r = scan_result[i];
	NVIC_EnableIRQ(IADC_IRQn);
	
	return r;
}
