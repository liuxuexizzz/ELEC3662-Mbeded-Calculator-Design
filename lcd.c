#include "lcd.h"
#include <stdint.h>

/*
 * 4-bit mode LCD driver (HD44780 compatible) - ELEC3662 wiring:
 *   RS = PA2
 *   R/W = PA3  (this driver keeps it LOW -> write-only)
 *   E  = PA4
 *   DB7..DB4 = PB7..PB4  (upper nibble)
 *
 * NOTE (Keil/ARMCC C90):
 *  Declarations must appear before executable statements in each block.
 */

// ======================= Pin mapping =======================
#define LCD_RS      0x04u   // PA2
#define LCD_RW      0x08u   // PA3
#define LCD_EN      0x10u   // PA4

#define LCD_DB_SHIFT 4     // 0 -> PB0..PB3, 4 -> PB4..PB7
#define LCD_DB_MASK  (0x0Fu << LCD_DB_SHIFT)

// ================== System Control Registers =================
#define SYSCTL_RCGCGPIO_R       (*((volatile uint32_t *)0x400FE608))
#define SYSCTL_PRGPIO_R         (*((volatile uint32_t *)0x400FEA08))

// ==================== GPIO Port A Registers ==================
#define GPIO_PORTA_DATA_R       (*((volatile uint32_t *)0x400043FC))
#define GPIO_PORTA_DIR_R        (*((volatile uint32_t *)0x40004400))
#define GPIO_PORTA_AFSEL_R      (*((volatile uint32_t *)0x40004420))
#define GPIO_PORTA_DEN_R        (*((volatile uint32_t *)0x4000451C))
#define GPIO_PORTA_AMSEL_R      (*((volatile uint32_t *)0x40004528))
#define GPIO_PORTA_PCTL_R       (*((volatile uint32_t *)0x4000452C))

// ==================== GPIO Port B Registers ==================
#define GPIO_PORTB_DATA_R       (*((volatile uint32_t *)0x400053FC))
#define GPIO_PORTB_DIR_R        (*((volatile uint32_t *)0x40005400))
#define GPIO_PORTB_AFSEL_R      (*((volatile uint32_t *)0x40005420))
#define GPIO_PORTB_DEN_R        (*((volatile uint32_t *)0x4000551C))
#define GPIO_PORTB_AMSEL_R      (*((volatile uint32_t *)0x40005528))
#define GPIO_PORTB_PCTL_R       (*((volatile uint32_t *)0x4000552C))

// ====================== SysTick Registers ====================
#define NVIC_ST_CTRL_R          (*((volatile uint32_t *)0xE000E010))
#define NVIC_ST_RELOAD_R        (*((volatile uint32_t *)0xE000E014))
#define NVIC_ST_CURRENT_R       (*((volatile uint32_t *)0xE000E018))

/*
 * Delay base clock assumption:
 * - The handout suggests using PLL at 80MHz.
 * - If you don't enable PLL (default ~16MHz), the delays will simply be longer (safe for LCD).
 */
#ifndef LCD_CPU_HZ
#define LCD_CPU_HZ 80000000u
#endif

// ========================= Delays ============================
static void SysTick_Init_NoInt(void){
  NVIC_ST_CTRL_R = 0;            // disable SysTick during setup
  NVIC_ST_RELOAD_R = 0x00FFFFFF; // maximum reload value
  NVIC_ST_CURRENT_R = 0;         // any write clears CURRENT
  NVIC_ST_CTRL_R = 0x00000005;   // enable SysTick with core clock, no interrupt
}

static void DelayCycles(uint32_t cycles){
  uint32_t chunk;
  while(cycles){
    chunk = cycles;
    if(chunk > 0x00FFFFFFu) chunk = 0x00FFFFFFu;
    NVIC_ST_RELOAD_R  = chunk - 1u;
    NVIC_ST_CURRENT_R = 0;
    while((NVIC_ST_CTRL_R & 0x00010000u) == 0u){ }
    cycles -= chunk;
  }
}

static void DelayUs(uint32_t us){
  uint32_t cycles_per_us;
  uint64_t total;
  uint32_t chunk;

  cycles_per_us = (LCD_CPU_HZ + 500000u) / 1000000u; // rounded
  total = (uint64_t)us * (uint64_t)cycles_per_us;

  while(total){
    chunk = (total > 0x00FFFFFFu) ? 0x00FFFFFFu : (uint32_t)total;
    DelayCycles(chunk);
    total -= chunk;
  }
}

static void DelayMs(uint32_t ms){
  while(ms--){
    DelayUs(1000u);
  }
}

