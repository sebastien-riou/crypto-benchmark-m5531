#include "RTE_Components.h"
#include CMSIS_device_header

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "NuMicro.h"


int icache_enabled(){ return SCB->CCR & SCB_CCR_IC_Msk ? 1 : 0;}
int dcache_enabled(){ return SCB->CCR & SCB_CCR_DC_Msk ? 1 : 0;}
/* Armv8-M puts the DWT behind two gates that firmware, not just a debugger, has to open:
   DEMCR.TRCENA clocks the unit, and the optional software lock (LAR/LSR at DWT_BASE+0xFB0,
   not modelled by CMSIS for the M55) has to be unlocked first if it is implemented. Writing
   LAR is harmless when it is not.
   This has to be re-assertable rather than one-shot: a debugger detaching clears DEMCR
   underneath us -- pyocd writes DEMCR = 0 in its disconnect sequence, roughly a second after
   the reset that started us, i.e. in the middle of our boot code. */
#define DWT_LAR_REG (*(volatile uint32_t*)(DWT_BASE + 0xFB0UL))
#define DWT_LAR_KEY 0xC5ACCE55UL

void dwt_enable(){
    DWT_LAR_REG = DWT_LAR_KEY;
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

uint32_t LBMK_get_cpu_timestamp(){
	return DWT->CYCCNT;
}

/* 1 if CYCCNT is actually counting. Cheap enough to call from a self-test, not from a loop. */
int dwt_cyccnt_alive(){
    const uint32_t start = DWT->CYCCNT;
    for(volatile unsigned int i=0;i<64;i++);
    return DWT->CYCCNT != start;
}

/* Fallback delay that does not touch the DWT at all. The loop body costs several cycles at -Og,
   so dividing by 4 overshoots -- which is what "at least" asks for. */
static void busy_delay_cycles(uint32_t cycles){
    volatile uint32_t n = (cycles / 4u) + 1u;
    while(n--);
}

void delay_at_least_cycles(uint32_t cycles){
    dwt_enable();
    const uint32_t start = LBMK_get_cpu_timestamp();
    const uint32_t stop = start + cycles;
    /* One poll costs more than one cycle, so a working counter always reaches `stop` in fewer
       than `cycles` iterations. Expiring this guard means CYCCNT froze on us. */
    uint32_t guard = (cycles > 0xFFFFF000u) ? 0xFFFFFFFFu : (cycles + 1000u);
    if(stop<start){
        while(LBMK_get_cpu_timestamp()> start){
            if(0 == --guard) goto stalled;
        }
    }
    while(LBMK_get_cpu_timestamp()< stop){
        if(0 == --guard) goto stalled;
    }
    return;
stalled:
    /* Most likely DEMCR.TRCENA was cleared under us. Re-open the gates for whoever comes next,
       then honour this delay without the DWT so we never hang here. */
    dwt_enable();
    busy_delay_cycles(cycles);
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

    /* Enable GPIOB module clock: the debug UART pins are PB12 (RXD) and PB13 (TXD), and on this
       family each port has its own clock gate (CLK->GPIOCTL, reset value 0). SetDebugUartMFP()
       only writes SYS->GPB_MFP3, so without this the input path of PB12 stays dead: TX works,
       RX never sees anything. */
    CLK_EnableModuleClock(GPIOB_MODULE);

    /*---------------------------------------------------------------------------------------------------------*/
    /* Init I/O Multi-function                                                                                 */
    /*---------------------------------------------------------------------------------------------------------*/
    SetDebugUartMFP();

    /* Pull up PB12 so the RX line idles high when nothing drives it (a floating RX pin looks like
       a break/garbage to the receiver). */
    PB->PUSEL = (PB->PUSEL & ~GPIO_PUSEL_PUSEL12_Msk) | (GPIO_PUSEL_PULL_UP << GPIO_PUSEL_PUSEL12_Pos);
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
#define xstr(s) str(s)
#define str(s) #s
void lean_benchmark(unsigned int ninfo, const char*info[], bool run_forever);
#define STDIN  0x00
#define STDOUT 0x01 //WARNING: only for GCC (see retarget_GCC.c), other compilers may use another value!
#define STDERR 0x02
#define FILEHANDLE int
int _write(FILEHANDLE fh, const unsigned char *buf, unsigned int len, int mode);
void com_tx(const void *const buf, unsigned int size){
    const uint8_t*buf8 = (const uint8_t*)buf;
    for(unsigned int i=0;i<size;i++){
        //SendChar_ToUART(buf8[i]);//change \n into \r\n
        while (DEBUG_PORT->FIFOSTS & UART_FIFOSTS_TXFULL_Msk) {}
        DEBUG_PORT->DAT = (uint32_t)buf8[i];
    }
    //_write(STDOUT,buf,size,0);//change \n into \r\n
}
void com_rx(void *const buf, unsigned int size){
    uint8_t*buf8 = (uint8_t*)buf;
    for(unsigned int i=0;i<size;i++){
        while ((DEBUG_PORT->FIFOSTS & UART_FIFOSTS_RXEMPTY_Msk));
        buf8[i] = (uint8_t)DEBUG_PORT->DAT;
    }
}
/* UART_Open() leaves the RX FIFO untouched and never clears the sticky error flags, and the pins
   are muxed before it runs. Drop whatever the floating line clocked in during boot. */
void com_init(void){
    DEBUG_PORT->FIFO |= UART_FIFO_RXRST_Msk;
    while (DEBUG_PORT->FIFO & UART_FIFO_RXRST_Msk);
    DEBUG_PORT->FIFOSTS = UART_FIFOSTS_RXOVIF_Msk
                        | UART_FIFOSTS_BIF_Msk
                        | UART_FIFOSTS_FEF_Msk
                        | UART_FIFOSTS_PEF_Msk;
}
/* printf() goes through LBMK_putchar()/lean-com framing, so it is not readable in a plain
   terminal. This one writes straight to the UART, for diagnostics before any handshake. */
__attribute__((format(printf,1,2)))
void dbg_printf(const char*fmt, ...){
    char buf[128];
    va_list args;
    va_start(args,fmt);
    int n = vsnprintf(buf,sizeof(buf),fmt,args);
    va_end(args);
    if(n > 0) com_tx(buf, (unsigned int)(n > (int)sizeof(buf) ? (int)sizeof(buf) : n));
}
/* --- Hard fault reporting -------------------------------------------------------------------
   Overrides the pack's __WEAK ProcessHardFault (StdDriver/src/retarget.c:424). Two problems
   with the pack's version in this project:
     - The pack's HardFault_Handler (startup_M5531.c:570) IGNORES the return value and simply
       returns from the exception, so the faulting instruction re-executes and the analysis is
       reported forever. Over lean-com that is an endless stream of PRINT packets: the host
       never reaches the data it is waiting for, so a crash presents as a hang.
     - It reports with printf(), which needs newlib's vsnprintf and a few hundred bytes of
       stack at the exact moment the stack may be the reason we faulted.
   This version formats without newlib and reports through LBMK_print_impl(), which is
   lean-com framed once the handshake has run and raw before it, so it is correct on either
   side of LBMK_init_leancom() with no flag to keep in sync. Then it stops, so the fault is
   reported exactly once. */
void LBMK_print_impl(const char*msg);

static char*fault_hex(char*p, uint32_t v){
    static const char hex[] = "0123456789ABCDEF";
    *p++ = '0'; *p++ = 'x';
    for(int i=28;i>=0;i-=4) *p++ = hex[(v>>i)&0xF];
    return p;
}
static char*fault_str(char*p, const char*s){
    while(*s) *p++ = *s++;
    return p;
}
uint32_t ProcessHardFault(uint32_t *pu32StackFrame){
    char buf[256];
    char*p = buf;
    p = fault_str(p, "\r\nEXCEPTION: HardFault\r\n  PC=");
    p = fault_hex(p, pu32StackFrame[6]);
    p = fault_str(p, " LR=");
    p = fault_hex(p, pu32StackFrame[5]);
    p = fault_str(p, " xPSR=");
    p = fault_hex(p, pu32StackFrame[7]);
    p = fault_str(p, "\r\n  CFSR=");
    p = fault_hex(p, SCB->CFSR);
    p = fault_str(p, " HFSR=");
    p = fault_hex(p, SCB->HFSR);
    p = fault_str(p, "\r\n  BFAR=");
    p = fault_hex(p, SCB->BFAR);
    p = fault_str(p, " MMFAR=");
    p = fault_hex(p, SCB->MMFAR);
    p = fault_str(p, "\r\n  halted, any results above are invalid\r\n");
    *p = 0;
    LBMK_print_impl(buf);
    /* Do not return: the handler would resume at the faulting instruction and fault again. */
    while(1);
}

int LBMK_putchar(int ch);

int _write(FILEHANDLE fh, const unsigned char *buf, unsigned int len, int mode)
{
    (void)mode;

    switch (fh)
    {
        case STDOUT:
        case STDERR:
        {
            for(unsigned int i=0;i<len;i++){
                LBMK_putchar(buf[i]);
            }

            return len;
        }

        default:
            return EOF;
    }
}

void LBMK_init_leancom();

static volatile uint64_t heap_usage;
void LBMK_init_heap_usage(){
  heap_usage = 0;
}
uint64_t LBMK_get_heap_usage(){
  return heap_usage;
}

/* Set to 1 to dump the UART/GPIO state and watch the RX line instead of running the benchmark.
   Read it with a plain terminal (115200 8N1), no host tool needed. */
/* 1 dumps the UART/GPIO state and echoes typed characters over a plain terminal instead of
   running the benchmark; 0 runs the benchmark. */
#define RX_DIAGNOSTIC 0

static void rx_diagnostic(void){
    dbg_printf("\r\n--- M5531 debug UART RX diagnostic (%s %s) ---\r\n", __DATE__, __TIME__);
    dbg_printf("CLK->GPIOCTL   = 0x%08x (bit1 = GPIOB, must be set)\r\n", CLK->GPIOCTL);
    dbg_printf("SYS->GPB_MFP3  = 0x%08x (expect 0x0606 in bits [12:0])\r\n", SYS->GPB_MFP3);
    dbg_printf("PB->PUSEL      = 0x%08x (expect 0x01 in bits [25:24])\r\n", PB->PUSEL);
    dbg_printf("PB->DINOFF     = 0x%08x (bit28 = PB12 input disable, must be clear)\r\n", PB->DINOFF);
    dbg_printf("UART0->FUNCSEL = 0x%08x (expect 0)\r\n", DEBUG_PORT->FUNCSEL);
    dbg_printf("UART0->LINE    = 0x%08x (expect 0x03 = 8N1)\r\n", DEBUG_PORT->LINE);
    dbg_printf("UART0->BAUD    = 0x%08x (expect 0x30000066)\r\n", DEBUG_PORT->BAUD);
    dbg_printf("Now type in the terminal: PB12 must idle at 1 and dip to 0 on each character.\r\n");
    for(unsigned int i=0;i<40;i++){
        uint32_t fifosts = DEBUG_PORT->FIFOSTS;
        dbg_printf("t=%02u PB12=%u FIFOSTS=0x%08x rxempty=%u rxovif=%u bif=%u fef=%u\r\n",
                   i,
                   (unsigned int)((PB->PIN >> 12) & 1),
                   fifosts,
                   (unsigned int)((fifosts & UART_FIFOSTS_RXEMPTY_Msk) ? 1 : 0),
                   (unsigned int)((fifosts & UART_FIFOSTS_RXOVIF_Msk) ? 1 : 0),
                   (unsigned int)((fifosts & UART_FIFOSTS_BIF_Msk) ? 1 : 0),
                   (unsigned int)((fifosts & UART_FIFOSTS_FEF_Msk) ? 1 : 0));
        while(0 == (DEBUG_PORT->FIFOSTS & UART_FIFOSTS_RXEMPTY_Msk)){
            uint8_t c = (uint8_t)DEBUG_PORT->DAT;
            dbg_printf("     RX 0x%02x\r\n", c);
        }
        delay_ms(250);
    }
    dbg_printf("--- end of diagnostic ---\r\n");
}

int main() {
    (void)rx_diagnostic;
    SYS_UnlockReg();
    SYS_Init();
    InitDebugUart();
    com_init();
    
#if RX_DIAGNOSTIC
    if(1){
        unsigned int i=0;
        while(1){
            dbg_printf("%2u: waiting for new char\r\n",i);
            unsigned int c=0;
            com_rx(&c,1);
            dbg_printf("%2u: received: 0x%02x '%c'\r\n",i,c,(char)c);
            i++;
        }
    }
    rx_diagnostic();
    while(1);
#endif
    __disable_irq();
    dbg_printf("M5531 tx works!\r\n");//allow to know if we have UART TX problem or not when leancom does not connect.
    LBMK_init_leancom();
    printf("\r\nM5531 (%s %s)\r\n", __DATE__, __TIME__); 
    printf("ICache: %d, DCache: %d\r\n",icache_enabled(),dcache_enabled());
    printf("SYS_IsRegLocked=0x%08x\r\n",SYS_IsRegLocked());

    if(1){
        printf("trying to access DWT, if nothing appears immediatly, the device is stuck.\n");
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
        
        print_test_pmu();
    }

    dwt_enable();
    if(!dwt_cyccnt_alive()){
        /* printf(), not dbg_printf(): once LBMK_init_leancom() has run, every byte on the wire
           must be lean-com framed. dbg_printf() writes raw to the UART and would desynchronise
           the host's packet parser. */
        printf("WARNING: DWT CYCCNT is not counting, all timing measurements will read 0\r\n");
    }

    if(0){
        uint32_t i=0;
        while(1){
            delay_ms(1000);
            //delay_cnt_s(1);
            printf("Alive: %u\r\n",i++);
        }
    } else {
        const char* icache_str = icache_enabled() ? "enabled" : "disabled";
        const char* dcache_str = dcache_enabled() ? "enabled" : "disabled";
        char frequency_mhz[10] = {0};
        sprintf(frequency_mhz,"%lu",SYS_FREQ_MHZ);
	    const char*hw_info[] = {
            "hw_platform", "m5531",
            "frequency_mhz", frequency_mhz,
            "ICACHE", icache_str,
            "DCACHE", dcache_str
        };
        lean_benchmark(sizeof(hw_info)/sizeof(char*),hw_info,0);
        /* The startup self-test only proves the counter was alive then. A debugger attaching and
           detaching mid-run powers the debug domain down again, which freezes CYCCNT silently and
           turns every measurement above into a constant. Say so rather than let the numbers pass
           for real ones. */
        if(!dwt_cyccnt_alive()){
            printf("WARNING: DWT CYCCNT stopped counting during the run, results above are invalid\r\n");
        }
        while(1);
    }
}
