#include <inttypes.h>
#include <stdio.h>
#include "btstack.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "connect.h"
#include "hid_controller.h"
#include "pair.h"
#include "timer.h"

uint8_t host_mac_addr[6];
uint8_t host_mac_addr_rev[6];
char host_mac_string[7];

device device1, device2, device3, device4, device5, device6, device7, device8;
device * device_list[8] = { &device1, &device2, &device3, &device4, &device5, &device6, &device7, &device8 };

enum INQUIRY_STATE inquiry_state = IDLE;
int device_count = 0;
uint8_t controllers_found = 0;
hid_controller_t * device_to_pair = NULL;
uint32_t pair_timeout;

void store_link_key(hid_controller_t * controller) {
    uint8_t i;
    uint8_t link_key[16];
    for (i = 0; i < 16; i++) link_key[i] = controller->command_response[32 - i] ^ 0xAA;	// Read link key little-endian, XOR each byte with 0xAA
    printf("Link key: ");
    printf_hexdump(link_key, 16);
    gap_store_link_key_for_bd_addr(controller->address, link_key, COMBINATION_KEY);
}

uint8_t ready_to_pair() {
    if (inquiry_state == READY_TO_PAIR) return 1;
    return 0;
}

uint8_t get_num_controllers_found() {
    return controllers_found;
}

uint8_t * get_next_discovered_controller_addr(enum HID_DEVICE * controller_type) {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        if (device_list[i]->controller_type != CNT_NONE) {
            *controller_type = device_list[i]->controller_type;
            device_list[i]->controller_type = CNT_NONE;
            return device_list[i]->address;
        }
    }
    return NULL;
}

void begin_pair(hid_controller_t * controller) {
    inquiry_state = PAIRING;
    device_to_pair = controller;    // Keep track of which controller is pairing in case of timeout event
	controller->pairing = 1;
	if (l2cap_create_channel(l2cap_event_handler, controller->address, HID_CONTROL_PSM, 64, &controller->l2cap_control_cid)) end_pair(controller);
    else printf("Pairing - L2CAP control channel created\n");
}

void end_pair(hid_controller_t * controller) {
    if (controller) {
        controller->pairing = 0;
        printf("Pairing complete\n");
    }
    device_to_pair = NULL;
    if (controllers_found > 0) controllers_found--;
    if (controllers_found > 0) {
        inquiry_state = READY_TO_PAIR;  // Allow next discovered controller to pair
    } else {
        inquiry_state = IDLE;
        gap_connectable_control(1);
    }
}

int get_device_index_for_address(uint8_t * addr) {
    uint8_t i;
    for (i = 0; i < device_count; i++){
        if (bd_addr_cmp(addr, device_list[i]->address) == 0) return i;
    }
    return -1;
}

uint8_t has_more_remote_name_requests() {
    uint8_t i;
    for (i = 0; i < device_count; i++) {
        if (device_list[i]->state == REMOTE_NAME_REQUEST) return 1;
    }
    return 0;
}

void do_next_remote_name_request() {
    uint8_t i;
    for (i = 0; i < device_count; i++) {
        if (device_list[i]->state == REMOTE_NAME_REQUEST) {
            printf("Get remote name of %s...\n", bd_addr_to_str(device_list[i]->address));
            gap_remote_name_request(device_list[i]->address, device_list[i]->page_scan_repetition_mode, device_list[i]->clock_offset | 0x8000);
            return;
        }
    }
}

void start_scan(void) {
    if (inquiry_state == IDLE) {
        printf("Starting inquiry scan...\n");
        controllers_found = 0;
        device_count = 0;
        inquiry_state = SEARCHING;
        gap_connectable_control(0);
        init_devices();
        gap_inquiry_start(INQUIRY_INTERVAL);
    }
}

void stop_scan() {
    printf("Stopping inquiry scan...\n");
    gap_inquiry_stop();
}

void init_devices() {
    uint8_t i;
    for (i = 0; i < 8; i++) {
        memset(device_list[i]->address, 0, 6);
        device_list[i]->controller_type = CNT_NONE;
    }
}

void pair_timeout_check() {
    if (inquiry_state == IDLE) pair_timeout = app_timer;
    else if (app_timer - pair_timeout >= 15000) {
        printf("Controller pair timeout\n");
        controllers_found = 0;
        end_pair(device_to_pair);
    }
}

enum HID_DEVICE get_discovered_device_type(char * name) {
    if (strcmp(name, "Joy-Con (L)") == 0) return CNT_JOYCON_L;
    else if (strcmp(name, "Joy-Con (R)") == 0) return CNT_JOYCON_R;
    else if (strcmp(name, "Pro Controller") == 0) return CNT_PROCON;
    else if (strcmp(name, "Nintendo RVL-CNT-01") == 0) return CNT_WIIMOTE;
    else if (strcmp(name, "Nintendo RVL-CNT-01-TR") == 0) return CNT_WIIMOTE;
    else if (strcmp(name, "Nintendo RVL-CNT-01-UC") == 0) return CNT_WIIU_PRO;
    else return CNT_NONE;
}

