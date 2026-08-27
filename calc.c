#include "calc.h"
#include <string.h> // strlen, strncpy, strchr
#include <ctype.h>  // isdigit

// ====================== ???? ======================

typedef enum{
    CALC_STATE_FIRST = 0,   // ???????? A
    CALC_STATE_SECOND,      // ???????? B
    CALC_STATE_RESULT       // ????
} CalcState;

#define CALC_MAX_INPUT 16   // ?????????(<= LCD ??)

static CalcState g_state;
static int       g_shift;                    // 0=??,1=Shift ????????
static char      g_curr[CALC_MAX_INPUT+1];   // ???????(A ? B)
static char      g_op;                       // ??? '+','-','*','/'
static double    g_opa, g_opb, g_result;     // ??????
static int       g_error;                    // ????(??? 0)

static char      g_line0[CALC_LCD_LINE_LEN+1];
static char      g_line1[CALC_LCD_LINE_LEN+1];

// ====================== ?????? ======================

static void CALC_RebuildDisplay(void);
static void CALC_ClearCurrent(void);
static void CALC_AppendChar(char c);
static void CALC_HandleOperator(char op);
static void CALC_HandleEndInput(void);
static void CALC_Rubout(void);
static double CALC_ParseNumber(const char *s);
static double CALC_DoBinary(double a, double b, char op);
static void CALC_FormatNumber(double value, char *buf, int bufSize);

// ====================== ?????? ======================

// ?????
void Calc_Init(void){
    g_state   = CALC_STATE_FIRST;
    g_shift   = 0;
    g_curr[0] = '\0';
    g_op      = 0;
    g_opa = g_opb = g_result = 0.0;
    g_error   = 0;

    strncpy(g_line0, "Mini-Calc", CALC_LCD_LINE_LEN);
    g_line0[CALC_LCD_LINE_LEN] = '\0';
    strncpy(g_line1, "0", CALC_LCD_LINE_LEN);
    g_line1[CALC_LCD_LINE_LEN] = '\0';
}

const char *Calc_GetLine0(void){ return g_line0; }
const char *Calc_GetLine1(void){ return g_line1; }

// ???????:? 4x4 ????????????
void Calc_ProcessKey(char key){
    int useShift;   // ???????? Shift ??

    // ???? Shift ?(D)
    if(key == 'D'){
        if(g_shift){
            g_shift = 0;   // ??????? Shift
        }else{
            g_shift = 1;   // ??,??????
        }
        return;
    }

    // ?????“???”?? Shift
    useShift = g_shift;
    g_shift = 0;

    // ??????????,??? Shift ?????
    if(g_error){
        Calc_Init();
        useShift = 0;
    }

    // ??? 0~9
    if(key >= '0' && key <= '9'){
        // ????????,?????????
        if(g_state == CALC_STATE_RESULT){
            g_state = CALC_STATE_FIRST;
            g_op = 0;
            g_opa = g_opb = g_result = 0.0;
            CALC_ClearCurrent();
        }
        CALC_AppendChar(key);
        CALC_RebuildDisplay();
        return;
    }

    // A:? Shift = ?;Shift = ?
    if(key == 'A'){
        if(useShift){
            CALC_HandleOperator('*');
        }else{
            CALC_HandleOperator('+');
        }
        CALC_RebuildDisplay();
        return;
    }

    // B:? Shift = ?;Shift = ?
    if(key == 'B'){
        if(useShift){
            CALC_HandleOperator('/');
        }else{
            CALC_HandleOperator('-');
        }
        CALC_RebuildDisplay();
        return;
    }

    // C:? Shift = ???;Shift = 'E'(×10 ??)
    if(key == 'C'){
        if(useShift){
            CALC_AppendChar('E');   // Times ten to the power
        }else{
            CALC_AppendChar('.');   // ???
        }
        CALC_RebuildDisplay();
        return;
    }

    // *:End Input,??? '='
    if(key == '*'){
        CALC_HandleEndInput();
        CALC_RebuildDisplay();
        return;
    }

    // #:? Shift = ??????;Shift = ????
    if(key == '#'){
        if(useShift){
            Calc_Init();        // ??????
        }else{
            CALC_Rubout();      // ??????
        }
        CALC_RebuildDisplay();
        return;
    }

    // ???????
}

// ====================== ???? ======================

// ???????????
static void CALC_ClearCurrent(void){
    g_curr[0] = '\0';
}

