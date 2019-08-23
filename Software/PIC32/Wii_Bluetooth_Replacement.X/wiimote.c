#include "wiimote.h"
#include "wm_reports.h"
#include "wm_crypto.h"
#include "wm_eeprom.h"
#include "spi.h"
#include "delay.h"
#include "uart.h"
#include "hci.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/endian.h>

#include <cp0defs.h>
#include <xc.h>

uint32_t prev_update_time = 0;

wiimote_t wiimotes[4];

wiimote_t * get_wiimote_from_handle(uint16_t hci_handle) {
	uint8_t i = 0;
	for (i = 0; i < 4; i++) {
		if (wiimotes[i].sys.hci_handle == hci_handle) return &wiimotes[i];
	}
	return NULL;
}

void _wiimote_recv_ctrl(uint16_t hci_handle, const uint8_t * buf, int len) {
	return;
}

int32_t _wiimote_get_ctrl(uint16_t hci_handle, uint8_t * buf) {
	return 0;
}

void _wiimote_recv_data(uint16_t hci_handle, const uint8_t * buf, int len) {
	wiimote_t * wiimote = get_wiimote_from_handle(hci_handle);
	if (wiimote) wiimote_recv_report(wiimote, buf, len);
}

int32_t _wiimote_get_data(uint16_t hci_handle, uint8_t * buf) {
	wiimote_t * wiimote = get_wiimote_from_handle(hci_handle);
	if (wiimote) return wiimote_get_report(wiimote, buf);
	return 0;
}

int wiimote_recv_report(wiimote_t * wiimote, const uint8_t * buf, int len) {
struct report_data * data = (struct report_data *)buf;

// Every output report contains rumble info
wiimote->sys.rumble = data->buf[0] & 0x01;

	switch (data->type) {
		case 0x11: {    // Player LEDS
		struct report_leds * rpt = (struct report_leds *)data->buf;

			wiimote->sys.led_1 = rpt->led_1;
			wiimote->sys.led_2 = rpt->led_2;
			wiimote->sys.led_3 = rpt->led_3;
			wiimote->sys.led_4 = rpt->led_4;

			report_queue_push_ack(wiimote, data->type, 0x00);
			break;
		}
		case 0x12: {    // Data reporting mode
			struct report_mode * rpt = (struct report_mode *)data->buf;

			//wiimote->sys.reporting_continuous = rpt->continuous;
			wiimote->sys.reporting_continuous = 1;	// Always use continuous mode for now
			wiimote->sys.reporting_mode = rpt->mode;

			report_queue_push_ack(wiimote, data->type, 0x00);
			break;
		}
		case 0x13:
		case 0x1a: {    // IR camera enable
			struct report_ir_enable * rpt = (struct report_ir_enable *)data->buf;

			wiimote->sys.ircam_enabled = rpt->enabled;

			report_queue_push_ack(wiimote, data->type, 0x00);
			break;
		}
		case 0x14:
		case 0x19: {    // Speaker enable
			struct report_speaker_enable * rpt = (struct report_speaker_enable *)data->buf;

			wiimote->sys.speaker_enabled = !rpt->muted;

			report_queue_push_ack(wiimote, data->type, 0x00);
			break;
		}
		case 0x15: // Status information request
			//send a status report 0x20
			report_queue_push_status(wiimote);
			break;
		case 0x16: {    // Write memory
			struct report_mem_write * rpt = (struct report_mem_write *)data->buf;

			if (rpt->source0 || rpt->source1) write_register(wiimote, ntohl(OFFSET24(rpt->offset)), rpt->size, rpt->data);
			else write_eeprom(wiimote, ntohl(OFFSET24(rpt->offset)), rpt->size, rpt->data);
			break;
		}
		case 0x17: {    // Read memory
			struct report_mem_read * rpt = (struct report_mem_read *)data->buf;

			if (rpt->source0 || rpt->source1) read_register(wiimote, ntohl(OFFSET24(rpt->offset)), ntohs(rpt->size));
			else read_eeprom(wiimote, ntohl(OFFSET24(rpt->offset)), ntohs(rpt->size));

			break;
		}
	}
	return 0;
}

