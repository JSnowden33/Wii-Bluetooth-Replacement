#include "wm_reports.h"
#include "wm_crypto.h"

#include <stdlib.h>
#include <string.h>
#include <sys/endian.h>

#define REPORT_MEM_SIZE 64
static struct report_mem reports[64] = {0};

static struct queued_report * malloc_rpt() {
	int i = 0;
	for (i = 0; i < REPORT_MEM_SIZE; i++) {
		if (reports[i].allocated == 0) {
			reports[i].allocated = 1;
			return (struct queued_report *)&reports[i];
		}
	}
}

static void free_rpt(struct queued_report * rpt) {
	struct report_mem * rpt_mem = (struct report_mem *)rpt;
	rpt_mem->allocated = 0;
}

struct report * report_queue_push(wiimote_t * wiimote) {
	struct queued_report * rpt;

	//allocate new report
	rpt = malloc_rpt();
	memset(rpt, 0, sizeof(struct queued_report));

	//append to the end of the queue
	if (wiimote->sys.queue_end != NULL) wiimote->sys.queue_end->next = rpt;
	else wiimote->sys.queue = rpt;

	wiimote->sys.queue_end = rpt;

	return &(rpt->rpt);
}

struct report * report_queue_peek(wiimote_t * wiimote) {
	if (wiimote->sys.queue == NULL) return NULL; //empty queue
	return &(wiimote->sys.queue->rpt);
}

void report_queue_pop(wiimote_t * wiimote) {
	struct queued_report * rpt;

	if (wiimote->sys.queue == NULL) return; //nothing to remove

	//remove from the queue
	rpt = wiimote->sys.queue;
	if (wiimote->sys.queue_end == rpt) wiimote->sys.queue_end = NULL;
	wiimote->sys.queue = rpt->next;

	//free report mem
	free_rpt(rpt);
}

void report_queue_push_ack(wiimote_t * wiimote, uint8_t report, uint8_t result) {
	//push acknowledgement report x22
	struct report * rpt = report_queue_push(wiimote);
	rpt->len = 6;
	rpt->data.io = 0xa1;
	rpt->data.type = 0x22;

	struct report_ack * ack = (struct report_ack *)rpt->data.buf;
	ack->report = report;
	ack->result = result;
}

void report_queue_push_status(wiimote_t * wiimote) {
	//push status report x20
	struct report * rpt = report_queue_push(wiimote);
	rpt->len = 8;
	rpt->data.io = 0xa1;
	rpt->data.type = 0x20;

	struct report_status * status = (struct report_status *)rpt->data.buf;
	status->low_battery         = wiimote->sys.low_battery;
	status->extension_connected = wiimote->sys.extension_connected;
	status->speaker_enabled     = wiimote->sys.speaker_enabled;
	status->ircam_enabled       = wiimote->sys.ircam_enabled;
	status->led_1               = wiimote->sys.led_1;
	status->led_2               = wiimote->sys.led_2;
	status->led_3               = wiimote->sys.led_3;
	status->led_4               = wiimote->sys.led_4;
	status->battery_level       = wiimote->sys.battery_level;
}

void report_queue_push_buttons(wiimote_t * wiimote) {
	struct report * rpt = report_queue_push(wiimote);
	rpt->len = 2;
	rpt->data.io = 0xa1;
	rpt->data.type = 0x30; 
}

void report_format_mem_resp(struct report * rpt, int size, int error, uint16_t addr, const uint8_t * buf) {
	struct report_mem_resp * resp = (struct report_mem_resp *)rpt->data.buf;

	rpt->len = 23;
	rpt->data.io = 0xa1;
	rpt->data.type = 0x21;

	resp->size = size-1;
	resp->error = error;
	resp->addr = ntohs(addr); //endianness
	if (buf != NULL) memcpy(resp->data, buf, size);	// buf will be null for error reports
}

