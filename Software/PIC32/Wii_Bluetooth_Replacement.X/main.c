#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "usb.h"
#include "hci.h"
#include <xc.h>
#include <string.h>
#include "usb_config.h"
#include "usb_ch9.h"
#include "usb_hid.h"
#include "uart.h"
#include "delay.h"
#include "wiimote.h"

// DEVCFG3
#pragma config PMDL1WAY = OFF           // Peripheral Module Disable Configuration (Allow only one reconfiguration)
#pragma config IOL1WAY = OFF            // Peripheral Pin Select Configuration (Allow only one reconfiguration)
#pragma config FUSBIDIO = ON            // USB USID Selection (Controlled by the USB Module)
#pragma config FVBUSONIO = OFF          // USB VBUS ON Selection (Controlled by software)

// DEVCFG2
#pragma config FPLLIDIV = DIV_2         // PLL Input Divider (2x Divider)
#pragma config FPLLMUL = MUL_15         // PLL Multiplier (15x Multiplier)
#pragma config UPLLIDIV = DIV_2         // USB PLL Input Divider (2x Divider)
#pragma config UPLLEN = ON              // USB PLL Enable (Enabled)
//#pragma config FPLLODIV = DIV_2         // System PLL Output Clock Divider (PLL Divide by 2) (30 MHz)
#pragma config FPLLODIV = DIV_1         // System PLL Output Clock Divider (PLL Divide by 1) (60 MHz, overclocked)

// DEVCFG1
#pragma config FNOSC = PRIPLL           // Oscillator Selection Bits (Primary Osc w/PLL (XT+,HS+,EC+PLL))
#pragma config FSOSCEN = OFF            // Secondary Oscillator Enable (Disabled)
#pragma config IESO = OFF               // Internal/External Switch Over (Disabled)
#pragma config POSCMOD = HS             // Primary Oscillator Configuration (HS osc mode)
#pragma config OSCIOFNC = OFF           // CLKO Output Signal Active on the OSCO Pin (Disabled)
#pragma config FPBDIV = DIV_4           // Peripheral Clock Divisor (Pb_Clk is Sys_Clk/4)
#pragma config FCKSM = CSDCMD           // Clock Switching and Monitor Selection (Clock Switch Disable, FSCM Disabled)
#pragma config WDTPS = PS1              // Watchdog Timer Postscaler (1:1)
#pragma config WINDIS = OFF             // Watchdog Timer Window Enable (Watchdog Timer is in Non-Window Mode)
#pragma config FWDTEN = OFF             // Watchdog Timer Enable (WDT Disabled (SWDTEN Bit Controls))
#pragma config FWDTWINSZ = WINSZ_25     // Watchdog Timer Window Size (Window Size is 25%)

// DEVCFG0
#pragma config JTAGEN = OFF             // JTAG Enable (JTAG Port Enabled)
#pragma config ICESEL = ICS_PGx1        // ICE/ICD Comm Channel Select (Communicate on PGEC1/PGED1)
#pragma config PWP = OFF                // Program Flash Write Protect (Disable)
#pragma config BWP = OFF                // Boot Flash Write Protect bit (Protection Disabled)
#pragma config CP = OFF                 // Code Protect (Protection Disabled)

static int32_t hci_len = 0;
static uint32_t hci_reset_timer = 0;

static uint32_t _excep_code;
static uint32_t _excep_addr;
// This function overrides the normal weak generic handler
void _general_exception_handler(void) {
	asm volatile("mfc0 %0,$13" : "=r" (_excep_code));
	asm volatile("mfc0 %0,$14" : "=r" (_excep_addr));
	_excep_code = (_excep_code & 0x0000007C) >> 2;    

	uart_transmit("Exception value/address:", 1);
	uart_transmit_val(_excep_code, 8, 1);
	uart_transmit_val(_excep_addr, 8, 1);

	while (1);
}

void __attribute__((vector(_TIMER_3_VECTOR), interrupt(), nomips16)) _T3Interrupt() {
	main_timer++;	// Count ms
	IFS0bits.T3IF = 0;
}

