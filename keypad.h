#ifndef KEYPAD_H
#define KEYPAD_H

#include <stdint.h>

// 4x4 Matrix Keypad (Multicomp MCAK1604NBWB)
// Rows  : input  from PORTE[0:3] (PE0-PE3)
// Cols  : output to   PORTD[0:3] (PD0-PD3)
// Key layout (row x col):
//   1 2 3 A
//   4 5 6 B
//   7 8 9 C
//   * 0 # D

// Initialize GPIO for keypad scanning.
void Keypad_Init(void);

// Non-blocking: returns 0 if no new key press detected.
// Debounces the key and waits for release before returning.
char Keypad_GetKey(void);

// Blocking: waits until a key is pressed (debounced) and released.
char Keypad_WaitKey(void);

#endif // KEYPAD_H
