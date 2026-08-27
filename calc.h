#ifndef CALC_H
#define CALC_H

// LCD ?????
#define CALC_LCD_LINE_LEN 16

// ??????
void Calc_Init(void);

// ??????('0'..'9','A','B','C','D','*','#')
void Calc_ProcessKey(char key);

// ?????? LCD ??????
const char *Calc_GetLine0(void);
const char *Calc_GetLine1(void);

#endif // CALCULATOR_H
