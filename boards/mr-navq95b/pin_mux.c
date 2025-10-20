/*
 * Copyright 2023, 2025 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "pin_mux.h"
#include "board.h"
#include "fsl_rgpio.h"

#define IOMUXC_GPR0_ADDRESS 0x443d0000

#define DID_ELE      0
#define DID_MTR_MSTR 1
#define DID_M33      2
#define DID_A55      3
#define DID_M7       4

/* FUNCTION ************************************************************************************************************
 *
 * Function Name : BOARD_InitPins
 * Description   : Configures pin routing and optionally pin electrical features.
 *
 * END ****************************************************************************************************************/
void BOARD_InitPins(void)
{
    *((volatile uint32_t *)IOMUXC_GPR0_ADDRESS) = (DID_ELE << 0) | (DID_M33 << 4) | (DID_M7 << 8) | (DID_MTR_MSTR << 12);

#if (BOARD_DEBUG_UART_INSTANCE == 1U)
    /* Configure LPUART 1 */
    IOMUXC_SetPinMux(IOMUXC_PAD_UART1_RXD__LPUART1_RX, 0U);
    IOMUXC_SetPinConfig(IOMUXC_PAD_UART1_RXD__LPUART1_RX, IOMUXC_PAD_PD(1U));

    IOMUXC_SetPinMux(IOMUXC_PAD_UART1_TXD__LPUART1_TX, 0);
    IOMUXC_SetPinConfig(IOMUXC_PAD_UART1_TXD__LPUART1_TX, IOMUXC_PAD_DSE(0xFU));
#elif (BOARD_DEBUG_UART_INSTANCE == 2U)
    /* Configure LPUART 2 */
    IOMUXC_SetPinMux(IOMUXC_PAD_UART2_RXD__LPUART2_RX, 0);
    IOMUXC_SetPinConfig(IOMUXC_PAD_UART2_RXD__LPUART2_RX, IOMUXC_PAD_PD(1U));

    IOMUXC_SetPinMux(IOMUXC_PAD_UART2_TXD__LPUART2_TX, 0);
    IOMUXC_SetPinConfig(IOMUXC_PAD_UART2_TXD__LPUART2_TX, IOMUXC_PAD_DSE(0xFU));
#elif (BOARD_DEBUG_UART_INSTANCE == 8U)
    /* Configure LPUART 8 */
    IOMUXC_SetPinMux(IOMUXC_PAD_GPIO_IO13__LPUART8_RX, 0);
    IOMUXC_SetPinConfig(IOMUXC_PAD_GPIO_IO13__LPUART8_RX, IOMUXC_PAD_PD(1U));

    IOMUXC_SetPinMux(IOMUXC_PAD_GPIO_IO12__LPUART8_TX, 0);
    IOMUXC_SetPinConfig(IOMUXC_PAD_GPIO_IO12__LPUART8_TX, IOMUXC_PAD_DSE(0xFU));
#endif

#if (BOARD_I2C_INSTANCE == 1U)
    /* Configure LPI2C 1 */
    IOMUXC_SetPinMux(IOMUXC_PAD_I2C1_SCL__LPI2C1_SCL, 1U);
    IOMUXC_SetPinConfig(IOMUXC_PAD_I2C1_SCL__LPI2C1_SCL, IOMUXC_PAD_DSE(0xFU)
        | IOMUXC_PAD_FSEL1(0x3U) | IOMUXC_PAD_PU(0x1U) | IOMUXC_PAD_OD(0x1U));

    IOMUXC_SetPinMux(IOMUXC_PAD_I2C1_SDA__LPI2C1_SDA, 1U);
    IOMUXC_SetPinConfig(IOMUXC_PAD_I2C1_SDA__LPI2C1_SDA, IOMUXC_PAD_DSE(0xFU)
        | IOMUXC_PAD_FSEL1(0x3U) | IOMUXC_PAD_PU(0x1U) | IOMUXC_PAD_OD(0x1U));
#elif (BOARD_I2C_INSTANCE == 2U)
    /* Configure LPI2C 2 */
    IOMUXC_SetPinMux(IOMUXC_PAD_I2C2_SCL__LPI2C2_SCL, 1U);
    IOMUXC_SetPinConfig(IOMUXC_PAD_I2C2_SCL__LPI2C2_SCL, IOMUXC_PAD_DSE(0xFU)
        | IOMUXC_PAD_FSEL1(0x3U) | IOMUXC_PAD_PU(0x1U) | IOMUXC_PAD_OD(0x1U));

    IOMUXC_SetPinMux(IOMUXC_PAD_I2C2_SDA__LPI2C2_SDA, 1U);
    IOMUXC_SetPinConfig(IOMUXC_PAD_I2C2_SDA__LPI2C2_SDA, IOMUXC_PAD_DSE(0xFU)
        | IOMUXC_PAD_FSEL1(0x3U) | IOMUXC_PAD_PU(0x1U) | IOMUXC_PAD_OD(0x1U));
#endif

    /* Configure GPIO1-10 (INT from the PCAL6408A) */
    IOMUXC_SetPinMux(IOMUXC_PAD_PDM_BIT_STREAM1__GPIO1_IO_BIT10, 0U);
    IOMUXC_SetPinConfig(IOMUXC_PAD_PDM_BIT_STREAM1__GPIO1_IO_BIT10, 0U);

    /* Set GPIO_IO24 in correct states for Startup */
    IOMUXC_SetPinMux(IOMUXC_PAD_GPIO_IO24__GPIO2_IO_BIT24, 0U);
    IOMUXC_SetPinConfig(IOMUXC_PAD_GPIO_IO24__GPIO2_IO_BIT24,
    IOMUXC_PAD_DSE(0xFU) | IOMUXC_PAD_PU(0x1U));

    rgpio_pin_config_t gpioConfig =
    {
        kRGPIO_DigitalOutput,
        1U
    };

    /* Init GPIO2-24 */
    RGPIO_PinInit(GPIO2, 24U, &gpioConfig);

}