// ???????????:?? 0~9, '.' , 'E'
static void CALC_AppendChar(char c){
    unsigned int len;

    len = (unsigned int)strlen(g_curr);
    if(len >= CALC_MAX_INPUT) return; // ??

    if(c == '.'){
        // ???????????,???? 'E' ??
        if(strchr(g_curr, '.') != 0) return;
        if(strchr(g_curr, 'E') != 0) return;
        if(len == 0){
            // ??????,????? "0."
            g_curr[len] = '0';
            len++;
        }
    }else if(c == 'E'){
        // E ????????,????? E
        if(len == 0) return;
        if(strchr(g_curr, 'E') != 0) return;
    }

    g_curr[len]   = c;
    g_curr[len+1] = '\0';
}

// ????? +,-,*,/
static void CALC_HandleOperator(char op){
    // ??????????
    if(g_state == CALC_STATE_FIRST){
        if(g_curr[0] == '\0'){
            // ??????,?????
            return;
        }
        g_opa = CALC_ParseNumber(g_curr);
        g_op  = op;
        CALC_ClearCurrent();
        g_state = CALC_STATE_SECOND;
        return;
    }

    // ??????????:?????????????
    if(g_state == CALC_STATE_SECOND){
        if(g_curr[0] == '\0'){
            g_op = op;       // ?? B,??????
        }else{
            // ???????,??????? A
            g_opb    = CALC_ParseNumber(g_curr);
            g_result = CALC_DoBinary(g_opa, g_opb, g_op);
            g_opa    = g_result;
            g_op     = op;
            CALC_ClearCurrent();
        }
        return;
    }

    // ????????,????????? A
    if(g_state == CALC_STATE_RESULT){
        g_opa   = g_result;
        g_op    = op;
        CALC_ClearCurrent();
        g_state = CALC_STATE_SECOND;
    }
}

// ?? *(End Input / =)
static void CALC_HandleEndInput(void){
    if(g_state == CALC_STATE_FIRST){
        // ?????:???????
        if(g_curr[0] != '\0'){
            g_opa = CALC_ParseNumber(g_curr);
            g_result = g_opa;
        }
        g_state = CALC_STATE_RESULT;
        return;
    }

    if(g_state == CALC_STATE_SECOND){
        if(g_curr[0] == '\0'){
            // ??? B,?? B = A
            g_opb = g_opa;
        }else{
            g_opb = CALC_ParseNumber(g_curr);
        }
        g_result = CALC_DoBinary(g_opa, g_opb, g_op);
        g_state  = CALC_STATE_RESULT;
        return;
    }

    // ???????,?? * ???
}

// ??????
static void CALC_Rubout(void){
    unsigned int len;
    len = (unsigned int)strlen(g_curr);
    if(len == 0) return;
    g_curr[len-1] = '\0';
}

// ?????????:??
//   123
//   12.34
//   1.2E3   (?? 1.2 × 10^3)
//   12E4
// ????????;??????????
static double CALC_ParseNumber(const char *s){
    double value;
    unsigned int i;

    value = 0.0;
    i = 0;

    // ????
    while(s[i] != '\0' && s[i] != '.' && s[i] != 'E'){
        if(isdigit((unsigned char)s[i])){
            value = value*10.0 + (double)(s[i] - '0');
        }
        i++;
    }

    // ????
    if(s[i] == '.'){
        double scale;
        i++;
        scale = 0.1;
        while(s[i] != '\0' && s[i] != 'E'){
            if(isdigit((unsigned char)s[i])){
                value += (double)(s[i] - '0') * scale;
                scale *= 0.1;
            }
            i++;
        }
    }

    // ????(10 ??)
    if(s[i] == 'E'){
        int exp;
        exp = 0;
        i++;
        while(s[i] != '\0'){
            if(isdigit((unsigned char)s[i])){
                exp = exp*10 + (s[i] - '0');
            }
            i++;
        }
        // value *= 10^exp
        while(exp > 0){
            value *= 10.0;
            exp--;
        }
    }

    return value;
}

// ??????
static double CALC_DoBinary(double a, double b, char op){
    double res;
    res = 0.0;

    switch(op){
        case '+':
            res = a + b;
            break;
        case '-':
            res = a - b;
            break;
        case '*':
            res = a * b;
            break;
        case '/':
            if(b == 0.0){
                g_error = 1;   // ? 0 ??
                res = 0.0;
            }else{
                res = a / b;
            }
            break;
        default:
            res = b;
            break;
    }
    return res;
}

