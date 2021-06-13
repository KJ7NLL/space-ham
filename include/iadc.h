// Set CLK_ADC to 10MHz
#define CLK_SRC_ADC_FREQ          10000000	// CLK_SRC_ADC
#define CLK_ADC_FREQ              10000000	// CLK_ADC - 10MHz max in

// Number of scan channels
#define IADC_NUM_INPUTS 2

// When changing GPIO port/pins below, make sure to change xBUSALLOC macro's
// accordingly.
#define IADC_INPUT_0_BUS          ABUSALLOC
#define IADC_INPUT_0_BUSALLOC     GPIO_ABUSALLOC_AEVEN0_ADC0
#define IADC_INPUT_1_BUS          ABUSALLOC
#define IADC_INPUT_1_BUSALLOC     GPIO_ABUSALLOC_AODD0_ADC0

#define IADC_INPUT_0              iadcPosInputPortAPin3
#define IADC_INPUT_1              iadcPosInputPortAPin4

void initIADC(void);
float iadc_get_result(int i);