void get_mac_address_string(uint8_t * addr, char * string) {
    uint8_t i;
    for (i = 0; i < 6; i++) {
        string[2 * i] = "0123456789ABCDEF"[(addr[i] & 0xF0) >> 4];
        string[2 * i + 1] = "0123456789ABCDEF"[addr[i] & 0xF];
    }
    string[12] = '\0';
}

void store_controller_type(enum HID_DEVICE type, uint8_t * addr) {
    nvs_flash_init();
    nvs_handle_t handle;
    nvs_open("storage", NVS_READWRITE, &handle);
    char mac_string[13];
    get_mac_address_string(addr, mac_string);
    printf("Storing controller %d type for address %s\n", (uint8_t)type, bd_addr_to_str(addr));
    nvs_set_i8(handle, mac_string, (uint8_t)type);
    nvs_commit(handle);
    nvs_close(handle);
}

enum HID_DEVICE get_controller_type(uint8_t * addr) {
    esp_err_t err;
    nvs_flash_init();
    nvs_handle_t handle;
    nvs_open("storage", NVS_READWRITE, &handle);
    uint8_t val;
    char mac_string[13];
    get_mac_address_string(addr, mac_string);
    err = nvs_get_i8(handle, mac_string, (int8_t *)&val);
    nvs_close(handle);
    printf("Found controller type %d for address %s\n", val, bd_addr_to_str(addr));
    if (err == ESP_OK && val <= 5) return (enum HID_DEVICE)val;
    else return CNT_NONE;
}

void gap_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    uint8_t event;
    bd_addr_t event_addr;
    int index;

    if (packet_type == HCI_EVENT_PACKET) {
		event = hci_event_packet_get_type(packet);
		switch (event) {            
            case GAP_EVENT_INQUIRY_RESULT:
                if (device_count >= 8) break;  // already full
                gap_event_inquiry_result_get_bd_addr(packet, event_addr);
                index = get_device_index_for_address(event_addr);
                if (index >= 0) break;   // already in our list
                
                memcpy(device_list[device_count]->address, event_addr, 6);
                device_list[device_count]->page_scan_repetition_mode = gap_event_inquiry_result_get_page_scan_repetition_mode(packet);
                device_list[device_count]->clock_offset = gap_event_inquiry_result_get_clock_offset(packet);
                device_list[device_count]->controller_type = CNT_NONE;
                printf("Device found: %s ", bd_addr_to_str(event_addr));
                printf("with COD: 0x%06x, ", (unsigned int) gap_event_inquiry_result_get_class_of_device(packet));
                printf("pageScan %d, ", device_list[device_count]->page_scan_repetition_mode);
                printf("clock offset 0x%04x", device_list[device_count]->clock_offset);
                if (gap_event_inquiry_result_get_rssi_available(packet)) {
                    printf(", rssi %d dBm", (int8_t)gap_event_inquiry_result_get_rssi(packet));
                }
                if (gap_event_inquiry_result_get_name_available(packet)) {
                    char name_buffer[50];
                    int name_len = gap_event_inquiry_result_get_name_len(packet);
                    memcpy(name_buffer, gap_event_inquiry_result_get_name(packet), name_len);
                    name_buffer[name_len] = '\0';
                    printf(", name '%s'", name_buffer);
                    device_list[device_count]->controller_type = get_discovered_device_type(name_buffer);
                    if (device_list[device_count]->controller_type != CNT_NONE) controllers_found++;
                    device_list[device_count]->state = REMOTE_NAME_FOUND;
                } else device_list[device_count]->state = REMOTE_NAME_REQUEST;
                printf("\n");
                device_count++;
                break;

            case GAP_EVENT_INQUIRY_COMPLETE:
                if (has_more_remote_name_requests()) {
                    printf("Requesting device names\n");
                    inquiry_state = REQUESTING_NAMES;
                    do_next_remote_name_request();
                }
                else if (controllers_found > 0) {
                    printf("%d controllers found\n", controllers_found);
                    inquiry_state = READY_TO_PAIR;
                } else {
                    printf("No controllers found\n");
                    inquiry_state = IDLE;
                    gap_connectable_control(1);
                }
                break;
            case HCI_EVENT_REMOTE_NAME_REQUEST_COMPLETE:
                reverse_bd_addr(&packet[3], event_addr);
                index = get_device_index_for_address(event_addr);
                if (index >= 0) {
                    if (packet[2] == 0) {
                        printf("Name: '%s'\n", &packet[9]);
                        device_list[index]->controller_type = get_discovered_device_type((char *)&packet[9]);
                        if (device_list[index]->controller_type != CNT_NONE) controllers_found++;
                    } else {
                        printf("Failed to get name: page timeout\n");
                    }
                    device_list[index]->state = REMOTE_NAME_FOUND;
                }
                if (has_more_remote_name_requests()) do_next_remote_name_request();
                else if (controllers_found > 0) {
                    printf("%d controllers found\n", controllers_found);
                    inquiry_state = READY_TO_PAIR;
                } else {
                    printf("No controllers found\n");
                    inquiry_state = IDLE;
                    gap_connectable_control(1);
                }
                break;
            default:
                break;
        }
    }
}