int wiimote_get_report(wiimote_t * wiimote, uint8_t * buf) {
	if (main_timer - wiimote->sys.last_report_time < 11) return 0;	// Wait 11ms or more between reports

	int len;

	struct report_data * data = (struct report_data *)buf;
	uint8_t * contents;

	// Add check for button changes for non-continuous reporting
	if (!wiimote->sys.reporting_continuous && !wiimote->sys.report_changed && wiimote->sys.queue == NULL) return 0;

	if (wiimote->sys.queue == NULL)
	{
		//regular report
		memset(data, 0, sizeof(struct report_data));
		len = 2;
		data->io = 0xa1;
		data->type = wiimote->sys.reporting_mode;
	}
	else
	{
		//queued report (acknowledgement, response, etc)
		struct report * rpt;
		rpt = report_queue_peek(wiimote);
		len = rpt->len;
		memcpy(data, &rpt->data, sizeof(struct report_data));
		report_queue_pop(wiimote);
	}

	contents = data->buf;

	// Fill report
	switch (data->type) {
		case 0x30: // core buttons
			report_append_buttons(wiimote, contents);
			len += 2;
			break;
		case 0x31: // core buttons + accelerometer
			report_append_buttons(wiimote, contents);
			report_append_accelerometer(wiimote, contents);
			len += 2 + 3;
			break;
		case 0x32: // core buttons + 8 extension bytes
			report_append_buttons(wiimote, contents);
			len += 2 + 8;
			break;
		case 0x33: // core buttons + accelerometer + 12 ir bytes
			report_append_buttons(wiimote, contents);
			report_append_accelerometer(wiimote, contents);
			report_append_ir_12(wiimote, contents + 5);
			len += 2 + 3 + 12;
			break;
		case 0x34: // core buttons + 19 extension bytes
			report_append_buttons(wiimote, contents);
			report_append_extension(wiimote, contents + 2, 19);
			len += 2 + 19;
			break;
		case 0x35: // core buttons + accelerometer + 16 extension bytes
			report_append_buttons(wiimote, contents);
			report_append_accelerometer(wiimote, contents);
			report_append_extension(wiimote, contents + 5, 16);
			len += 2 + 3 + 16;
			break;
		case 0x36: // core buttons + 10 ir bytes + 9 extension bytes
			report_append_buttons(wiimote, contents);
			report_append_ir_10(wiimote, contents + 2);
			report_append_extension(wiimote, contents + 12, 9);
			len += 2 + 10 + 9;
			break;
		case 0x37: // core buttons + accelerometer + 10 ir bytes + 6 extension bytes
			report_append_buttons(wiimote, contents);
			report_append_accelerometer(wiimote, contents);
			report_append_ir_10(wiimote, contents + 5);
			report_append_extension(wiimote, contents + 15, 6);
			len += 2 + 3 + 10 + 6;
			break;
		case 0x3d: // 21 extension bytes
			report_append_extension(wiimote, contents, 21);
			len += 21;
			break;
		case 0x3e: // interleaved core buttons + accelerometer with 36 ir bytes pt I
		case 0x3f: // interleaved core buttons + accelerometer with 36 ir bytes pt II
			report_append_buttons(wiimote, contents);
			report_append_interleaved(wiimote, contents);
			len += 21;
			break;
		case 0x20:  // Disable continuous reporting when status is sent
			//wiimote->sys.reporting_continuous = 0;
			report_append_buttons(wiimote, contents);
			break;  
		default: // special output report (acknowledgement, status, or memory read)
			report_append_buttons(wiimote, contents);
			break;
	}

	wiimote->sys.report_changed = 0;
	wiimote->sys.last_report_time = main_timer;
	return len;
}

void ir_object_clear(wiimote_t * wiimote, uint8_t num) {
	memset(&(wiimote->usr.ir_object[num]), 0xff, sizeof(struct wiimote_ir_object));
}

void read_eeprom(wiimote_t * wiimote, uint32_t offset, uint16_t size) {
	const uint8_t * buffer;
	struct report * rpt;
	int i;

	// Addresses greater than 0x16FF cannot be read or written
	if (offset + size > 0x16FF) {
		rpt = report_queue_push(wiimote);
		report_format_mem_resp(rpt, 0x10, 0x8, offset, NULL);
		return;
	}

	buffer = wiimote_eeprom + offset + 0x70;

	// Equivalent to ceil(size / 0x10)
	int totalpackets = (size + 0x10 - 1) / 0x10;

	// Allocate all the needed reports
	for (i=0; i<totalpackets; i++) report_queue_push(wiimote);

	// Copy packet data
	struct queued_report * currentrpt = wiimote->sys.queue;

	for (i=0; i<totalpackets; i++) {
		rpt = &currentrpt->rpt;
		int psize = (i == totalpackets-1) ? (size % 0x10) : 0xf;
		report_format_mem_resp(rpt, psize, 0x0, offset + i*0x10, &buffer[i*0x10]);
		currentrpt = currentrpt->next;
	}
}

void write_eeprom(wiimote_t * wiimote, uint32_t offset, uint8_t size, const uint8_t * buf) {
	struct report * rpt;

	//addresses greater than 0x16FF cannot be read or written
	if (offset + size > 0x16FF) {
		rpt = report_queue_push(wiimote);
		report_format_mem_resp(rpt, 0x10, 0x8, offset, NULL);
		return;
	}

	// Send values to EEPROM

	report_queue_push_ack(wiimote, 0x16, 0x00);
}