// ===================== GPIO Init (LCD) =======================
static void LCD_GPIO_Init(void){
  // Enable clocks for Port A and Port B
  SYSCTL_RCGCGPIO_R |= 0x03u; // bit0=PortA, bit1=PortB
  while((SYSCTL_PRGPIO_R & 0x03u) != 0x03u){ }

  // Port A: PA2(EN), PA3(RS)
  GPIO_PORTA_DIR_R   |= (LCD_EN | LCD_RS | LCD_RW);
  GPIO_PORTA_DEN_R   |= (LCD_EN | LCD_RS);
  GPIO_PORTA_AFSEL_R &= ~(LCD_EN | LCD_RS);
  GPIO_PORTA_AMSEL_R &= ~(LCD_EN | LCD_RS);
  GPIO_PORTA_PCTL_R  &= ~0x0000FF00u; // clear PCTL for PA2/PA3
  GPIO_PORTA_DATA_R  &= ~(LCD_EN | LCD_RS | LCD_RW);

  // Port B: 4 data bits for DB4..DB7
  GPIO_PORTB_DIR_R   |= LCD_DB_MASK;
  GPIO_PORTB_DEN_R   |= LCD_DB_MASK;
  GPIO_PORTB_AFSEL_R &= ~LCD_DB_MASK;
  GPIO_PORTB_AMSEL_R &= ~LCD_DB_MASK;

#if (LCD_DB_SHIFT == 0)
  GPIO_PORTB_PCTL_R  &= ~0x0000FFFFu; // PB0-3
#else
  GPIO_PORTB_PCTL_R  &= ~0xFFFF0000u; // PB4-7
#endif

  GPIO_PORTB_DATA_R  &= ~LCD_DB_MASK;
}

// ===================== Low-level send =======================
static void LCD_WriteNibbleToPortB(uint8_t nibble){
  uint32_t data;
  data = GPIO_PORTB_DATA_R;
  data &= ~LCD_DB_MASK;
  data |= ((uint32_t)(nibble & 0x0Fu) << LCD_DB_SHIFT);
  GPIO_PORTB_DATA_R = data;
}

static void LCD_PulseEnable(void){
  GPIO_PORTA_DATA_R |= LCD_EN;
  DelayUs(1u);                 // >= 450ns
  GPIO_PORTA_DATA_R &= ~LCD_EN;
  DelayUs(1u);
}

// Send one nibble (4 bits) with RS=0 (cmd) or RS=1 (data)
static void SendDisplayNibble(uint8_t nibble, uint8_t rs){
  if(rs) GPIO_PORTA_DATA_R |= LCD_RS;
  else   GPIO_PORTA_DATA_R &= ~LCD_RS;
  GPIO_PORTA_DATA_R &= ~LCD_RW; // write mode

  LCD_WriteNibbleToPortB(nibble);
  LCD_PulseEnable();
}

// Send one byte (8 bits) in 4-bit mode: high nibble first, then low nibble
static void SendDisplayByte(uint8_t byte, uint8_t rs){
  SendDisplayNibble((uint8_t)(byte >> 4), rs);
  SendDisplayNibble((uint8_t)(byte & 0x0F), rs);
  DelayUs(50u); // >= 37us execution time (safe)
}

// ======================= Public APIs =========================
void LCD_Command(uint8_t cmd){
  SendDisplayByte(cmd, 0u);
  if(cmd == 0x01u || cmd == 0x02u){
    DelayMs(2u); // clear/home need longer
  }
}

void LCD_WriteChar(char c){
  SendDisplayByte((uint8_t)c, 1u);
}

void LCD_WriteString(const char *s){
  while(*s){
    LCD_WriteChar(*s++);
  }
}

void LCD_Clear(void){
  LCD_Command(0x01u);
}

void LCD_SetCursor(uint8_t row, uint8_t col){
  uint8_t addr;
  static const uint8_t row_addr[] = {0x00u, 0x40u, 0x14u, 0x54u};

  if(col > 15u) col = 15u;
  if(row > 3u) row = 0u;

  addr = (uint8_t)(row_addr[row] + col);
  LCD_Command((uint8_t)(0x80u | addr));
}

void LCD_Init(void){
  LCD_GPIO_Init();
  SysTick_Init_NoInt();

  // Wait after power-up
  DelayMs(40u);

  // ---- Special 4-bit init sequence (Appendix C / Figure 24) ----
  // First 3 transmissions of 0b0011 (RS=0), then 0b0010 to enter 4-bit mode.
  SendDisplayNibble(0x03u, 0u);  DelayMs(5u);
  SendDisplayNibble(0x03u, 0u);  DelayUs(200u);
  SendDisplayNibble(0x03u, 0u);  DelayUs(50u);
  SendDisplayNibble(0x02u, 0u);  DelayUs(50u);

  // Now in 4-bit mode: send full instructions as bytes (two nibbles)
  LCD_Command(0x28u); // Function set: 4-bit, 2 lines, 5x8 dots
  LCD_Command(0x08u); // Display off
  LCD_Command(0x01u); // Clear display
  LCD_Command(0x06u); // Entry mode: increment, no shift
  LCD_Command(0x0Cu); // Display on, cursor off, blink off
}
