#include "lcd.h"
#include "keypad.h"
#include "calc.h"
// ????,????????? PLL / SysTick:
// #include "PLL.h"
// #include "TExaS.h"

int main(void){
    // ??:????????SysTick ?
    // PLL_Init(80);
    // SysTick_Init();

    LCD_Init();
    Keypad_Init();
    Calc_Init();

    LCD_Clear();
    LCD_SetCursor(0,0);
    LCD_WriteString(Calc_GetLine0());
    LCD_SetCursor(1,0);
    LCD_WriteString(Calc_GetLine1());

    while(1){
        char k = Keypad_GetKey();   // ?????? 0
        if(k){
            // ??????????
            Calc_ProcessKey(k);

            // ?? LCD ??(????????)
            LCD_SetCursor(0,0);
            LCD_WriteString("                "); // 16 spaces
            LCD_SetCursor(0,0);
            LCD_WriteString(Calc_GetLine0());

            LCD_SetCursor(1,0);
            LCD_WriteString("                ");
            LCD_SetCursor(1,0);
            LCD_WriteString(Calc_GetLine1());
        }
    }
}
