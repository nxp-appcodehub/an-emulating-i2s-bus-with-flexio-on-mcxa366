/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017,2019 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
#include <stdio.h>
#include <stdlib.h>
/*${standard_header_anchor}*/

#include "fsl_device_registers.h"
#include "fsl_debug_console.h"
#include "pin_mux.h"
#include "clock_config.h"
#include "board.h"

#include "fsl_flexio_i2s.h"


#include "fsl_edma.h"
#include "fsl_flexio_i2s_edma.h"


/*******************************************************************************
 * Definitions
 ******************************************************************************/
/* SAI and I2C instance and clock */
#define DEMO_I2C         LPI2C2
#define DEMO_FLEXIO_BASE FLEXIO0

///* Get frequency of lpi2c clock */
#define DEMO_I2C_CLK_FREQ CLOCK_GetRootClockFreq(kCLOCK_Root_Lpi2c0102)

#define FLEXIO_CLOCK_FREQUENCY CLOCK_GetFlexioClkFreq()
#define DEMO_FLEXIO_CLK_FREQ CLOCK_GetFlexioClkFreq()

#define BCLK_PIN       (10U)
#define FRAME_SYNC_PIN (8U)
#define TX_DATA_PIN    (9U)
#define RX_DATA_PIN    (28U)
#define MCLK_PIN       (14U)

#define RX_BCLK_PIN       (27U)
#define RX_FRAME_SYNC_PIN (13U)
#define RX_MCLK_PIN       (14U)


#define OVER_SAMPLE_RATE (384)
#define BUFFER_SIZE      (1024)
#define BUFFER_NUM       (4)
#define PLAY_COUNT       (50000 * 2U)
#define ZERO_BUFFER_SIZE (BUFFER_SIZE * 2)
/* demo audio sample rate */
#define DEMO_AUDIO_SAMPLE_RATE (kFLEXIO_I2S_SampleRate48KHz)
/* demo audio master clock */
#if (defined FSL_FEATURE_SAI_HAS_MCLKDIV_REGISTER && FSL_FEATURE_SAI_HAS_MCLKDIV_REGISTER) || \
    (defined FSL_FEATURE_PCC_HAS_SAI_DIVIDER && FSL_FEATURE_PCC_HAS_SAI_DIVIDER)
#define DEMO_AUDIO_MASTER_CLOCK OVER_SAMPLE_RATE *DEMO_AUDIO_SAMPLE_RATE
#else
#define DEMO_AUDIO_MASTER_CLOCK DEMO_SAI_CLK_FREQ
#endif
/* demo audio data channel */
#define DEMO_AUDIO_DATA_CHANNEL (2U)
/* demo audio bit width */
#define DEMO_AUDIO_BIT_WIDTH (kFLEXIO_I2S_WordWidth16bits)
      
#define DEMO_DMA_BASEADDR DMA0
#define DEMO_DMA_CHANNEL_TX   0U
#define DEMO_DMA_CHANNEL_RX   1U
#define APP_DMA_IRQ          DMA_CH0_IRQn
#define APP_DMA_IRQ_HANDLER  DMA_CH0_IRQHandler
      
#define BOARD_SW_GPIO        BOARD_SW2_GPIO
#define BOARD_SW_GPIO_PIN    BOARD_SW2_GPIO_PIN
#define BOARD_SW_NAME        BOARD_SW2_NAME
#define BOARD_SW_IRQ         BOARD_SW2_IRQ
#define BOARD_SW_IRQ_HANDLER BOARD_SW2_IRQ_HANDLER
      
/*******************************************************************************
 * Prototypes
 ******************************************************************************/
void BOARD_InitHardware(void);
void Audio_FlexioI2SInit(void);

static void txCallback(FLEXIO_I2S_Type *i2sBase, flexio_i2s_handle_t *handle, status_t status, void *userData);
static void rxCallback(FLEXIO_I2S_Type *i2sBase, flexio_i2s_handle_t *handle, status_t status, void *userData);
/*******************************************************************************
 * Variables
 ******************************************************************************/
