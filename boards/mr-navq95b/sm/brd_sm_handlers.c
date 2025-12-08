/*
** ###################################################################
**
** Copyright 2023-2025 NXP
**
** Redistribution and use in source and binary forms, with or without modification,
** are permitted provided that the following conditions are met:
**
** o Redistributions of source code must retain the above copyright notice, this list
**   of conditions and the following disclaimer.
**
** o Redistributions in binary form must reproduce the above copyright notice, this
**   list of conditions and the following disclaimer in the documentation and/or
**   other materials provided with the distribution.
**
** o Neither the name of the copyright holder nor the names of its
**   contributors may be used to endorse or promote products derived from this
**   software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
** ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
** WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
** DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
** ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
** (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
** LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
** ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
** SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**
**
** ###################################################################
*/

/*==========================================================================*/
/* File containing the implementation of the handlers for the board.        */
/*==========================================================================*/

/* Includes */

#include "sm.h"
#include "brd_sm.h"
#include "dev_sm.h"
#include "fsl_iomuxc.h"
#include "fsl_lpi2c.h"
#include "fsl_rgpio.h"

/* Local defines */

/* I2C device addresses */
#define BOARD_PF09_DEV_ADDR         0x08U
#define BOARD_PCAL6416A_DEV_ADDR    0x20U
#define BOARD_PF5301_DEV_ADDR       0x2AU
#define BOARD_PF5302_DEV_ADDR       0x29U
#define BOARD_PCA2131_DEV_ADDR      0x53U

#define PCAL6408A_INPUT_PF53_ARM_PG  1U
#define PCAL6408A_INPUT_PF53_SOC_PG  2U
#define PCAL6408A_INPUT_PF09_INT     3U
#define PCAL6408A_INPUT_PCA2131_INT  6U

/* Local types */

/* Local variables */

/* Global variables */

PCAL6416A_Type pcal6416a_i2c2_0x20;
PF09_Type g_pf09Dev;
PF53_Type g_pf5301Dev;
PF53_Type g_pf5302Dev;
PCA2131_Type g_pca2131Dev;

irq_prio_info_t g_brdIrqPrioInfo[BOARD_NUM_IRQ_PRIO_IDX] =
{
    [BOARD_IRQ_PRIO_IDX_GPIO1_0] =
    {
        .irqId = GPIO1_0_IRQn,
        .irqCntr = 0U,
        .basePrio = 0U,
        .dynPrioEn = false
    }
};

bool g_pca2131Used = false;

uint32_t g_pmicFaultFlags = 0U;

/* Local functions */

static void BRD_SM_Pf09Handler(void);
static int32_t BRD_SM_SerialDevicesInWakeupDomainInit(void);

