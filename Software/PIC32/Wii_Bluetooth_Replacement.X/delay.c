#include <xc.h>
#include "delay.h"

uint32_t main_timer = 0;    // App run time in ms

void t1_init(uint8_t prescaler, uint16_t period, uint8_t int_en)
{
    T1CONbits.ON = 0;
    
    T1CONbits.TCKPS = prescaler;    // 0 = 1:1, 1 = 1:8, 2 = 1:64, 3 = 1:256
    T1CONbits.TCS = 0;              // Source is PBCLK
    T1CONbits.TGATE = 0;            // Gated time accumulation off
    PR1 = period;                   // 16-bit period
    TMR1 = 0;                       // Clear timer
        
    IPC1bits.T1IP = 6;              // High interrupt priority
    IFS0bits.T1IF = 0;              // Clear interrupt flag
    if (int_en) IEC0bits.T1IE = 1;  // Timer interrupt enabled
    
    T1CONbits.ON = 1;
}

void t2_init(uint8_t bit_mode, uint8_t prescaler, uint32_t period, uint8_t int_en)
{
    T2CONbits.ON = 0;
    
    if (bit_mode)
    {
        T2CONbits.T32 = 1;                  // 32-bit mode on
        T2CONbits.TCKPS = prescaler;        
        T2CONbits.TCS = 0;                  // Source is PBCLK
        T2CONbits.TGATE = 0;                // Gated time accumulation off
        PR2 = period & 0xFFFF;              // LSW of 32-bit period
        PR3 = (period & 0xFFFF0000) >> 16;  // MSW of 32-bit period
        TMR2 = 0;                           // Clear timer LSW
        TMR3 = 0;                           // Clear timer MSW
        
        IPC3bits.T3IP = 6;              // High interrupt priority
        IFS0bits.T3IF = 0;              // Clear interrupt flag
        if (int_en) IEC0bits.T3IE = 1;  // Timer interrupt enabled
    }
    else
    {
        T2CONbits.T32 = 0;              // 16-bit mode on
        T2CONbits.TCKPS = prescaler;   
        T2CONbits.TCS = 0;              // Source is FCY
        T2CONbits.TGATE = 0;            // Gated time accumulation off
        PR2 = period & 0xFFFF;          // 16-bit period
        TMR2 = 0;                       // Clear timer
        
        IPC2bits.T2IP = 6;              // High interrupt priority
        IFS0bits.T2IF = 0;              // Clear interrupt flag
        if (int_en) IEC0bits.T2IE = 1;  // Timer interrupt enabled
    }
    
    T2CONbits.ON = 1;
}

void t3_init(uint8_t prescaler, uint16_t period, uint8_t int_en)
{
    T3CONbits.ON = 0;
    
    T2CONbits.T32 = 0;              // 16-bit mode on
    T3CONbits.TCKPS = prescaler;    
    T3CONbits.TCS = 0;              // Source is PBCLK
    T3CONbits.TGATE = 0;            // Gated time accumulation off
    PR3 = period & 0xFFFF;          // 16-bit period
    TMR3 = 0;                       // Clear timer
    
    IPC3bits.T3IP = 6;              // High interrupt priority
    IFS0bits.T3IF = 0;              // Clear interrupt flag
    if (int_en) IEC0bits.T3IE = 1;  // Timer interrupt enabled
    
    T3CONbits.ON = 1;
}