flexio_i2s_transfer_t s_TxTransfer;
AT_NONCACHEABLE_SECTION_INIT(flexio_i2s_edma_handle_t txHandle) = {0};
AT_NONCACHEABLE_SECTION_INIT(flexio_i2s_edma_handle_t rxHandle) = {0};
static volatile bool isTxFinished = false;
static volatile bool isRxFinished = false;
AT_NONCACHEABLE_SECTION_ALIGN(uint8_t audioBuff[BUFFER_SIZE * BUFFER_NUM], 4);

static volatile uint32_t beginCount   = 0;
static volatile uint32_t sendCount    = 0;
static volatile uint32_t receiveCount = 0;
static volatile uint8_t emptyBlock    = 0;
static volatile bool isZeroBuffer     = true;
FLEXIO_I2S_Type s_base;
FLEXIO_I2S_Type s_base_rx;

edma_handle_t txDmaHandle;
edma_handle_t rxDmaHandle;

flexio_i2s_transfer_t txXfer, rxXfer;
static uint32_t tx_index = 0U, rx_index = 0U;
AT_NONCACHEABLE_SECTION_ALIGN(static uint8_t Buffer[BUFFER_NUM * BUFFER_SIZE], 4);


/* Whether the SW button is pressed */
volatile bool g_ButtonPress = false;

/*******************************************************************************
 * Code
 ******************************************************************************/
void DelayMS(uint32_t ms)
{
    for (uint32_t i = 0; i < ms; i++)
    {
        SDK_DelayAtLeastUs(1000, SystemCoreClock);
    }
}

static void rx_callback(FLEXIO_I2S_Type *i2sBase, flexio_i2s_edma_handle_t *handle, status_t status, void *userData)
{
    emptyBlock--;
}

static void tx_callback(FLEXIO_I2S_Type *i2sBase, flexio_i2s_edma_handle_t *handle, status_t status, void *userData)
{
    emptyBlock++;
}



void FLEXIO_I2S_Init_FOR_MCLK(FLEXIO_I2S_Type *base, const flexio_i2s_config_t *config)
{
	flexio_timer_config_t timerConfig     = {0};
	uint8_t mclkTimerIndex = 2;
	if (config->masterSlave == kFLEXIO_I2S_Master)
	{
		timerConfig.triggerSelect   = FLEXIO_TIMER_TRIGGER_SEL_SHIFTnSTAT(base->txShifterIndex);
		timerConfig.triggerPolarity = kFLEXIO_TimerTriggerPolarityActiveLow;
		timerConfig.triggerSource   = kFLEXIO_TimerTriggerSourceInternal;
		timerConfig.pinSelect       = MCLK_PIN;//base->bclkPinIndex;
		timerConfig.pinConfig       = kFLEXIO_PinConfigOutput;
		timerConfig.pinPolarity     = kFLEXIO_PinActiveHigh;
		timerConfig.timerMode       = kFLEXIO_TimerModeSingle16Bit;
		timerConfig.timerOutput     = kFLEXIO_TimerOutputOneNotAffectedByReset;
		timerConfig.timerDecrement  = kFLEXIO_TimerDecSrcOnFlexIOClockShiftTimerOutput;
		timerConfig.timerReset      = kFLEXIO_TimerResetNever;
		timerConfig.timerDisable    = kFLEXIO_TimerDisableNever;
		timerConfig.timerEnable     = kFLEXIO_TimerEnableOnTriggerHigh;
		timerConfig.timerStart      = kFLEXIO_TimerStartBitEnabled;
		timerConfig.timerStop       = kFLEXIO_TimerStopBitDisabled;
	}
	FLEXIO_SetTimerConfig(base->flexioBase, mclkTimerIndex, &timerConfig);

    /* Set Frame sync timer cmp */
	base->flexioBase->TIMCMP[mclkTimerIndex] = FLEXIO_TIMCMP_CMP(1 - 1U);/*--10-12M...*/
}