// ? double ?????:
//  ???? + ?? 4 ???,?????? 0 ????
static void CALC_FormatNumber(double value, char *buf, int bufSize){
    int  negative;
    long intPart;
    double frac;
    long fracInt;
    char tmp[20];
    int  pos;
    int  idx;

    if(bufSize < 2) return;

    negative = 0;
    if(value < 0.0){
        negative = 1;
        value = -value;
    }

    intPart = (long)value;
    frac    = value - (double)intPart;

    // 4 ???,????
    fracInt = (long)(frac * 10000.0 + 0.5);

    // ?? 0.99995 ????
    if(fracInt >= 10000){
        fracInt = 0;
        intPart += 1;
    }

    // ????????(??)
    pos = 0;
    do{
        if(pos < (int)sizeof(tmp)-1){
            tmp[pos] = (char)('0' + (int)(intPart % 10));
            pos++;
        }
        intPart /= 10;
    }while(intPart > 0 && pos < (int)sizeof(tmp)-1);

    if(negative && pos < (int)sizeof(tmp)-1){
        tmp[pos] = '-';
        pos++;
    }

    // ??? buf
    idx = 0;
    while(pos > 0 && idx < bufSize-1){
        pos--;
        buf[idx] = tmp[pos];
        idx++;
    }

    // ????
    if(fracInt > 0 && idx < bufSize-2){
        int k;
        int d[4];

        buf[idx] = '.';
        idx++;

        for(k = 3; k >= 0; k--){
            d[k] = (int)(fracInt % 10);
            fracInt /= 10;
        }
        for(k = 0; k < 4 && idx < bufSize-1; k++){
            buf[idx] = (char)('0' + d[k]);
            idx++;
        }

        // ??????? 0 ????
        while(idx > 0 && buf[idx-1] == '0'){
            idx--;
        }
        if(idx > 0 && buf[idx-1] == '.'){
            idx--;
        }
    }

    buf[idx] = '\0';
}

// ??????????????
static void CALC_RebuildDisplay(void){
    char buf[20];

    // ????
    if(g_error){
        strncpy(g_line0, "Error", CALC_LCD_LINE_LEN);
        g_line0[CALC_LCD_LINE_LEN] = '\0';
        strncpy(g_line1, "Div by zero", CALC_LCD_LINE_LEN);
        g_line1[CALC_LCD_LINE_LEN] = '\0';
        return;
    }

    // ??????
    if(g_state == CALC_STATE_FIRST){
        strncpy(g_line0, "Enter A", CALC_LCD_LINE_LEN);
        g_line0[CALC_LCD_LINE_LEN] = '\0';

        if(g_curr[0] == '\0'){
            strncpy(g_line1, "0", CALC_LCD_LINE_LEN);
        }else{
            strncpy(g_line1, g_curr, CALC_LCD_LINE_LEN);
        }
        g_line1[CALC_LCD_LINE_LEN] = '\0';
        return;
    }

    // ??????
    if(g_state == CALC_STATE_SECOND){
        int len;

        // ? 1 ??? “A <op>”
        CALC_FormatNumber(g_opa, buf, sizeof(buf));

        len = 0;
        while(buf[len] != '\0' && len < CALC_LCD_LINE_LEN-2){
            g_line0[len] = buf[len];
            len++;
        }
        if(len < CALC_LCD_LINE_LEN-1){
            g_line0[len] = ' ';
            len++;
            g_line0[len] = g_op;
            len++;
        }
        g_line0[len] = '\0';

        if(g_curr[0] == '\0'){
            strncpy(g_line1, "?", CALC_LCD_LINE_LEN);
        }else{
            strncpy(g_line1, g_curr, CALC_LCD_LINE_LEN);
        }
        g_line1[CALC_LCD_LINE_LEN] = '\0';
        return;
    }

    // ????
    strncpy(g_line0, "Ans", CALC_LCD_LINE_LEN);
    g_line0[CALC_LCD_LINE_LEN] = '\0';

    CALC_FormatNumber(g_result, buf, sizeof(buf));
    strncpy(g_line1, buf, CALC_LCD_LINE_LEN);
    g_line1[CALC_LCD_LINE_LEN] = '\0';
}