void report_append_buttons(wiimote_t * wiimote, uint8_t * buf) {
	struct report_buttons * rpt = (struct report_buttons *)buf;

	rpt->left  = wiimote->usr.left;
	rpt->right = wiimote->usr.right;
	rpt->down  = wiimote->usr.down;
	rpt->up    = wiimote->usr.up;
	rpt->one   = wiimote->usr.one;
	rpt->two   = wiimote->usr.two;
	rpt->a     = wiimote->usr.a;
	rpt->b     = wiimote->usr.b;
	rpt->plus  = wiimote->usr.plus;
	rpt->minus = wiimote->usr.minus;
	rpt->home  = wiimote->usr.home;
}

void report_append_accelerometer(wiimote_t * wiimote, uint8_t * buf) {
	struct report_accelerometer * rpt = (struct report_accelerometer *)buf;

	rpt->buttons.accel_0 = wiimote->usr.accel_x;
	rpt->buttons.accel_1 = (wiimote->usr.accel_z & 0x2) |
						   ((wiimote->usr.accel_y >> 1) & 0x1);

	rpt->x = wiimote->usr.accel_x >> 2;
	rpt->y = wiimote->usr.accel_y >> 2;
	rpt->z = wiimote->usr.accel_z >> 2;
}

void report_append_ir_10(wiimote_t * wiimote, uint8_t * buf) {
	struct report_ir_basic * rpt = (struct report_ir_basic *)buf;

	rpt->x1_lo = wiimote->usr.ir_object[0].x;
	rpt->y1_lo = wiimote->usr.ir_object[0].y;
	rpt->x1_hi = wiimote->usr.ir_object[0].x >> 8;
	rpt->y1_hi = wiimote->usr.ir_object[0].y >> 8;

	rpt->x2_lo = wiimote->usr.ir_object[1].x;
	rpt->y2_lo = wiimote->usr.ir_object[1].y;
	rpt->x2_hi = wiimote->usr.ir_object[1].x >> 8;
	rpt->y2_hi = wiimote->usr.ir_object[1].y >> 8;

	rpt->x3_lo = wiimote->usr.ir_object[2].x;
	rpt->y3_lo = wiimote->usr.ir_object[2].y;
	rpt->x3_hi = wiimote->usr.ir_object[2].x >> 8;
	rpt->y3_hi = wiimote->usr.ir_object[2].y >> 8;

	rpt->x4_lo = wiimote->usr.ir_object[3].x;
	rpt->y4_lo = wiimote->usr.ir_object[3].y;
	rpt->x4_hi = wiimote->usr.ir_object[3].x >> 8;
	rpt->y4_hi = wiimote->usr.ir_object[3].y >> 8;
}

void report_append_ir_12(wiimote_t * wiimote, uint8_t * buf) {
	struct report_ir_ext * rpt = (struct report_ir_ext *)buf;
	int i;

	for (i=0; i<4; i++) {
		rpt->obj[i].x_lo = wiimote->usr.ir_object[i].x;
		rpt->obj[i].y_lo = wiimote->usr.ir_object[i].y;
		rpt->obj[i].x_hi = wiimote->usr.ir_object[i].x >> 8;
		rpt->obj[i].y_hi = wiimote->usr.ir_object[i].y >> 8;
		rpt->obj[i].size = wiimote->usr.ir_object[i].size;
	}
}

