#include "RTE_Components.h"
#include CMSIS_device_header

#include <stdio.h>
#include <string.h>
#include "NuMicro.h"


int icache_enabled(){ return SCB->CCR & SCB_CCR_IC_Msk ? 1 : 0;}
int dcache_enabled(){ return SCB->CCR & SCB_CCR_DC_Msk ? 1 : 0;}
void dwt_enable(){DWT->CTRL |= 1;}

uint32_t LBMK_get_cpu_timestamp(){
	return DWT->CYCCNT;
}

void delay_at_least_cycles(uint32_t cycles){
    dwt_enable();
    const uint32_t start = LBMK_get_cpu_timestamp();
    const uint32_t stop = start + cycles;
    if(stop<start){
        while(LBMK_get_cpu_timestamp()> start);
    }
    while(LBMK_get_cpu_timestamp()< stop);
}
void delay_kcycles(uint32_t kcycles){
    delay_at_least_cycles(1000*kcycles);
}
void delay_mcycles(uint32_t mcycles){
    delay_kcycles(1000*mcycles);
}
#define SYS_FREQ_HZ FREQ_220MHZ
#define SYS_FREQ_MHZ (SYS_FREQ_HZ / 1000000)
void delay_ms(uint32_t ms){
    uint32_t cycles = ms * 1000 * SYS_FREQ_MHZ;
    delay_at_least_cycles(cycles);
} 
void SYS_Init(void){

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init System Clock                                                                                       */
    /*---------------------------------------------------------------------------------------------------------*/

    /* Enable Internal RC 12MHz clock */
    CLK_EnableXtalRC(CLK_SRCCTL_HIRCEN_Msk);

    /* Waiting for Internal RC clock ready */
    CLK_WaitClockReady(CLK_STATUS_HIRCSTB_Msk);

    /* Enable External RC 12MHz clock */
    CLK_EnableXtalRC(CLK_SRCCTL_HXTEN_Msk);

    /* Waiting for External RC clock ready */
    CLK_WaitClockReady(CLK_STATUS_HXTSTB_Msk);


    /* Enable PLL0 220MHz clock */
    CLK_EnableAPLL(CLK_APLLCTL_APLLSRC_HIRC, SYS_FREQ_HZ, CLK_APLL0_SELECT);

    /* Switch SCLK clock source to PLL0 and divide 1 */
    CLK_SetSCLK(CLK_SCLKSEL_SCLKSEL_APLL0);

    /* Set HCLK2 divide 2 */
    CLK_SET_HCLK2DIV(2);

    /* Set PCLKx divide 2 */
    CLK_SET_PCLK0DIV(2);
    CLK_SET_PCLK1DIV(2);
    CLK_SET_PCLK2DIV(2);
    CLK_SET_PCLK3DIV(2);
    CLK_SET_PCLK4DIV(4);

    /* Update System Core Clock */
    /* User can use SystemCoreClockUpdate() to calculate SystemCoreClock. */
    SystemCoreClockUpdate();

    /* Enable UART0 module clock */
    CLK_EnableModuleClock(UART0_MODULE);

    /* Debug UART clock setting*/
    SetDebugUartCLK();

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    SetDebugUartMFP();

}

void print_test_pmu(){
    __disable_irq();
    printf("SYS_IsRegLocked=0x%08x\r\n",SYS_IsRegLocked());
	PMU->CTRL = 1;//enable counters
	printf("PMU->CTRL     = 0x%08x\r\n",PMU->CTRL);
	PMU->CNTENSET = 1<<31;
	printf("PMU->CNTENSET = 0x%08x\r\n",PMU->CNTENSET);
	printf("PMU->CCNTR    = 0x%08x\r\n",PMU->CCNTR);
	printf("PMU->CCNTR    = 0x%08x\r\n",PMU->CCNTR);
    printf("SYS_IsRegLocked=0x%08x\r\n",SYS_IsRegLocked());
}

void delay_cnt_s(uint32_t seconds){
    for(unsigned int i=0;i<seconds;i++){
        volatile uint32_t cnt=0;
        for(cnt=0;cnt<1000*1000*10;cnt++);
    }
}

int main() {
    SYS_UnlockReg();
    SYS_Init();
    InitDebugUart();
    __disable_irq();
    printf("\r\nwait 5s\r\n"); 
    delay_cnt_s(5);
    printf("\r\nM5531 (%s %s)\r\n", __DATE__, __TIME__); 
    printf("ICache: %d, DCache: %d\r\n",icache_enabled(),dcache_enabled());
    printf("SYS_IsRegLocked=0x%08x\r\n",SYS_IsRegLocked());

    if(1){
        printf("DWT->CTRL=0x%08x\r\n",DWT->CTRL);
        printf("DWT->CYCCNT=0x%08x\r\n",DWT->CYCCNT);
        DWT->CTRL |= 1;//enable DWT cycle counter
        printf("DWT->CTRL=0x%08x\r\n",DWT->CTRL);
        printf("DWT->CYCCNT=0x%08x\r\n",DWT->CYCCNT);
        printf("DWT->CYCCNT=0x%08x\r\n",DWT->CYCCNT);
        printf("SYS_IsRegLocked=0x%08x\r\n",SYS_IsRegLocked());
        DWT->CTRL = 0;
        printf("DWT->CTRL=0x%08x\r\n",DWT->CTRL);
        printf("DWT->CYCCNT=0x%08x\r\n",DWT->CYCCNT);
        
        //print_test_pmu();
    }
    uint32_t i=0;
    while(1){
        delay_ms(1000);
        //delay_cnt_s(1);
        printf("Alive: %u\r\n",i++);
    }
}
