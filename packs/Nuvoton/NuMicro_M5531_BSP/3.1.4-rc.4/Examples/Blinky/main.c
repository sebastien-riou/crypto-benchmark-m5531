/**************************************************************************//**
 * @file    main.c
 * @version V1.00
 * @brief   Blinky example with VIO
 *
 * @copyright SPDX-License-Identifier: Apache-2.0
 * @copyright (C) 2025 Nuvoton Technology Corp. All rights reserved.
 *****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include "NuMicro.h"
#include "cmsis_vio.h"

#define LONG_DURATION  (500000U)
#define SHORT_DURATION (100000U)

volatile uint32_t g_Toggle_Duration = LONG_DURATION;
volatile uint32_t g_Toggle_State = 0;

void SysTick_Handler(void)
{
    static uint32_t s_Tick_Count = 0;
    s_Tick_Count++;

    if (s_Tick_Count >= g_Toggle_Duration)
    {
        if (g_Toggle_State)
        {
            vioSetSignal(vioLED1, vioLEDon);
            vioSetSignal(vioLED2, vioLEDoff);
        }
        else
        {
            vioSetSignal(vioLED1, vioLEDoff);
            vioSetSignal(vioLED2, vioLEDon);
        }

        s_Tick_Count = 0;
        g_Toggle_State ^= 1;
    }
}

static void SYS_Init(void)
{
    /* Unlock protected registers */
    SYS_UnlockReg();
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/
    /* Enable PLL0 220MHz clock from HIRC and switch SCLK clock source to APLL0 */
    CLK_SetBusClock(CLK_SCLKSEL_SCLKSEL_APLL0, CLK_APLLCTL_APLLSRC_HIRC, FREQ_220MHZ);
    /* Use SystemCoreClockUpdate() to calculate and update SystemCoreClock. */
    SystemCoreClockUpdate();
    /* Enable UART module clock */
    SetDebugUartCLK();
    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    SetDebugUartMFP();
    /* Lock protected registers */
    SYS_LockReg();
}

int main(void)
{
    uint32_t state = 0;
    uint32_t last0 = 0U, last1 = 0U;
    /* Init System, IP clock and multi-function I/O */
    SYS_Init();
    /* Init Debug UART to 115200-8N1 for print message */
    InitDebugUart();
#if defined (__GNUC__) && !defined(__ARMCC_VERSION) && defined(OS_USE_SEMIHOSTING)
    initialise_monitor_handles();
#endif
    vioInit();
    vioSetSignal(vioLED1, vioLEDoff);
    vioSetSignal(vioLED2, vioLEDoff);
    CLK_EnableSysTick(CLK_STSEL_ST0SEL_ACLK_DIV2, CyclesPerUs);
    printf("\n\nCPU @ %dHz\n", SystemCoreClock);
    printf("+-------------------------------------------------+\n");
    printf("|    Blinky example with VIO                      |\n");
    printf("+-------------------------------------------------+\n");
    printf("  >> press vioBUTTON0 to toggle vioLED1 and vioLED2 periodically with LONG_DURATION (%d) << \n", LONG_DURATION);
    printf("  >> press vioBUTTON1 to toggle vioLED1 and vioLED2 periodically with SHORT_DURATION (%d) << \n\n", SHORT_DURATION);

    while (1)
    {
        state = (vioGetSignal(vioBUTTON0));

        if (state != last0)
        {
            if (state == vioBUTTON0)
            {
                g_Toggle_Duration = LONG_DURATION;
                printf("vioBUTTON0 pressed, set toggle duration to LONG_DURATION\n");
            }
            else
            {
                printf("vioBUTTON0 released.\n");
            }

            last0 = state;
        }

        state = (vioGetSignal(vioBUTTON1));

        if (state != last1)
        {
            if (state == vioBUTTON1)
            {
                g_Toggle_Duration = SHORT_DURATION;
                printf("vioBUTTON1 pressed, set toggle duration to SHORT_DURATION\n");
            }
            else
            {
                printf("vioBUTTON1 released.\n");
            }

            last1 = state;
        }
    }
}