void Audio_FlexioI2SInit(void)
{
    flexio_i2s_config_t config;
    flexio_i2s_format_t format;

    dma_request_source_t dma_request_source_tx;
    dma_request_source_t dma_request_source_rx;
    edma_config_t userConfig;

    /* Set flexio i2s pin, shifter and timer */
    s_base.bclkPinIndex   = BCLK_PIN;
    s_base.fsPinIndex     = FRAME_SYNC_PIN;
    s_base.txPinIndex     = TX_DATA_PIN;
    s_base.rxPinIndex     = RX_DATA_PIN;
    s_base.txShifterIndex = 0;
    s_base.rxShifterIndex = 1;
    s_base.bclkTimerIndex = 0;
    s_base.fsTimerIndex   = 1;
    s_base.flexioBase     = DEMO_FLEXIO_BASE;
    
    dma_request_source_tx = kDma0RequestMuxFlexIO0ShiftRegister0Request;
    dma_request_source_rx = kDma0RequestMuxFlexIO0ShiftRegister1Request;

    /*
     * config.enableI2S = true;
     */
    FLEXIO_I2S_GetDefaultConfig(&config);

    config.masterSlave = kFLEXIO_I2S_Master;
    config.bclkPinPolarity = kFLEXIO_PinActiveLow;
    FLEXIO_I2S_Init(&s_base, &config);
    FLEXIO_I2S_Init_FOR_MCLK(&s_base, &config);

    /* Configure the audio format */
    format.bitWidth      = DEMO_AUDIO_BIT_WIDTH;
    format.sampleRate_Hz = DEMO_AUDIO_SAMPLE_RATE;

    /* Configure EDMA channel for one shot transfer */
    EDMA_GetDefaultConfig(&userConfig);
    EDMA_Init(DEMO_DMA_BASEADDR, &userConfig);
    EDMA_CreateHandle(&txDmaHandle, DEMO_DMA_BASEADDR, DEMO_DMA_CHANNEL_TX);
    EDMA_CreateHandle(&rxDmaHandle, DEMO_DMA_BASEADDR, DEMO_DMA_CHANNEL_RX);

    EDMA_SetChannelMux(DMA0, DEMO_DMA_CHANNEL_TX, dma_request_source_tx);
    EDMA_SetChannelMux(DMA0, DEMO_DMA_CHANNEL_RX, dma_request_source_rx);

    FLEXIO_I2S_TransferTxCreateHandleEDMA(&s_base, &txHandle, tx_callback, NULL, &txDmaHandle);
    FLEXIO_I2S_TransferRxCreateHandleEDMA(&s_base, &rxHandle, rx_callback, NULL, &rxDmaHandle);

    FLEXIO_I2S_TransferSetFormatEDMA(&s_base, &txHandle, &format, CLOCK_GetFlexioClkFreq());
    FLEXIO_I2S_TransferSetFormatEDMA(&s_base, &rxHandle, &format, CLOCK_GetFlexioClkFreq());

}

/*!
 * @brief Interrupt service fuction of switch.
 *
 * This function toggles the LED
 */
void BOARD_SW_IRQ_HANDLER(void)
{
#if (defined(FSL_FEATURE_PORT_HAS_NO_INTERRUPT) && FSL_FEATURE_PORT_HAS_NO_INTERRUPT) || \
    (!defined(FSL_FEATURE_SOC_PORT_COUNT))
    /* Clear external interrupt flag. */
    GPIO_GpioClearInterruptFlags(BOARD_SW_GPIO, 1U << BOARD_SW_GPIO_PIN);
#else
    /* Clear external interrupt flag. */
    GPIO_PortClearInterruptFlags(BOARD_SW_GPIO, 1U << BOARD_SW_GPIO_PIN);
#endif
    /* Change state of button. */
    g_ButtonPress = true;
    SDK_ISR_EXIT_BARRIER;
}