void t4_init(uint8_t bit_mode, uint8_t prescaler, uint32_t period, uint8_t int_en)
{
    T4CONbits.ON = 0;
    
    if (bit_mode)
    {
        T4CONbits.T32 = 1;                  // 32-bit mode on
        T4CONbits.TCKPS = prescaler;        
        T4CONbits.TCS = 0;                  // Source is PBCLK
        T4CONbits.TGATE = 0;                // Gated time accumulation off
        PR4 = period & 0xFFFF;              // LSW of 32-bit period
        PR5 = (period & 0xFFFF0000) >> 16;  // MSW of 32-bit period
        TMR4 = 0;                           // Clear timer LSW
        TMR5 = 0;                           // Clear timer MSW
        
        IPC5bits.T5IP = 6;              // High interrupt priority
        IFS0bits.T5IF = 0;              // Clear interrupt flag
        if (int_en) IEC0bits.T5IE = 1;  // Timer interrupt enabled
    }
    else
    {
        T4CONbits.T32 = 0;              // 16-bit mode on
        T4CONbits.TCKPS = prescaler;    
        T4CONbits.TCS = 0;              // Source is PBCLK
        T4CONbits.TGATE = 0;            // Gated time accumulation off
        PR4 = period & 0xFFFF;          // 16-bit period
        TMR4 = 0;                       // Clear timer
        
        IPC4bits.T4IP = 6;              // High interrupt priority
        IFS0bits.T4IF = 0;              // Clear interrupt flag
        if (int_en) IEC0bits.T4IE = 1;  // Timer interrupt enabled
    }
    
    T4CONbits.ON = 1;
}

void t5_init(uint8_t prescaler, uint16_t period, uint8_t int_en)
{
    T5CONbits.ON = 0;
    
    //T5CONbits.T32 = 0;              // 16-bit mode on
    T5CONbits.TCKPS = prescaler;    
    T5CONbits.TCS = 0;              // Source is PBCLK
    T5CONbits.TGATE = 0;            // Gated time accumulation off
    PR5 = period & 0xFFFF;          // 16-bit period
    TMR5 = 0;                       // Clear timer
    
    IPC4bits.T4IP = 6;              // High interrupt priority
    IFS0bits.T4IF = 0;              // Clear interrupt flag
    if (int_en) IEC0bits.T4IE = 1;  // Timer interrupt enabled
    
    T5CONbits.ON = 1;
}

void t1_off() {
    T1CONbits.ON = 0;
    IEC0bits.T1IE = 0;
    IFS0bits.T1IF = 0;
    TMR1 = 0;
}

void t2_off() {
    T2CONbits.ON = 0;
    if (T2CONbits.T32) {
        IEC0bits.T3IE = 0;
        IFS0bits.T3IF = 0;
        TMR2 = 0;
        TMR3 = 0;
    } else {
        IEC0bits.T2IE = 0;
        IFS0bits.T2IF = 0;
        TMR2 = 0;
    }
}

void t3_off() {
    T3CONbits.ON = 0;
    IEC0bits.T3IE = 0;
    IFS0bits.T3IF = 0;
    TMR3 = 0;
}

void t4_off() {
    T4CONbits.ON = 0;
    if (T4CONbits.T32) {
        IEC0bits.T5IE = 0;
        IFS0bits.T5IF = 0;
        TMR4 = 0;
        TMR5 = 0;
    } else {
        IEC0bits.T4IE = 0;
        IFS0bits.T4IF = 0;
        TMR4 = 0;
    }
}

void t5_off() {
    T5CONbits.ON = 0;
    IEC0bits.T5IE = 0;
    IFS0bits.T5IF = 0;
    TMR5 = 0;
}


// Count on T1 with period of 1ms
void t1_count_ms() {
    t1_init(0, PBCLK / 1000, 1);
}

void t2_count_ms() {
    t2_init(1, 0, PBCLK / 1000, 1);
}

// Count on T1 with period of 0.1ms
void t1_count_01ms() {
    t1_init(0, PBCLK / 10000, 1);
}

// Count on T1 with period of 1us
void t1_count_us() {
    t1_init(0, PBCLK / 1000000, 1);
}

// Accepts up to 1000 ms 
void delay_ms(uint32_t time) {
    // Period = SYSCLK divided by number of milliseconds in 1 second, then divided by prescaler
    t1_init(3, time * (PBCLK / 256000), 0);
    while (!IFS0bits.T1IF);
    t1_off();
}

// Accepts up to 4096 us
void delay_us(uint32_t time) {
    // Period = SYSCLK divided by number of microseconds in 1 second
    t1_init(0, time * (PBCLK / 1000000), 0);
    while (!IFS0bits.T1IF);
    t1_off();
}