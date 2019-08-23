#ifndef _PAIR_H_
#define	_PAIR_H_

#include "hid_controller.h"

#define INQUIRY_INTERVAL 4

extern uint8_t host_mac_addr[6];
extern uint8_t host_mac_addr_rev[6];
extern char host_mac_string[7];

enum INQUIRY_STATE { SEARCHING, REQUESTING_NAMES, READY_TO_PAIR, PAIRING, IDLE };
enum DEVICE_STATE { REMOTE_NAME_REQUEST, REMOTE_NAME_FOUND };
typedef struct {
    uint8_t address[6];
    uint8_t page_scan_repetition_mode;
    uint16_t clock_offset;
    enum HID_DEVICE controller_type;
    enum DEVICE_STATE state; 
} device;

void store_link_key(hid_controller_t * controller);
uint8_t ready_to_pair();
uint8_t get_num_controllers_found();
uint8_t * get_next_discovered_controller_addr(enum HID_DEVICE * controller_type);
void begin_pair(hid_controller_t * controller);
void end_pair(hid_controller_t * controller);
int get_device_index_for_address(uint8_t * addr);
uint8_t has_more_remote_name_requests();
void do_next_remote_name_request();
void start_scan(void);
void stop_scan();
void init_devices();
void pair_timeout_check();
enum HID_DEVICE get_discovered_device_type(char * name);
void store_controller_type(enum HID_DEVICE type, uint8_t * addr);
enum HID_DEVICE get_controller_type(uint8_t * addr);
void gap_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

#endif