/*!
 * @brief Application task function.
 *
 * This function runs the task for application.
 *
 * @return None.
 */
#if defined(__CC_ARM) || (defined(__ARMCC_VERSION)) || defined(__GNUC__)
int main(void)
#else
void main(void)
#endif
{       
    /* Define the init structure for the input switch pin */
    gpio_pin_config_t gpio_config = {
        kGPIO_DigitalOutput,
        0,
    };
    CLOCK_SetClockDiv(kCLOCK_DivFLEXIO0, 1u);
    CLOCK_AttachClk(kFRO_HF_to_FLEXIO0);

    /* Release peripheral reset */
    RESET_ReleasePeripheralReset(kFLEXIO0_RST_SHIFT_RSTn);
    CLOCK_EnableClock(kCLOCK_GateFLEXIO0);
    
    CLOCK_SetClockDiv(kCLOCK_DivLPI2C2, 1u);
    CLOCK_AttachClk(kFRO_LF_DIV_to_LPI2C2);
    
    BOARD_InitHardware();
    
    CLOCK_SetupExtClocking(8000000U);
    /*!< Set up PLL0 */
    const pll_setup_t pll1Setup = {
        .pllctrl = SCG_SPLLCTRL_SOURCE(0U) | SCG_SPLLCTRL_LIMUPOFF_MASK  | SCG_SPLLCTRL_SELI(4U) | SCG_SPLLCTRL_SELP(3U) | SCG_SPLLCTRL_SELR(4U),
        .pllndiv = SCG_SPLLNDIV_NDIV(1U),
        .pllpdiv = SCG_SPLLPDIV_PDIV(6U),
        .pllsscg = {(SCG_SPLLSSCG0_SS_MDIV_LSB(1236950581U)),((SCG0->SPLLSSCG1 & ~SCG_SPLLSSCG1_SS_PD_MASK) | SCG_SPLLSSCG1_SS_MDIV_MSB(0U) | (uint32_t)(kSS_MF_512) | (uint32_t)(kSS_MR_K1) | (uint32_t)(kSS_MC_NOC) | SCG_SPLLSSCG1_SEL_SS_MDIV_MASK)},
        .pllRate = 24575999U
    };

    CLOCK_SetPLL1Freq(&pll1Setup);

    /* attach PLL1 to FLEXIO */
    CLOCK_SetClockDiv(kCLOCK_DivFLEXIO0, 1u);
    CLOCK_SetClockDiv(kCLOCK_DivPLL1CLK, 1u);
    CLOCK_AttachClk(kPll1ClkDiv_to_FLEXIO0);

    PRINTF("FLEXIO_I2S example started!\n\r");
    
    Audio_FlexioI2SInit();

    while (1)
    {
        if (emptyBlock > 0)
        {
        	rxXfer.data     = Buffer + rx_index * BUFFER_SIZE;
        	rxXfer.dataSize = BUFFER_SIZE;
            if (kStatus_Success == FLEXIO_I2S_TransferReceiveEDMA(&s_base, &rxHandle, &rxXfer))
            {
                rx_index++;
            }
            if (rx_index == BUFFER_NUM)
            {
                rx_index = 0U;
            }
        }
        if (emptyBlock < BUFFER_NUM)
        {
        	txXfer.data     = Buffer + tx_index * BUFFER_SIZE;
        	txXfer.dataSize = BUFFER_SIZE;
            if (kStatus_Success == FLEXIO_I2S_TransferSendEDMA(&s_base, &txHandle, &txXfer))
            {
                tx_index++;
            }
            if (tx_index == BUFFER_NUM)
            {
                tx_index = 0U;
            }
        }
    }
}
