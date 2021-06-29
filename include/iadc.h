// Set CLK_ADC to 10MHz
#define CLK_SRC_ADC_FREQ          10000000	// CLK_SRC_ADC
#define CLK_ADC_FREQ              10000000	// CLK_ADC - 10MHz max in

// Number of scan channels
#define IADC_NUM_INPUTS 2

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

#define IADC_INPUT_0              iadcPosInputPortDPin2
#define IADC_INPUT_1              iadcPosInputPortDPin3

void initIADC(void);
float iadc_get_result(int i);