void read_register(wiimote_t * wiimote, uint32_t offset, uint16_t size) {
	uint8_t * buffer;
	struct report * rpt;
	int i;

	switch ((offset >> 16) & 0xfe) {	// Select register, ignore lsb 
		case 0xa2: //speaker
			buffer = wiimote->register_a2 + (offset & 0xff);
			break;
		case 0xa4: //extension
			if (wiimote->sys.wmp_state == 1) {
				if (((offset & 0xff) == 0xf6) || ((offset & 0xff) == 0xf7)) {
					wiimote->sys.tries += 1;
					if (wiimote->sys.tries == 5) wiimote->register_a6[0xf7] = 0x0e;
				}
				buffer = wiimote->register_a6 + (offset & 0xff);
			}
			else buffer = wiimote->register_a4 + (offset & 0xff);
			break;
		case 0xa6: //motionplus
			if (wiimote->sys.wmp_state == 1) {
				rpt = report_queue_push(wiimote);
				report_format_mem_resp(rpt, 0x10, 0x7, offset, NULL);
				return;
			}
			buffer = wiimote->register_a6 + (offset & 0xff);
			break;
		case 0xb0: //ir camera
			buffer = wiimote->register_b0 + (offset & 0xff);
			break;
		default: //???
			break;
	}

	//equivalent to math.ceil(size / 0x10)
	int totalpackets = (size + 0x10 - 1) / 0x10;

	//allocate all the needed reports
	for (i=0; i<totalpackets; i++) report_queue_push(wiimote);

	//copy packet data
	struct queued_report * currentrpt = wiimote->sys.queue;

	for (i=0; i<totalpackets; i++)
	{
		rpt = &currentrpt->rpt;
		int psize = (i == totalpackets-1) ? (size % 0x10) : 0xf;
		report_format_mem_resp(rpt, psize, 0x0, offset + i*0x10, &buffer[i*0x10]);
		currentrpt = currentrpt->next;
	}
}

void write_register(wiimote_t * wiimote, uint32_t offset, uint8_t size, const uint8_t * buf) {
	uint8_t * reg;

	switch ((offset >> 16) & 0xfe) //select register, ignore lsb
	{
		case 0xa2: //speaker
			reg = wiimote->register_a2;
			memcpy(reg + (offset & 0xff), buf, size);
			break;
		case 0xa4: //extension
			if (wiimote->sys.wmp_state == 1) reg = wiimote->register_a6;
			else reg = wiimote->register_a4;

			memcpy(reg + (offset & 0xff), buf, size);

			//TODO: double check what this does, the buf location it's looking for
			if (((offset & 0xff) == 0xf0) && (buf[0] == 0x55) && (wiimote->sys.wmp_state == 1)) {	// Deactivate WMP
				wiimote->sys.wmp_state = 3;

				init_extension(wiimote);

				report_queue_push_ack(wiimote, 0x16, 0x00);
				wiimote->sys.extension_connected = 0;
				report_queue_push_status(wiimote);
				wiimote->sys.extension_connected = 1;
				report_queue_push_status(wiimote);
				return;
			}
			else if (((offset & 0xff) == 0xfe) && (buf[0] == 0x00) && (wiimote->sys.wmp_state == 1)) { //also deactivate wmp?
				wiimote->sys.wmp_state = 0;

				init_extension(wiimote);

				report_queue_push_ack(wiimote, 0x16, 0x00);
				wiimote->sys.extension_connected = 0;
				report_queue_push_status(wiimote);
				wiimote->sys.extension_connected = 1;
				report_queue_push_status(wiimote);
				return;
			}
			else if ((offset & 0xff) == 0x4c) { //last part of encryption code
				generate_tables(wiimote);
				wiimote->sys.extension_encrypted = 1;
			}
			/*
			else if (((offset & 0xff) == 0xf0) && (buf[0] == 0xaa)) {
				//technically this sets the encryption
				//wiimote->sys.extension_encrypted = 1;
			}
			*/
			else if ((offset & 0xff) == 0xf1) {
				wiimote->register_a6[0xf7] = 0x1a;

				// Sometimes this must be updated
				wiimote->register_a6[0x50] = 0xe7;
				wiimote->register_a6[0x51] = 0x98;
				wiimote->register_a6[0x52] = 0x31;
				wiimote->register_a6[0x53] = 0x8a;
				wiimote->register_a6[0x54] = 0x18;
				wiimote->register_a6[0x55] = 0x82;
				wiimote->register_a6[0x56] = 0x37;
				wiimote->register_a6[0x57] = 0x5e;
				wiimote->register_a6[0x58] = 0x02;
				wiimote->register_a6[0x59] = 0x4f;
				wiimote->register_a6[0x5a] = 0x68;
				wiimote->register_a6[0x5b] = 0x47;
				wiimote->register_a6[0x5c] = 0x78;
				wiimote->register_a6[0x5d] = 0xef;
				wiimote->register_a6[0x5e] = 0xbb;
				wiimote->register_a6[0x5f] = 0xd7;

				wiimote->register_a6[0x60] = 0x86;
				wiimote->register_a6[0x61] = 0xc8;
				wiimote->register_a6[0x62] = 0x95;
				wiimote->register_a6[0x63] = 0xbd;
				wiimote->register_a6[0x64] = 0x20;
				wiimote->register_a6[0x65] = 0x9b;
				wiimote->register_a6[0x66] = 0xeb;
				wiimote->register_a6[0x67] = 0x8b;
				wiimote->register_a6[0x68] = 0x79;
				wiimote->register_a6[0x69] = 0x81;
				wiimote->register_a6[0x6a] = 0xdc;
				wiimote->register_a6[0x6b] = 0x61;
				wiimote->register_a6[0x6c] = 0x13;
				wiimote->register_a6[0x6d] = 0x54;
				wiimote->register_a6[0x6e] = 0x79;
				wiimote->register_a6[0x6f] = 0x4c;

				wiimote->register_a6[0x70] = 0xb7;
				wiimote->register_a6[0x71] = 0x26;
				wiimote->register_a6[0x72] = 0x82;
				wiimote->register_a6[0x73] = 0x17;
				wiimote->register_a6[0x74] = 0xe8;
				wiimote->register_a6[0x75] = 0x0f;
				wiimote->register_a6[0x76] = 0xa9;
				wiimote->register_a6[0x77] = 0xb5;
				wiimote->register_a6[0x78] = 0x45;
				wiimote->register_a6[0x79] = 0xa0;
				wiimote->register_a6[0x7a] = 0x38;
				wiimote->register_a6[0x7b] = 0x8e;
				wiimote->register_a6[0x7c] = 0x9e;
				wiimote->register_a6[0x7d] = 0x86;
				wiimote->register_a6[0x7e] = 0x72;
				wiimote->register_a6[0x7f] = 0x55;

				wiimote->register_a6[0x80] = 0x3d;
				wiimote->register_a6[0x81] = 0x46;
				wiimote->register_a6[0x82] = 0x2e;
				wiimote->register_a6[0x83] = 0x3e;
				wiimote->register_a6[0x84] = 0x10;
				wiimote->register_a6[0x85] = 0x1f;
				wiimote->register_a6[0x86] = 0x8e;
				wiimote->register_a6[0x87] = 0x0c;
				wiimote->register_a6[0x88] = 0xf4;
				wiimote->register_a6[0x89] = 0x04;
				wiimote->register_a6[0x8a] = 0x89;
				wiimote->register_a6[0x8b] = 0x4c;
				wiimote->register_a6[0x8c] = 0xca;
				wiimote->register_a6[0x8d] = 0x3e;
				wiimote->register_a6[0x8e] = 0x9f;
				wiimote->register_a6[0x8f] = 0x36;
			}
			break;
		case 0xa6: //motionplus
			reg = wiimote->register_a6;
			memcpy(reg + (offset & 0xff), buf, size);

			if (((offset & 0xff) == 0xfe) && ((buf[0] >> 2) & 0x1)) { //activate wmp
				wiimote->sys.wmp_state = 1;
				wiimote->sys.extension_report_type = (buf[0] & 0x7);
				//printf("activate wmp\n");

				init_extension(wiimote);

				report_queue_push_ack(wiimote, 0x16, 0x00);
				wiimote->sys.extension_connected = 0;
				report_queue_push_status(wiimote);
				wiimote->sys.extension_connected = 1;
				report_queue_push_status(wiimote);
				return;
			}

			break;
		case 0xb0: //ir camera
			reg = wiimote->register_b0;
			memcpy(reg + (offset & 0xff), buf, size);
			break;
		default: //???
		  break;
	}
	report_queue_push_ack(wiimote, 0x16, 0x00);
}


