#include "keypad.h"

// =====================================================
// TM4C123 / Tiva-C register definitions (direct address)
// =====================================================
#define SYSCTL_RCGCGPIO_R       (*((volatile unsigned long *)0x400FE608))
#define SYSCTL_PRGPIO_R         (*((volatile unsigned long *)0x400FEA08))

// Port D (base 0x40007000)
#define GPIO_PORTD_DATA_R       (*((volatile unsigned long *)0x400073FC))
#define GPIO_PORTD_DIR_R        (*((volatile unsigned long *)0x40007400))
#define GPIO_PORTD_AFSEL_R      (*((volatile unsigned long *)0x40007420))
#define GPIO_PORTD_DEN_R        (*((volatile unsigned long *)0x4000751C))
#define GPIO_PORTD_AMSEL_R      (*((volatile unsigned long *)0x40007528))
#define GPIO_PORTD_PCTL_R       (*((volatile unsigned long *)0x4000752C))

// Port E (base 0x40024000)
#define GPIO_PORTE_DATA_R       (*((volatile unsigned long *)0x400243FC))
#define GPIO_PORTE_DIR_R        (*((volatile unsigned long *)0x40024400))
#define GPIO_PORTE_AFSEL_R      (*((volatile unsigned long *)0x40024420))
#define GPIO_PORTE_DEN_R        (*((volatile unsigned long *)0x4002451C))
#define GPIO_PORTE_AMSEL_R      (*((volatile unsigned long *)0x40024528))
#define GPIO_PORTE_PCTL_R       (*((volatile unsigned long *)0x4002452C))
#define GPIO_PORTE_PUR_R        (*((volatile unsigned long *)0x40024510))

// =====================================================
// Keypad wiring
// =====================================================
#define KP_COL_MASK 0x0F  // PD0-PD3
#define KP_ROW_MASK 0x0F  // PE0-PE3

static const char kKeyMap[4][4] = {
    {'1','2','3','A'},
    {'4','5','6','B'},
    {'7','8','9','C'},
    {'*','0','#','D'}
};

// =====================================================
// Small delays (approx)
// =====================================================
static void DelayUs(uint32_t us){
    volatile uint32_t i;
    while(us--){
        for(i = 0; i < 16; i++);
    }
}

/* static void DelayMs(uint32_t ms){
    volatile uint32_t i;
    while(ms--){
        for(i = 0; i < 16000; i++);
    }
}*/

// =====================================================
// Low-level scan (no debounce)
// =====================================================
static char Keypad_ScanRaw(void){
    uint32_t col, row, out, rows;

    // Default: all columns HIGH
    GPIO_PORTD_DATA_R = (GPIO_PORTD_DATA_R & ~KP_COL_MASK) | KP_COL_MASK;

    for(col = 0; col < 4; col++){
        // Drive one column LOW, others HIGH
        out = KP_COL_MASK & ~(1U << col);
        GPIO_PORTD_DATA_R = (GPIO_PORTD_DATA_R & ~KP_COL_MASK) | out;
        DelayUs(2);

        rows = GPIO_PORTE_DATA_R & KP_ROW_MASK;
        if(rows != KP_ROW_MASK){
            for(row = 0; row < 4; row++){
                if((rows & (1U << row)) == 0){
                    GPIO_PORTD_DATA_R = (GPIO_PORTD_DATA_R & ~KP_COL_MASK) | KP_COL_MASK;
                    return kKeyMap[row][col];
                }
            }
        }
    }

    GPIO_PORTD_DATA_R = (GPIO_PORTD_DATA_R & ~KP_COL_MASK) | KP_COL_MASK;
    return 0;
}

// =====================================================
// Public API
// =====================================================
void Keypad_Init(void){
    SYSCTL_RCGCGPIO_R |= (1U<<3) | (1U<<4);
    while((SYSCTL_PRGPIO_R & ((1U<<3) | (1U<<4))) != ((1U<<3) | (1U<<4))){}

    // Port D: columns output
    GPIO_PORTD_AMSEL_R &= ~KP_COL_MASK;
    GPIO_PORTD_PCTL_R  &= ~0x0000FFFF;
    GPIO_PORTD_AFSEL_R &= ~KP_COL_MASK;
    GPIO_PORTD_DIR_R   |=  KP_COL_MASK;
    GPIO_PORTD_DEN_R   |=  KP_COL_MASK;
    GPIO_PORTD_DATA_R   = (GPIO_PORTD_DATA_R & ~KP_COL_MASK) | KP_COL_MASK;

    // Port E: rows input with pull-ups
    GPIO_PORTE_AMSEL_R &= ~KP_ROW_MASK;
    GPIO_PORTE_PCTL_R  &= ~0x0000FFFF;
    GPIO_PORTE_AFSEL_R &= ~KP_ROW_MASK;
    GPIO_PORTE_DIR_R   &= ~KP_ROW_MASK;
    GPIO_PORTE_DEN_R   |=  KP_ROW_MASK;
    GPIO_PORTE_PUR_R   |=  KP_ROW_MASK;
}

char Keypad_GetKey(void){
    static char lastRaw = 0;
    static uint8_t sameCnt = 0;
    static uint8_t pressed = 0;

    char raw;

    raw = Keypad_ScanRaw();

    // soft debouncing: It requires reading the same value continuously for multiple times to be considered stable
    if(raw == lastRaw){
        if(sameCnt < 255) sameCnt++;
    }else{
        lastRaw = raw;
        sameCnt = 0;
    }

    // three consecutive agreements(sameCnt: 0,1,2)
    if(sameCnt >= 2){
        // press: return once
        if((pressed == 0) && (raw != 0)){
            pressed = 1;
            return raw;
        }
        // pressed: release for detection
        if((pressed == 1) && (raw == 0)){
            pressed = 0;
        }
    }

    return 0;
}


char Keypad_WaitKey(void){
    char k;
    do{
        k = Keypad_GetKey();
    }while(k == 0);
    return k;
}
