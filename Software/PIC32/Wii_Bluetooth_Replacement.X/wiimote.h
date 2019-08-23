#ifndef WIIMOTE_H
#define	WIIMOTE_H

#include <stdint.h>
#include <stdbool.h>

enum EXTENSION_TYPE { 
	EXT_NONE = 0, 
	EXT_NUNCHUK = 1, 
	EXT_CLASSIC = 2 
};

struct wiimote_ir_object {
	uint16_t x;
	uint16_t y;
	uint8_t size;
	uint8_t xmin;
	uint8_t ymin;
	uint8_t xmax;
	uint8_t ymax;
	uint8_t intensity;
};

struct wiimote_nunchuk {
	uint16_t accel_x;
	uint16_t accel_y;
	uint16_t accel_z;
	uint8_t x;
	uint8_t y;
	bool c;
	bool z;
};

struct wiimote_classic {
	bool a;
	bool b;
	bool x;
	bool y;
	bool minus;
	bool plus;
	bool r;
	bool l;
	bool home;
	bool zr;
	bool zl;
	bool up;
	bool down;
	bool left;
	bool right;
	uint8_t lx;
	uint8_t ly;
	uint8_t rx;
	uint8_t ry;
	uint8_t lt;
	uint8_t rt;
};

struct wiimote_motionplus {
	uint16_t yaw_down;
	uint16_t roll_left;
	uint16_t pitch_left;
	bool yaw_slow;
	bool roll_slow;
	bool pitch_slow;
};

struct wiimote_state_usr {
	bool a;
	bool b;
	bool minus;
	bool plus;
	bool home;
	bool one;
	bool two;
	bool up;
	bool down;
	bool left;
	bool right;

	// Accelerometer (10 bit range, unsigned)
	uint16_t accel_x;
	uint16_t accel_y;
	uint16_t accel_z;

	// Four IR camera dots
	// x, y, size, x min, y min, x max, y max, intensity
	struct wiimote_ir_object ir_object[4];

	struct wiimote_nunchuk nunchuk;
	struct wiimote_classic classic;
	struct wiimote_motionplus motionplus;
};

struct wiimote_state_sys {
	uint8_t connectable;
	uint8_t connected;
	uint32_t disconnect_timer;
	uint8_t hci_connection_failed;
	uint8_t l2cap_connection_failed;
	uint8_t hci_connection_requested;
	uint16_t hci_handle;
	uint8_t l2cap_role;   // 0 = connecting via sync, 1 = auto connect
	uint8_t syncing;

	//controller led status
	bool led_1;
	bool led_2;
	bool led_3;
	bool led_4;

	bool rumble;

	bool ircam_enabled;
	bool speaker_enabled;

	uint8_t battery_level;
	bool low_battery;

	bool extension_connected;
	bool extension_report;
	bool extension_encrypted;
	enum EXTENSION_TYPE extension;
	uint8_t extension_report_type;
	uint8_t wmp_state; //0 inactive, 1 active, 2 deactivated

	uint8_t reporting_mode;
	bool reporting_continuous;
	bool report_changed;

	struct queued_report * queue;
	struct queued_report * queue_end;
	uint32_t last_report_time;
	
	uint8_t tries;

	// Extensions:
	// none, nunchuk, classic, wm+, wm+ and nunchuk, wm+ and classic
};

typedef struct {
	struct wiimote_state_sys sys;
	struct wiimote_state_usr usr;

	uint8_t register_a2[0x09 + 1]; // Speaker
	uint8_t register_a4[0xff + 1]; // Extension
	uint8_t register_a6[0xff + 1]; // Wii motion plus
	uint8_t register_b0[0x33 + 1]; // IR camera
	
	// Crypto tables
	uint8_t ft[8];
	uint8_t sb[8];
} wiimote_t;

extern wiimote_t wiimotes[4];

wiimote_t * get_wiimote_from_handle(uint16_t hci_handle);

void _wiimote_recv_ctrl(uint16_t hci_handle, const uint8_t *buf, int len);
int32_t _wiimote_get_ctrl(uint16_t hci_handle, uint8_t * buf);
void _wiimote_recv_data(uint16_t hci_handle, const uint8_t * buf, int len);
int32_t _wiimote_get_data(uint16_t hci_handle, uint8_t * buf);

int wiimote_recv_report(wiimote_t * wiimote, const uint8_t * buf, int len);
int wiimote_get_report(wiimote_t * wiimote, uint8_t * buf);

void ir_object_clear(wiimote_t * wiimote, uint8_t num);

void read_eeprom(wiimote_t * wiimote, uint32_t offset, uint16_t size);
void write_eeprom(wiimote_t * wiimote, uint32_t offset, uint8_t size, const uint8_t * buf);
void read_register(wiimote_t * wiimote, uint32_t offset, uint16_t size);
void write_register(wiimote_t * wiimote, uint32_t offset, uint8_t size, const uint8_t * buf);

void destroy_wiimote(wiimote_t * wiimote);
void init_extension(wiimote_t * wiimote);
void init_wiimote(wiimote_t * wiimote, uint16_t hci_handle);
void update_wiimotes();

#endif	/* WIIMOTE_H */