void init_extension(wiimote_t *wiimote) {
	if (wiimote->sys.wmp_state == 1) {
		//wiimote->register_a6[0xfa] = 0x00;
		//wiimote->register_a6[0xfb] = 0x00;
		//wiimote->register_a6[0xfc] = 0xa4;
		//wiimote->register_a6[0xfd] = 0x20;
		//wiimote->register_a6[0xfe] = wiimote->sys.extension_report_type;
		//wiimote->register_a6[0xff] = 0x05;
		wiimote->register_a6[0xfc] = 0xa4;

		wiimote->sys.extension_encrypted = 0;

		// Pulled from Wiimote, not sure what this is for
		wiimote->register_a6[0xf0] = 0x55;
		wiimote->register_a6[0xf1] = 0xff;
		wiimote->register_a6[0xf2] = 0xff;
		wiimote->register_a6[0xf3] = 0xff;
		wiimote->register_a6[0xf4] = 0xff;
		wiimote->register_a6[0xf5] = 0xff;
		wiimote->register_a6[0xf6] = 0x00;

		//a4 40 post init
		wiimote->register_a6[0x40] = 0x81;
		wiimote->register_a6[0x41] = 0x80;
		wiimote->register_a6[0x42] = 0x80;
		wiimote->register_a6[0x43] = 0x28;
		wiimote->register_a6[0x44] = 0xb4;
		wiimote->register_a6[0x45] = 0xb3;
		wiimote->register_a6[0x46] = 0xb3;
		wiimote->register_a6[0x47] = 0x26;
		wiimote->register_a6[0x48] = 0xe3;
		wiimote->register_a6[0x49] = 0x22;
		wiimote->register_a6[0x4a] = 0x7a;
		wiimote->register_a6[0x4b] = 0xd8;
		wiimote->register_a6[0x4c] = 0x1b;
		wiimote->register_a6[0x4d] = 0x81;
		wiimote->register_a6[0x4e] = 0x31;
		wiimote->register_a6[0x4f] = 0x86;

		wiimote->register_a6[0x20] = 0x7c;
		wiimote->register_a6[0x21] = 0x97;
		wiimote->register_a6[0x22] = 0x7f;
		wiimote->register_a6[0x23] = 0x0a;
		wiimote->register_a6[0x24] = 0x7c;
		wiimote->register_a6[0x25] = 0xa8;
		wiimote->register_a6[0x26] = 0x33;
		wiimote->register_a6[0x27] = 0xb7;
		wiimote->register_a6[0x28] = 0xcc;
		wiimote->register_a6[0x29] = 0x12;
		wiimote->register_a6[0x2a] = 0x33;
		wiimote->register_a6[0x2b] = 0x08;
		wiimote->register_a6[0x2c] = 0xc8;
		wiimote->register_a6[0x2d] = 0x01;
		wiimote->register_a6[0x2e] = 0x72;
		wiimote->register_a6[0x2f] = 0xd4;

		wiimote->register_a6[0x30] = 0x7c;
		wiimote->register_a6[0x31] = 0x53;
		wiimote->register_a6[0x32] = 0x87;
		wiimote->register_a6[0x33] = 0x58;
		wiimote->register_a6[0x34] = 0x7c;
		wiimote->register_a6[0x35] = 0x9f;
		wiimote->register_a6[0x36] = 0x36;
		wiimote->register_a6[0x37] = 0xb2;
		wiimote->register_a6[0x38] = 0xc9;
		wiimote->register_a6[0x39] = 0x34;
		wiimote->register_a6[0x3a] = 0x35;
		wiimote->register_a6[0x3b] = 0xf8;
		wiimote->register_a6[0x3c] = 0x2d;
		wiimote->register_a6[0x3d] = 0x60;
		wiimote->register_a6[0x3e] = 0xd7;
		wiimote->register_a6[0x3f] = 0xd5;

		// This may not be needed
		wiimote->register_a6[0x50] = 0x15;
		wiimote->register_a6[0x51] = 0x6d;
		wiimote->register_a6[0x52] = 0xe0;
		wiimote->register_a6[0x53] = 0x23;
		wiimote->register_a6[0x54] = 0x20;
		wiimote->register_a6[0x55] = 0x79;
		wiimote->register_a6[0x56] = 0xd3;
		wiimote->register_a6[0x57] = 0x73;
		wiimote->register_a6[0x58] = 0x01;
		wiimote->register_a6[0x59] = 0xa9;
		wiimote->register_a6[0x5a] = 0xf0;
		wiimote->register_a6[0x5b] = 0x25;
		wiimote->register_a6[0x5c] = 0xb0;
		wiimote->register_a6[0x5d] = 0xbc;
		wiimote->register_a6[0x5e] = 0xff;
		wiimote->register_a6[0x5f] = 0xe1;

		wiimote->register_a6[0x60] = 0xd8;
		wiimote->register_a6[0x61] = 0x3f;
		wiimote->register_a6[0x62] = 0x82;
		wiimote->register_a6[0x63] = 0x52;
		wiimote->register_a6[0x64] = 0x75;
		wiimote->register_a6[0x65] = 0x99;
		wiimote->register_a6[0x66] = 0xbe;
		wiimote->register_a6[0x67] = 0xdb;
		wiimote->register_a6[0x68] = 0xcb;
		wiimote->register_a6[0x69] = 0x61;
		wiimote->register_a6[0x6a] = 0x60;
		wiimote->register_a6[0x6b] = 0x0f;
		wiimote->register_a6[0x6c] = 0x35;
		wiimote->register_a6[0x6d] = 0xbd;
		wiimote->register_a6[0x6e] = 0xd4;
		wiimote->register_a6[0x6f] = 0x4d;

		wiimote->register_a6[0x70] = 0x5c;
		wiimote->register_a6[0x71] = 0x9f;
		wiimote->register_a6[0x72] = 0x5d;
		wiimote->register_a6[0x73] = 0x81;
		wiimote->register_a6[0x74] = 0x71;
		wiimote->register_a6[0x75] = 0xde;
		wiimote->register_a6[0x76] = 0x22;
		wiimote->register_a6[0x77] = 0xe6;
		wiimote->register_a6[0x78] = 0xb9;
		wiimote->register_a6[0x79] = 0x23;
		wiimote->register_a6[0x7a] = 0xa4;
		wiimote->register_a6[0x7b] = 0x58;
		wiimote->register_a6[0x7c] = 0xb7;
		wiimote->register_a6[0x7d] = 0x62;
		wiimote->register_a6[0x7e] = 0x33;
		wiimote->register_a6[0x7f] = 0xa4;

		wiimote->register_a6[0x80] = 0xcd;
		wiimote->register_a6[0x81] = 0x8b;
		wiimote->register_a6[0x82] = 0x3a;
		wiimote->register_a6[0x83] = 0xfe;
		wiimote->register_a6[0x84] = 0x98;
		wiimote->register_a6[0x85] = 0xf0;
		wiimote->register_a6[0x86] = 0xd9;
		wiimote->register_a6[0x87] = 0x57;
		wiimote->register_a6[0x88] = 0x0c;
		wiimote->register_a6[0x89] = 0xe8;
		wiimote->register_a6[0x8a] = 0x27;
		wiimote->register_a6[0x8b] = 0x51;
		wiimote->register_a6[0x8c] = 0xb6;
		wiimote->register_a6[0x8d] = 0xea;
		wiimote->register_a6[0x8e] = 0xe5;
		wiimote->register_a6[0x8f] = 0x78;

		//init progress byte, set it to done
		wiimote->register_a6[0xf7] = 0x0c;
		wiimote->register_a6[0xf8] = 0x00;
		wiimote->register_a6[0xf9] = 0x00;

	} else {
		wiimote->register_a6[0xfa] = 0x00;
		wiimote->register_a6[0xfb] = 0x00;
		wiimote->register_a6[0xfc] = 0xa6;
		wiimote->register_a6[0xfd] = 0x20;
		//wiimote->register_a6[0xfe] = 0x00; //leave this be
		wiimote->register_a6[0xff] = 0x05;

		wiimote->register_a6[0xf7] = 0x0c;
		wiimote->register_a6[0xf8] = 0xff;
		wiimote->register_a6[0xf9] = 0xff;

		wiimote->sys.extension_report_type = wiimote->sys.extension;

		if (wiimote->sys.extension == EXT_NUNCHUK) {
			wiimote->register_a4[0xfa] = 0x00;
			wiimote->register_a4[0xfb] = 0x00;
			wiimote->register_a4[0xfc] = 0xa4;
			wiimote->register_a4[0xfd] = 0x20;
			wiimote->register_a4[0xfe] = 0x00;
			wiimote->register_a4[0xff] = 0x00;
		} else if (wiimote->sys.extension == EXT_CLASSIC) {
			wiimote->register_a4[0xfa] = 0x00;
			wiimote->register_a4[0xfb] = 0x00;
			wiimote->register_a4[0xfc] = 0xa4;
			wiimote->register_a4[0xfd] = 0x20;
			wiimote->register_a4[0xfe] = 0x01;
			wiimote->register_a4[0xff] = 0x01;
		} else {
			wiimote->register_a4[0xfa] = 0xff;
			wiimote->register_a4[0xfb] = 0xff;
			wiimote->register_a4[0xfc] = 0xff;
			wiimote->register_a4[0xfd] = 0xff;
			wiimote->register_a4[0xfe] = 0xff;
			wiimote->register_a4[0xff] = 0xff;
		}
	}
}

