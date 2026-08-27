// PLL related Defines
#define SYSCTL_RIS_R          (*((volatile unsigned long *)0x400FE050))	
#define SYSCTL_RCC_R          (*((volatile unsigned long *)0x400FE060))
#define SYSCTL_RCC2_R         (*((volatile unsigned long *)0x400FE070))	

// SysTick related Defines	
#define NVIC_ST_CTRL_R        (*((volatile unsigned long *)0xE000E010))
#define NVIC_ST_RELOAD_R      (*((volatile unsigned long *)0xE000E014))
#define NVIC_ST_CURRENT_R     (*((volatile unsigned long *)0xE000E018))

// ************** SysTick_Init **************
void SysTick_Init(void){
  NVIC_ST_CTRL_R = 0;            // disable SysTick during setup
  NVIC_ST_RELOAD_R = 0x00FFFFFF; // maximum reload value
  NVIC_ST_CURRENT_R = 0;         // any write to CURRENT clears it             
  NVIC_ST_CTRL_R = 0x00000005;   // enable SysTick with core clock, no interrupt
}

// ************** PLL_Init (?????) **************
// freqMHz: ?????????(?? MHz)
// ??:SysClk = 400MHz / (divider + 1)
void PLL_Init(unsigned long freqMHz){
  unsigned long divider;

  // 0) Use RCC2
  SYSCTL_RCC2_R |=  0x80000000;  // USERCC2
  // 1) bypass PLL while initializing
  SYSCTL_RCC2_R |=  0x00000800;  // BYPASS2, PLL bypass
  // 2) select the crystal value and oscillator source
  SYSCTL_RCC_R = (SYSCTL_RCC_R &~0x000007C0)   // clear XTAL field, bits 10-6
                 + 0x00000540;                 // 10101, configure for 16 MHz crystal
  SYSCTL_RCC2_R &= ~0x00000070;                // configure for main oscillator source
  // 3) activate PLL by clearing PWRDN
  SYSCTL_RCC2_R &= ~0x00002000;

  // 4) set the desired system divider, based on freqMHz (in MHz)
  if(freqMHz == 0){
    freqMHz = 50;                              // default to 50 MHz if 0 is passed
  }
  if(freqMHz > 400){
    freqMHz = 400;                             // limit to max PLL frequency
  }

  // SysClk = 400MHz / (divider + 1)
  divider = (400 / freqMHz) - 1;               // integer divider
  if(divider > 0x7F){
    divider = 0x7F;                            // SYSDIV2 field is 7 bits
  }

  SYSCTL_RCC2_R |= 0x40000000;                 // use 400 MHz PLL
  SYSCTL_RCC2_R = (SYSCTL_RCC2_R & ~0x1FC00000)// clear system clock divider
                  + (divider << 22);           // set system clock divider

  // 5) wait for the PLL to lock by polling PLLLRIS
  while((SYSCTL_RIS_R & 0x00000040) == 0){};   // wait for PLLRIS bit
  // 6) enable use of PLL by clearing BYPASS
  SYSCTL_RCC2_R &= ~0x00000800;
}

// ************** SysTick_Wait **************
// delay: ?????“???????”
// ?????? = delay / ??????
// ??:50MHz ?,1 ??? = 20ns;delay=500000 ?? 10ms
void SysTick_Wait(unsigned long delay){
  NVIC_ST_RELOAD_R = delay - 1;      // number of counts to wait
  NVIC_ST_CURRENT_R = 0;             // any value written to CURRENT clears
  while((NVIC_ST_CTRL_R & 0x00010000) == 0){ // wait for count flag
  }
}