/*--------------------------------------------------------------------------*/
/* Init serial devices                                                      */
/*--------------------------------------------------------------------------*/
int32_t BRD_SM_SerialDevicesInit(void)
{
    int32_t status = SM_ERR_SUCCESS;
    LPI2C_Type *const s_i2cBases[] = LPI2C_BASE_PTRS;
    uint16_t io;

    pcal6416a_config_t pcal6416Config;

    /* Fill in pcal6416a_i2c2_0x20 dev */
    pcal6416a_i2c2_0x20.i2cBase = s_i2cBases[BOARD_I2C_INSTANCE];
    pcal6416a_i2c2_0x20.devAddr = BOARD_PCAL6416A_DEV_ADDR;

    /* Init the bus expander for pcal6416a_i2c2_0x20 */
    PCAL6416A_GetDefaultConfig(&pcal6416Config);
    pcal6416Config.inputLatch = 0xFFFF;
    pcal6416Config.direction = 0xFFFF; /* All inputs */
    
    status = PCAL6416A_Init(&pcal6416a_i2c2_0x20, &pcal6416Config) ? SM_ERR_SUCCESS : SM_ERR_HARDWARE_ERROR;
    if (status == SM_ERR_SUCCESS)
    {
        status = PCAL6416A_IntMaskSet(&pcal6416a_i2c2_0x20, PCAL6416A_INITIAL_MASK) ? SM_ERR_SUCCESS : SM_ERR_HARDWARE_ERROR;
    }

    PCAL6416A_InputGet(&pcal6416a_i2c2_0x20, &io);
    printf("pcal6416a_i2c2_0x20 input  values: 0x%04x\n", io);

    if (status == SM_ERR_SUCCESS)
    {
        /* Fill in PF09 PMIC handle */
        g_pf09Dev.i2cBase = s_i2cBases[BOARD_I2C_INSTANCE];
        g_pf09Dev.devAddr = BOARD_PF09_DEV_ADDR;
        g_pf09Dev.crcEn = true;

        /* Initialize PF09 PMIC */
        if (!PF09_Init(&g_pf09Dev))
        {
            status = SM_ERR_HARDWARE_ERROR;
        }

        /* Disable voltage monitor 1 */
        if (status == SM_ERR_SUCCESS)
        {
            if (!PF09_MonitorEnable(&g_pf09Dev, PF09_VMON1, false))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Disable voltage monitor 2 */
        if (status == SM_ERR_SUCCESS)
        {
            if (!PF09_MonitorEnable(&g_pf09Dev, PF09_VMON2, false))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Disable the PWRUP interrupt */
        if (status == SM_ERR_SUCCESS)
        {
            const uint8_t mask[PF09_MASK_LEN] =
            {
                [PF09_MASK_IDX_STATUS1] = 0x08U
            };

            if (!PF09_IntEnable(&g_pf09Dev, mask, PF09_MASK_LEN, false))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Change the LDO3 sequence */
        if (status == SM_ERR_SUCCESS)
        {
            if (!PF09_PmicWrite(&g_pf09Dev, 0x4AU, 0x1EU, 0xFFU))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Set the LDO3 OV bypass */
        if (status == SM_ERR_SUCCESS)
        {
            if (!PF09_PmicWrite(&g_pf09Dev, 0x7FU, 0xFCU, 0xFFU))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Enable the LDO3 in RUN mode */
        if (status == SM_ERR_SUCCESS)
        {
            if (!PF09_PmicWrite(&g_pf09Dev, 0x7DU, 0x20U, 0xFFU))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Set the OV debounce to 50us due to errata ER011/12 */
        if (status == SM_ERR_SUCCESS)
        {
            if (!PF09_PmicWrite(&g_pf09Dev, 0x37U, 0x94U, 0xFFU))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Save and clear any fault flags */
        if (status == SM_ERR_SUCCESS)
        {
            if (!PF09_FaultFlags(&g_pf09Dev, &g_pmicFaultFlags, true))
            {
                status = SM_ERR_HARDWARE_ERROR;
            }
        }

        /* Configure I/O expander on I/O board */
        if (status == SM_ERR_SUCCESS)
        {
            (void)BRD_SM_SerialDevicesInWakeupDomainInit();
        }

        /* Handle any already pending PF09 interrupts */
        if (status == SM_ERR_SUCCESS)
        {
            BRD_SM_Pf09Handler();
        }
    }

    if (status == SM_ERR_SUCCESS)
    {
        /* Fill in PF5301 PMIC handle */
        g_pf5301Dev.i2cBase = s_i2cBases[BOARD_I2C_INSTANCE];
        g_pf5301Dev.devAddr = BOARD_PF5301_DEV_ADDR;

        /* Initialize PF5301 PMIC */
        if (!PF53_Init(&g_pf5301Dev))
        {
            status = SM_ERR_HARDWARE_ERROR;
        }

    }

    if (status == SM_ERR_SUCCESS)
    {
        /* Fill in PF5302 PMIC handle */
        g_pf5302Dev.i2cBase = s_i2cBases[BOARD_I2C_INSTANCE];
        g_pf5302Dev.devAddr = BOARD_PF5302_DEV_ADDR;

        /* Initialize PF5302 PMIC */
        if (!PF53_Init(&g_pf5302Dev))
        {
            status = SM_ERR_HARDWARE_ERROR;
        }
    }

    if (status == SM_ERR_SUCCESS)
    {
        /* Fill in PCA2131 RTC handle */
        g_pca2131Dev.i2cBase = s_i2cBases[BOARD_I2C_INSTANCE];
        g_pca2131Dev.devAddr = BOARD_PCA2131_DEV_ADDR;

        /* Initialize PCA2131 RTC */
        if (!PCA2131_Init(&g_pca2131Dev))
        {
            status = SM_ERR_HARDWARE_ERROR;
        }
    }

    if (status == SM_ERR_SUCCESS)
    {
        rgpio_pin_config_t gpioConfig =
        {
            kRGPIO_DigitalInput,
            0U
        };

        /* Init GPIO1-10 */
        RGPIO_PinInit(GPIO1, 10U, &gpioConfig);
        RGPIO_SetPinInterruptConfig(GPIO1, 10U, kRGPIO_InterruptOutput0,
            kRGPIO_InterruptLogicZero);
    }

    /* Return status */
    return status;
}

/*--------------------------------------------------------------------------*/
/* Set bus expander interrupt mask                                          */
/*--------------------------------------------------------------------------*/
int32_t BRD_SM_BusExpMaskSet(uint8_t val, uint8_t mask)
{
    int32_t status = SM_ERR_SUCCESS;

    /* Return status */
    return status;
}

/*--------------------------------------------------------------------------*/
/* GPIO1 handler                                                            */
/*--------------------------------------------------------------------------*/
void GPIO1_0_IRQHandler(void)
{
    uint32_t flags;

    /* Get GPIO status */
    flags = RGPIO_GetPinsInterruptFlags(GPIO1, kRGPIO_InterruptOutput0);

    /* Clear GPIO interrupts */
    RGPIO_ClearPinsInterruptFlags(GPIO1, kRGPIO_InterruptOutput0, flags);

    /* Adjust dynamic IRQ priority */
    (void) DEV_SM_IrqPrioUpdate();
}

/*==========================================================================*/

/*--------------------------------------------------------------------------*/
/* PF09 handler                                                             */
/*--------------------------------------------------------------------------*/
static void BRD_SM_Pf09Handler(void)
{
    uint8_t stat[PF09_MASK_LEN] = { 0 };

    /* Read status of interrupts */
    (void) PF09_IntStatus(&g_pf09Dev, stat, PF09_MASK_LEN);

    /* Clear pending */
    (void) PF09_IntClear(&g_pf09Dev, stat, PF09_MASK_LEN);

    /* Handle pending temp interrupts */
    if ((stat[PF09_MASK_IDX_STATUS2] & 0x0FU) != 0U)
    {
        BRD_SM_SensorHandler();
    }
}

static int32_t BRD_SM_SerialDevicesInWakeupDomainInit(void)
{
    int32_t status = SM_ERR_SUCCESS;
    PCAL6416A_Type pcal6416a_i2c4_0x20 = {LPI2C4, BOARD_PCAL6416A_DEV_ADDR};
    lpi2c_master_config_t lpi2cConfig;
    pcal6416a_config_t pcal6416Config;
    uint16_t io;

    /* Enable I2C4 */
    LPI2C_MasterGetDefaultConfig(&lpi2cConfig);
    lpi2cConfig.baudRate_Hz = BOARD_I2C_BAUDRATE;
    lpi2cConfig.enableDoze = false;

    LPI2C_MasterInit(LPI2C4, &lpi2cConfig, U64_U32(CCM_RootGetRate(CLOCK_ROOT_LPI2C4)));

    /* Init the bus expander for pcal6416a_i2c4_0x20 */
    PCAL6416A_GetDefaultConfig(&pcal6416Config);
    pcal6416Config.inputLatch = 0xFFFF;
    pcal6416Config.direction = 0x4740; /* inputs: CAN*_ERR_N and nc's  | outputs: CAN*_EN, CAN*_STB_N, CAN_PWR_EN, LED_* and SW_LED_OUT */

    status = PCAL6416A_Init(&pcal6416a_i2c4_0x20, &pcal6416Config) ? SM_ERR_SUCCESS : SM_ERR_HARDWARE_ERROR;

    if (status == SM_ERR_SUCCESS)
    {
        status = PCAL6416A_IntMaskSet(&pcal6416a_i2c4_0x20, PCAL6416A_INITIAL_MASK) ? SM_ERR_SUCCESS : SM_ERR_HARDWARE_ERROR;
    }

    if (status == SM_ERR_SUCCESS)
    {
        /* Enable CAN busses and LEDs */
        status = PCAL6416A_OutputSet(&pcal6416a_i2c4_0x20, 0xB8BF) ? SM_ERR_SUCCESS : SM_ERR_HARDWARE_ERROR;
    }

    if (status == SM_ERR_SUCCESS)
    {
        PCAL6416A_InputGet(&pcal6416a_i2c4_0x20, &io);
        printf("pcal6416a_i2c4_0x20 input  values: 0x%04x\n", io);
        PCAL6416A_OutputGet(&pcal6416a_i2c4_0x20, &io);
        printf("pcal6416a_i2c4_0x20 output values: 0x%04x\n", io);
    }
    else
    {
        printf("pcal6416a_i2c4_0x20 not found\n");
    }

    LPI2C_MasterDeinit(LPI2C4);

    return status;
}