void destroy_wiimote(wiimote_t * wiimote) {
	memset(wiimote, 0, sizeof(wiimote_t));
}

void init_wiimote(wiimote_t * wiimote, uint16_t hci_handle) {
	destroy_wiimote(wiimote);

	wiimote->sys.reporting_mode = 0x30;
	wiimote->sys.battery_level = 0xff;

	// Flat
	wiimote->usr.accel_x = 0x80 << 2;
	wiimote->usr.accel_y = 0x80 << 2;
	wiimote->usr.accel_z = 0x98 << 2;

	ir_object_clear(wiimote, 0);
	ir_object_clear(wiimote, 1);
	ir_object_clear(wiimote, 2);
	ir_object_clear(wiimote, 3);

	wiimote->usr.nunchuk.x = 128;
	wiimote->usr.nunchuk.y = 128;
	wiimote->usr.nunchuk.accel_x = 512;
	wiimote->usr.nunchuk.accel_y = 512;
	wiimote->usr.nunchuk.accel_z = 760;

	wiimote->usr.classic.lx = 32;
	wiimote->usr.classic.ly = 32;
	wiimote->usr.classic.rx = 15;
	wiimote->usr.classic.ry = 15;

	wiimote->usr.motionplus.yaw_down = 0x1F7F;
	wiimote->usr.motionplus.roll_left = 0x1F7F;
	wiimote->usr.motionplus.pitch_left = 0x1F7F;
	wiimote->usr.motionplus.yaw_slow = 1;
	wiimote->usr.motionplus.roll_slow = 1;
	wiimote->usr.motionplus.pitch_slow = 1;


	wiimote->sys.extension = EXT_NONE;
	wiimote->sys.extension_connected = 0;
	init_extension(wiimote);

	//report_queue_push_buttons(wiimote);
	if (wiimote->sys.extension_connected) report_queue_push_status(wiimote);

	//wiimote->sys.reporting_enabled = 0;
	//wiimote->sys.feature_ef_byte_6 = 0xa0;

	wiimote->sys.hci_handle = hci_handle;
	wiimote->sys.disconnect_timer = main_timer;
}

