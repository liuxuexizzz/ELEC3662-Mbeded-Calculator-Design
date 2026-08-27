
enum InputPorts{
  SW_PIN_PF40 = 5
};

enum OutputPorts{
  LED_PIN_PF1 = 5
};

// ************TExaS_Init*****************
// Initialize grader, triggered by timer 5A
// Lab 9 does not activate ADC1, PD3, or PLL
// This needs to be called once 
// Inputs: iport input(s) connected to this port
//         oport output(s) connected to this port
// Outputs: none
void TExaS_Init(enum InputPorts iport, enum OutputPorts oport);

// ************TExaS_Stop*****************
// Stop the transfer 
// Inputs:  none
// Outputs: none
void TExaS_Stop(void);