int main(void) {
	TRISBbits.TRISB0 = 0;	// ESP32 EN
	TRISBbits.TRISB1 = 1;	// SDI
	TRISBbits.TRISB2 = 0;   // SDO
	TRISBbits.TRISB3 = 0;   // CS
	TRISBbits.TRISB4 = 1;   // SPI_EN
	TRISBbits.TRISB14 = 0;  // SCK
	TRISBbits.TRISB15 = 0;  // TX
	
	LATBbits.LATB0 = 0;

	// Set all pins as digital
	ANSELA = 0;
	ANSELB = 0;

	// Allow multi-vector interrupts for USB
	INTCONbits.MVEC = 1;
	IPC7bits.USBIP = 4;
	__asm volatile("ei");

	t2_count_ms();
	uart_configure(115200);
	spi_master_init(2000000, 1);
	uart_transmit("UART init", 1);

	if (RCONbits.SWR) uart_transmit("RST - software", 1);
	if (RCONbits.WDTO) uart_transmit("RST - watchdog", 1);
	if (RCONbits.SLEEP) uart_transmit("RST - wakeup", 1);
	if (RCONbits.IDLE) uart_transmit("RST - was idle", 1);
	if (RCONbits.BOR) uart_transmit("RST - brown-out", 1);
	if (RCONbits.POR) uart_transmit("RST - power-on", 1);
	RCON = 0;

	usb_init();
	
	LATBbits.LATB0 = 1;
	TRISBbits.TRISB0 = 1;

	while (1) {
		if (usb_is_configured()) {
			if (!usb_in_endpoint_busy(1)) {
				int32_t len = hci_get_event(usb_get_in_buffer(1));
				if (len > 0) usb_send_in_buffer(1, len);
			}
			if (!usb_in_endpoint_busy(2)) {
				int32_t len = hci_get_data(usb_get_in_buffer(2));
				if (len > 0) usb_send_in_buffer(2, len);
			}
		}
		
		update_wiimotes();
		
		// Handle ESP32 reset
		/*
		if (hci_reset_status) {
			hci_reset_timer = main_timer;
			hci_reset_status = 0;
			TRISBbits.TRISB0 = 0;
			LATBbits.LATB0 = 0;
		}
		if ((main_timer - hci_reset_timer >= 50) && !PORTBbits.RB0) {
			LATBbits.LATB0 = 1;
			TRISBbits.TRISB0 = 1;
		}
		*/
	}
	return 0;
}

/* Callbacks. These function names are set in usb_config.h. */

void app_set_configuration_callback(uint8_t configuration) {

}

uint16_t app_get_device_status_callback() {
	return 0x0000;
}

void app_endpoint_halt_callback(uint8_t endpoint, bool halted) {

}

int8_t app_set_interface_callback(uint8_t interface, uint8_t alt_setting) {
	return 0;
}

int8_t app_get_interface_callback(uint8_t interface) {
	return 0;
}

void app_out_transaction_callback(uint8_t endpoint) {
	// Received data from the bulk endpoint
	uint8_t len;
	uint8_t * buf;
	len = usb_get_out_buffer(2, (const unsigned char **)&buf);

	hci_recv_data(buf, len);
	usb_arm_out_endpoint(2);
}

void app_in_transaction_complete_callback(uint8_t endpoint) {

}

int8_t ep0_cb(bool data_ok, void * context) {
	if (data_ok) {
		hci_recv_command(hci_len);
		return 0;
	} else return -1;
}

int8_t app_unknown_setup_request_callback(const struct setup_packet *setup) {
	if (setup->REQUEST.type == 1) {	// Class request (HCI command)? 
		hci_len = setup->wLength;
		usb_start_receive_ep0_data_stage(hci_get_cmd_buffer(), setup->wLength, (usb_ep0_data_stage_callback)ep0_cb, NULL);
		return 0;
	} else return -1;
}

int16_t app_unknown_get_descriptor_callback(const struct setup_packet *pkt, const void **descriptor) {
	return 0;
}

//void app_start_of_frame_callback(void)
//{
//
//}
//
//void app_usb_reset_callback(void)
//{
//
//}