void report_append_interleaved(wiimote_t * wiimote, uint8_t * buf) {
	struct report_interleaved * rpt = (struct report_interleaved *)buf;
	int i;

	if (wiimote->sys.reporting_mode == 0x3e) {
		rpt->buttons.accel_0 = wiimote->usr.accel_z >> 4;
		rpt->buttons.accel_1 = wiimote->usr.accel_z >> 6;
		rpt->accel = wiimote->usr.accel_x >> 2;

		for (i=0; i<2; i++) {
			rpt->obj[i].x_lo = wiimote->usr.ir_object[i].x;
			rpt->obj[i].y_lo = wiimote->usr.ir_object[i].y;
			rpt->obj[i].x_hi = wiimote->usr.ir_object[i].x >> 8;
			rpt->obj[i].y_hi = wiimote->usr.ir_object[i].y >> 8;
			rpt->obj[i].size = wiimote->usr.ir_object[i].size;
			rpt->obj[i].x_min = wiimote->usr.ir_object[i].xmin;
			rpt->obj[i].y_min = wiimote->usr.ir_object[i].ymin;
			rpt->obj[i].y_max = wiimote->usr.ir_object[i].xmax;
			rpt->obj[i].y_max = wiimote->usr.ir_object[i].ymax;
			rpt->obj[i].intensity = wiimote->usr.ir_object[i].intensity;
		}
		wiimote->sys.reporting_mode = 0x3f;
	} else {
		rpt->buttons.accel_0 = wiimote->usr.accel_z;
		rpt->buttons.accel_1 = wiimote->usr.accel_z >> 2;
		rpt->accel = wiimote->usr.accel_y >> 2;

		for (i=0; i<2; i++) {
			rpt->obj[i].x_lo = wiimote->usr.ir_object[i+2].x;
			rpt->obj[i].y_lo = wiimote->usr.ir_object[i+2].y;
			rpt->obj[i].x_hi = wiimote->usr.ir_object[i+2].x >> 8;
			rpt->obj[i].y_hi = wiimote->usr.ir_object[i+2].y >> 8;
			rpt->obj[i].size = wiimote->usr.ir_object[i+2].size;
			rpt->obj[i].x_min = wiimote->usr.ir_object[i+2].xmin;
			rpt->obj[i].y_min = wiimote->usr.ir_object[i+2].ymin;
			rpt->obj[i].y_max = wiimote->usr.ir_object[i+2].xmax;
			rpt->obj[i].y_max = wiimote->usr.ir_object[i+2].ymax;
			rpt->obj[i].intensity = wiimote->usr.ir_object[i+2].intensity;
		}
		wiimote->sys.reporting_mode = 0x3e;
	}
}

