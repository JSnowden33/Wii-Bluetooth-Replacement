#ifndef DELAY_H
#define DELAY_H

#define SYSCLK 60000000ULL
#define PBCLK  15000000ULL

extern uint32_t main_timer;

void t1_init(uint8_t prescaler, uint16_t period, uint8_t int_en);
void t2_init(uint8_t bit_mode, uint8_t prescaler, uint32_t period, uint8_t int_en);
void t3_init(uint8_t prescaler, uint16_t period, uint8_t int_en);
void t4_init(uint8_t bit_mode, uint8_t prescaler, uint32_t period, uint8_t int_en);
void t5_init(uint8_t prescaler, uint16_t period, uint8_t int_en);
void t1_off();
void t2_off();
void t3_off();
void t4_off();
void t5_off();
void t1_count_ms();
void t2_count_ms();
void t1_count_01ms();
void t1_count_us();
void delay_ms(uint32_t time);
void delay_us(uint32_t time);

#endif