uint8_t wiimote_get_player_num(wiimote_t * wiimote) {
	uint8_t led_state = wiimote->sys.led_1 | (wiimote->sys.led_2 << 1) | (wiimote->sys.led_3 << 2) | (wiimote->sys.led_4 << 3);

	switch (led_state) {
		case 0x01:
			return 1;
			break;
		case 0x02:
			return 2;
			break;
		case 0x04:
			return 3;
			break;
		case 0x08:
			return 4;
			break;
		default:
			return 0;
			break;
	}
}

void update_wiimotes() {
	uint8_t input_data[128] = { 0 };

	if (((main_timer - prev_update_time) >= 15) && SPI_EN) {   // Allow 15ms for ESP32 to acknowledge last transfer
		prev_update_time = main_timer;
		uint8_t i, j;

		SPI_CS = 0;	// Begin SPI transaction

		input_data[0] = spi_transfer((wiimotes[0].sys.rumble << 7) | 
						(wiimote_get_player_num(&wiimotes[0]) << 4) | 
						(wiimotes[0].sys.connectable << 3) | 0x01);
		input_data[1] = spi_transfer((wiimotes[1].sys.rumble << 7) | 
						(wiimote_get_player_num(&wiimotes[1]) << 4) | 
						(wiimotes[1].sys.connectable << 3) | 0x02);
		input_data[2] = spi_transfer((wiimotes[2].sys.rumble << 7) | 
						(wiimote_get_player_num(&wiimotes[2]) << 4) | 
						(wiimotes[2].sys.connectable << 3) | 0x03);
		input_data[3] = spi_transfer((wiimotes[3].sys.rumble << 7) | 
						(wiimote_get_player_num(&wiimotes[3]) << 4) | 
						(wiimotes[3].sys.connectable << 3) | 0x04);

		for (i = 4; i < 128; i++) input_data[i] = spi_transfer(0x00);
		
		SPI_CS = 1;	// End SPI transaction

		for (i = 0; i < 4; i++) {        
			// Allow controller to connect unless Wiimote was just initialized
			if (main_timer - wiimotes[i].sys.disconnect_timer >= 500) wiimotes[i].sys.connectable = hci_get_connectable_status();
			
			if ((input_data[32 * i] & 0x07) == i + 1) { // Check if controller is connected on ESP32
				// Initiate connection with Wii
				if (!wiimotes[i].sys.hci_connection_requested && 
					 wiimotes[i].sys.connectable && 
					(main_timer - wiimotes[i].sys.disconnect_timer >= 1000)) {
					hci_queue_evt(HCI_CONNECTION_REQUEST, 0, wiimotes[i].sys.hci_handle);
					uart_transmit("Wiimote ", 0);
					uart_transmit_val(i + 1, 1, 0);
					uart_transmit(" connecting", 1);
					wiimotes[i].sys.hci_connection_requested = 1;
					wiimotes[i].sys.l2cap_role = 1;
				}

				// Auto-connect failed, controller must sync
				if (wiimotes[i].sys.hci_connection_failed && wiimotes[i].sys.l2cap_connection_failed) {
					hci_queue_evt(0xFF, 0, wiimotes[i].sys.hci_handle); // Queue sync button press event
					wiimotes[i].sys.hci_connection_failed = 0;
					wiimotes[i].sys.l2cap_connection_failed = 0;
					wiimotes[i].sys.l2cap_role = 0;
					wiimotes[i].sys.syncing = 1;
					uart_transmit("Wiimote ", 0);
					uart_transmit_val(i + 1, 1, 0);
					uart_transmit(" syncing", 1);
				}
				
				// Wii has terminated the connection
				if (wiimotes[i].sys.connected && wiimotes[i].sys.hci_connection_failed) {
					init_wiimote(&wiimotes[i], wiimotes[i].sys.hci_handle);  // Reset Wiimote
					uart_transmit("Wiimote ", 0);
					uart_transmit_val(i + 1, 1, 0);
					uart_transmit(" disconnected", 1);
				}

				uint8_t checksum_buttons = 0;
				for (j = 0; j < 4; j++) checksum_buttons += input_data[j + 1 + (32 * i)];
				checksum_buttons += 0x55;

				if (checksum_buttons == input_data[30 + (32 * i)]) {    // Verify checksum
					if ((input_data[32 * i] >> 4) != wiimotes[i].sys.extension) {
						wiimotes[i].sys.extension = input_data[32 * i] >> 4;
						init_extension(&wiimotes[i]);
						wiimotes[i].sys.extension_connected = 0;
						report_queue_push_status(&wiimotes[i]);
						wiimotes[i].sys.extension_connected = 1;
						report_queue_push_status(&wiimotes[i]);
					}

					wiimotes[i].usr.a = input_data[1 + (32 * i)] & 0x01;
					wiimotes[i].usr.b = (input_data[1 + (32 * i)] & 0x02) >> 1;
					wiimotes[i].usr.one = (input_data[1 + (32 * i)] & 0x04) >> 2;
					wiimotes[i].usr.two = (input_data[1 + (32 * i)] & 0x08) >> 3;
					wiimotes[i].usr.up = (input_data[1 + (32 * i)] & 0x10) >> 4;
					wiimotes[i].usr.down = (input_data[1 + (32 * i)] & 0x20) >> 5;
					wiimotes[i].usr.right = (input_data[1 + (32 * i)] & 0x40) >> 6;
					wiimotes[i].usr.left = (input_data[1 + (32 * i)] & 0x80) >> 7;
					wiimotes[i].usr.plus = input_data[2 + (32 * i)] & 0x01;
					wiimotes[i].usr.minus = (input_data[2 + (32 * i)] & 0x02) >> 1;
					wiimotes[i].usr.home = (input_data[2 + (32 * i)] & 0x04) >> 2;
					wiimotes[i].usr.classic.plus = (input_data[2 + (32 * i)] & 0x08) >> 3;
					wiimotes[i].usr.classic.minus = (input_data[2 + (32 * i)] & 0x10) >> 4;
					wiimotes[i].usr.classic.home = (input_data[2 + (32 * i)] & 0x20) >> 5;
					wiimotes[i].usr.nunchuk.c = (input_data[2 + (32 * i)] & 0x40) >> 6;
					wiimotes[i].usr.nunchuk.z = (input_data[2 + (32 * i)] & 0x80) >> 7;
					wiimotes[i].usr.classic.a = input_data[3 + (32 * i)] & 0x01;
					wiimotes[i].usr.classic.b = (input_data[3 + (32 * i)] & 0x02) >> 1;
					wiimotes[i].usr.classic.x = (input_data[3 + (32 * i)] & 0x04) >> 2;
					wiimotes[i].usr.classic.y = (input_data[3 + (32 * i)] & 0x08) >> 3;
					wiimotes[i].usr.classic.up = (input_data[3 + (32 * i)] & 0x10) >> 4;
					wiimotes[i].usr.classic.down = (input_data[3 + (32 * i)] & 0x20) >> 5;
					wiimotes[i].usr.classic.right = (input_data[3 + (32 * i)] & 0x40) >> 6;
					wiimotes[i].usr.classic.left = (input_data[3 + (32 * i)] & 0x80) >> 7;
					wiimotes[i].usr.classic.r = input_data[4 + (32 * i)] & 0x01;
					wiimotes[i].usr.classic.zr = (input_data[4 + (32 * i)] & 0x02) >> 1;
					wiimotes[i].usr.classic.l = (input_data[4 + (32 * i)] & 0x04) >> 2;
					wiimotes[i].usr.classic.zl = (input_data[4 + (32 * i)] & 0x08) >> 3;

					// Extra mappings
					if (wiimotes[i].sys.extension != EXT_CLASSIC) wiimotes[i].usr.b |= wiimotes[i].usr.classic.zr;

					wiimotes[i].usr.classic.lx = input_data[5 + (32 * i)] >> 2;
					wiimotes[i].usr.classic.ly = input_data[6 + (32 * i)] >> 2;
					wiimotes[i].usr.classic.rx = input_data[7 + (32 * i)] >> 3;
					wiimotes[i].usr.classic.ry = input_data[8 + (32 * i)] >> 3;
					wiimotes[i].usr.nunchuk.x = input_data[5 + (32 * i)];
					wiimotes[i].usr.nunchuk.y = input_data[6 + (32 * i)];

					wiimotes[i].usr.accel_x = input_data[9 + (32 * i)] | ((input_data[12 + (32 * i)] & 0x03) << 8);
					wiimotes[i].usr.accel_y = input_data[10 + (32 * i)] | ((input_data[12 + (32 * i)] & 0x0C) << 6);
					wiimotes[i].usr.accel_z = input_data[11 + (32 * i)] | ((input_data[12 + (32 * i)] & 0x30) << 4);

					wiimotes[i].usr.nunchuk.accel_x = input_data[13 + (32 * i)] | ((input_data[16 + (32 * i)] & 0x03) << 8);
					wiimotes[i].usr.nunchuk.accel_y = input_data[14 + (32 * i)] | ((input_data[16 + (32 * i)] & 0x0C) << 6);
					wiimotes[i].usr.nunchuk.accel_z = input_data[15 + (32 * i)] | ((input_data[16 + (32 * i)] & 0x30) << 4);

					wiimotes[i].usr.ir_object[0].x = input_data[22 + (32 * i)] | (input_data[23 + (32 * i)] << 8);
					wiimotes[i].usr.ir_object[0].y = input_data[24 + (32 * i)] | (input_data[25 + (32 * i)] << 8);
					wiimotes[i].usr.ir_object[0].size = 8;
					wiimotes[i].usr.ir_object[1].x = input_data[26 + (32 * i)] | (input_data[27 + (32 * i)] << 8);
					wiimotes[i].usr.ir_object[1].y = input_data[28 + (32 * i)] | (input_data[29 + (32 * i)] << 8);
					wiimotes[i].usr.ir_object[1].size = 8;
					wiimotes[i].usr.ir_object[2].x = 0xFFFF;
					wiimotes[i].usr.ir_object[2].y = 0xFFFF;
					wiimotes[i].usr.ir_object[2].size = 0;
					wiimotes[i].usr.ir_object[3].x = 0xFFFF;
					wiimotes[i].usr.ir_object[3].y = 0xFFFF;
					wiimotes[i].usr.ir_object[3].size = 0;
				}  
			} else {    // Controller is not connected
				if (wiimotes[i].sys.connected) {
					hci_queue_evt(HCI_DISCONNECTION_COMPLETE, 0, wiimotes[i].sys.hci_handle);   // Terminate connection
					init_wiimote(&wiimotes[i], wiimotes[i].sys.hci_handle);  // Reset Wiimote
					uart_transmit("Wiimote ", 0);
					uart_transmit_val(i + 1, 1, 0);
					uart_transmit(" disconnected", 1);
				}
			}
		}
	} 
}