void report_append_extension(wiimote_t * wiimote, uint8_t * buf, uint8_t bytes) {
	//a600fe = 0x04 activate motionplus, 0x05 activate nunchuk passthrough, 0x07 activate classic passthrough
		//if no other extension, send 0x20

	//a600f0 = 0x55 deactivate motionplus
		//send report 0x20 twice (once for unplugged, once for plugged in)

	//0xa400fa contents
	//0000 A420 0000   Nunchuk
	//0000 A420 0101    Classic
	//0000 A420 0405    WMP
	//0000 A420 0505    WMP nunchuk
	//0000 A420 0705    WMP classic

	//0000 A620 0005    Inactive WMP
	//      ^=6         Deactivated WMP

	//memset(buf + 6, 0, sizeof(uint8_t) * (bytes - 6));
	//memset(buf + offset + 6, 0, sizeof(uint8_t) * (bytes - 6));

	switch (wiimote->sys.extension_report_type) {
		case 0x01: {	// Nunchuk
			struct report_ext_nunchuk * rpt = (struct report_ext_nunchuk *)buf;

			rpt->x = wiimote->usr.nunchuk.x;
			rpt->y = wiimote->usr.nunchuk.y;
			rpt->accel_x_hi = wiimote->usr.nunchuk.accel_x >> 2;
			rpt->accel_y_hi = wiimote->usr.nunchuk.accel_y >> 2;
			rpt->accel_z_hi = wiimote->usr.nunchuk.accel_z >> 2;
			rpt->accel_x_lo = wiimote->usr.nunchuk.accel_x;
			rpt->accel_y_lo = wiimote->usr.nunchuk.accel_y;
			rpt->accel_z_lo = wiimote->usr.nunchuk.accel_z;
			rpt->c = !wiimote->usr.nunchuk.c;
			rpt->z = !wiimote->usr.nunchuk.z;

			break;
		} case 0x02: {	// Classic
			struct report_ext_classic * rpt = (struct report_ext_classic *)buf;

			rpt->lx = wiimote->usr.classic.lx;
			rpt->ly = wiimote->usr.classic.ly;
			rpt->rx_hi = wiimote->usr.classic.rx >> 3;
			rpt->rx_m = wiimote->usr.classic.rx >> 1;
			rpt->rx_lo = wiimote->usr.classic.rx;
			rpt->ry = wiimote->usr.classic.ry;

			rpt->lt_hi = wiimote->usr.classic.lt >> 3;
			rpt->lt_lo = wiimote->usr.classic.lt;
			rpt->rt = wiimote->usr.classic.rt;

			rpt->left = !wiimote->usr.classic.left;
			rpt->right = !wiimote->usr.classic.right;
			rpt->up = !wiimote->usr.classic.up;
			rpt->down = !wiimote->usr.classic.down;
			rpt->l = !wiimote->usr.classic.l;
			rpt->r = !wiimote->usr.classic.r;
			rpt->zl = !wiimote->usr.classic.zl;
			rpt->zr = !wiimote->usr.classic.zl;
			rpt->a = !wiimote->usr.classic.a;
			rpt->b = !wiimote->usr.classic.b;
			rpt->x = !wiimote->usr.classic.x;
			rpt->y = !wiimote->usr.classic.y;
			rpt->plus = !wiimote->usr.classic.plus;
			rpt->minus = !wiimote->usr.classic.minus;
			rpt->home = !wiimote->usr.classic.home;

			rpt->unused = 1;

			break;
		} case 0x04: {	// WMP
			struct report_ext_motionplus * rpt = (struct report_ext_motionplus *)buf;

			rpt->yaw_hi = wiimote->usr.motionplus.yaw_down >> 8;
			rpt->yaw_lo = wiimote->usr.motionplus.yaw_down;
			rpt->roll_hi = wiimote->usr.motionplus.roll_left >> 8;
			rpt->roll_lo = wiimote->usr.motionplus.roll_left;
			rpt->pitch_hi = wiimote->usr.motionplus.pitch_left >> 8;
			rpt->pitch_lo = wiimote->usr.motionplus.pitch_left;

			rpt->yaw_slow = wiimote->usr.motionplus.yaw_slow;
			rpt->pitch_slow = wiimote->usr.motionplus.pitch_slow;
			rpt->roll_slow = wiimote->usr.motionplus.roll_slow;

			rpt->ext = 0;
			rpt->unused_0 = 1;

			break;
		} case 0x05: // Nunchuk + WMP
			if (wiimote->sys.extension_report) {
				struct report_ext_motionplus * rpt = (struct report_ext_motionplus *)buf;

				rpt->yaw_hi = wiimote->usr.motionplus.yaw_down >> 8;
				rpt->yaw_lo = wiimote->usr.motionplus.yaw_down;
				rpt->roll_hi = wiimote->usr.motionplus.roll_left >> 8;
				rpt->roll_lo = wiimote->usr.motionplus.roll_left;
				rpt->pitch_hi = wiimote->usr.motionplus.pitch_left >> 8;
				rpt->pitch_lo = wiimote->usr.motionplus.pitch_left;

				rpt->yaw_slow = wiimote->usr.motionplus.yaw_slow;
				rpt->pitch_slow = wiimote->usr.motionplus.pitch_slow;
				rpt->roll_slow = wiimote->usr.motionplus.roll_slow;

				rpt->ext = 1;
				rpt->unused_0 = 1;

				wiimote->sys.extension_report = 0;
			} else {
				struct report_ext_nunchuk_pt * rpt = (struct report_ext_nunchuk_pt *)buf;

				rpt->x = wiimote->usr.nunchuk.x;
				rpt->y = wiimote->usr.nunchuk.y;
				rpt->accel_x_hi = wiimote->usr.nunchuk.accel_x >> 2;
				rpt->accel_y_hi = wiimote->usr.nunchuk.accel_y >> 2;
				rpt->accel_z_hi = wiimote->usr.nunchuk.accel_z >> 3;
				rpt->accel_x_lo = wiimote->usr.nunchuk.accel_x >> 1;
				rpt->accel_y_lo = wiimote->usr.nunchuk.accel_y >> 1;
				rpt->accel_z_lo = wiimote->usr.nunchuk.accel_z >> 1;
				rpt->c = !wiimote->usr.nunchuk.c;
				rpt->z = !wiimote->usr.nunchuk.z;

				rpt->ext = 1;

				wiimote->sys.extension_report = 1;
			}
			break;
		case 0x07: // Classic + WMP
			if (wiimote->sys.extension_report) {
				struct report_ext_motionplus * rpt = (struct report_ext_motionplus *)buf;

				rpt->yaw_hi = wiimote->usr.motionplus.yaw_down >> 8;
				rpt->yaw_lo = wiimote->usr.motionplus.yaw_down;
				rpt->roll_hi = wiimote->usr.motionplus.roll_left >> 8;
				rpt->roll_lo = wiimote->usr.motionplus.roll_left;
				rpt->pitch_hi = wiimote->usr.motionplus.pitch_left >> 8;
				rpt->pitch_lo = wiimote->usr.motionplus.pitch_left;

				rpt->yaw_slow = wiimote->usr.motionplus.yaw_slow;
				rpt->pitch_slow = wiimote->usr.motionplus.pitch_slow;
				rpt->roll_slow = wiimote->usr.motionplus.roll_slow;

				rpt->ext = 1;
				rpt->unused_0 = 1;

				wiimote->sys.extension_report = 0;
			} else {
				struct report_ext_classic_pt * rpt = (struct report_ext_classic_pt *)buf;

				rpt->lx = wiimote->usr.classic.lx >> 1;
				rpt->ly = wiimote->usr.classic.ly >> 1;
				rpt->rx_hi = wiimote->usr.classic.rx >> 3;
				rpt->rx_m = wiimote->usr.classic.rx >> 1;
				rpt->rx_lo = wiimote->usr.classic.rx;
				rpt->ry = wiimote->usr.classic.ry;

				rpt->lt_hi = wiimote->usr.classic.lt >> 3;
				rpt->lt_lo = wiimote->usr.classic.lt;
				rpt->rt = wiimote->usr.classic.rt;

				rpt->left = !wiimote->usr.classic.left;
				rpt->right = !wiimote->usr.classic.right;
				rpt->up = !wiimote->usr.classic.up;
				rpt->down = !wiimote->usr.classic.down;
				rpt->l = !wiimote->usr.classic.l;
				rpt->r = !wiimote->usr.classic.r;
				rpt->zl = !wiimote->usr.classic.zl;
				rpt->zr = !wiimote->usr.classic.zr;
				rpt->a = !wiimote->usr.classic.a;
				rpt->b = !wiimote->usr.classic.b;
				rpt->x = !wiimote->usr.classic.x;
				rpt->y = !wiimote->usr.classic.y;
				rpt->plus = !wiimote->usr.classic.plus;
				rpt->minus = !wiimote->usr.classic.minus;
				rpt->home = !wiimote->usr.classic.home;

				rpt->ext = 1;

				wiimote->sys.extension_report = 1;
			}
			break;
	}

	// Only first 6 extension bytes are encrypted for now
	if (wiimote->sys.extension_encrypted) {
	  int i;
	  for (i=0;i<6;i++) {
		//buf[i] = (buf[i] - wiimote->ft[(0x08 + i)%8]) ^ wiimote->sb[(0x08 + i)%8];
		//above is technically the full operation, below is equivalent (as of now)
		buf[i] = (buf[i] - wiimote->ft[i]) ^ wiimote->sb[i];
	  }
	}
}

