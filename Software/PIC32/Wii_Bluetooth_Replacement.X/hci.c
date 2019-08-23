#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include "hci.h"
#include "l2cap.h"
#include "uart.h"
#include "delay.h"
#include "wiimote.h"

//#define HCI_DUMP	// Print all HCI events and commands
//#define ACL_DUMP	// Print all ACL transmissions

// Store name given by Wii
char local_name[250] = "BCM2045B2 ROM + EEPROM";

uint8_t connectable = 0;
uint8_t syncing = 0;
uint8_t hci_reset_status = 0;

// MAC addresses for virtual Wiimotes
const uint8_t remote_addr[4][6] = {
	{ 0x78, 0x2C, 0xE5, 0xAA, 0x22, 0x01 },
	{ 0x78, 0x2C, 0xE5, 0xAA, 0x22, 0x02 },
	{ 0x78, 0x2C, 0xE5, 0xAA, 0x22, 0x03 },
	{ 0x78, 0x2C, 0xE5, 0xAA, 0x22, 0x04 }
};

// Bluetooth module address
const uint8_t host_addr[6] = { 0xA0, 0x35, 0xA3, 0xA3, 0xBD, 0x58 };

// Link key for all connections
const uint8_t link_key[16] = { 0x58, 0xB4, 0x81, 0xA1, 0x15, 0x3D, 0xE7, 0xA7, 0x7A, 0xCE, 0x56, 0xD3, 0xEF, 0xE7, 0x0F, 0x0E };

// Store outgoing event
uint8_t evt_buffer[257];
static int32_t evt_length = 0;
static int32_t evt_offset = 0;

// Store incoming command
static uint8_t cmd_buffer[256];
static int32_t cmd_length = 0;

static uint8_t evt_queue[HCI_EVT_QUEUE_SIZE];	// Store pending event codes
static uint16_t evt_queue_opcode[HCI_EVT_QUEUE_SIZE];   // Store opcode for cause of pending events
static uint16_t evt_queue_handle[HCI_EVT_QUEUE_SIZE];   // Store connection handles for pending events
static uint8_t evt_queue_pos = 0;
static uint8_t evt_queue_num = 0;

// Handles for special events that span several commands
uint16_t sync_handle = 0;
uint16_t authentication_handle = 0;
uint16_t connection_request_handle = 0;

uint32_t connection_request_timer = 0;	// Track when Wiimote is ready to initiate l2cap connection
uint8_t connection_request_queued = 0;	// Set on HCI_CONNECTION_COMPLETE event (if PIC initiated the connection)

struct hci_connection hci_connections[MAX_HCI_CONNECTIONS];
static int cur_connection = 0;

void hci_reset() {
	int i = 0;
	for (i = 0; i < MAX_HCI_CONNECTIONS; i++) {
		hci_connections[i].active = true;
		hci_connections[i].handle = 0x000B + i;
		hci_connections[i].l2cap_recv_len = 0;
		hci_connections[i].l2cap_send_len = 0;
		hci_connections[i].l2cap_send_offset = 0;
		hci_connections[i].l2cap_cmd_queue_pos = 0;
		hci_connections[i].l2cap_cmd_queue_num = 0;

		if (i < 4) {
			hci_connections[i].addr = remote_addr[i];
			init_wiimote(&wiimotes[i], hci_connections[i].handle);
		}
		l2cap_init_connections(hci_connections[i].connections);
	}
	l2cap_init_counters();
	connectable = 0;
	//evt_queue_num = 0;
	hci_reset_status = 1;
	uart_transmit("HCI Reset", 1);
}

uint8_t hci_get_connectable_status() {
	return connectable;
}

struct hci_connection * hci_get_connection_from_handle(uint16_t handle) {
	uint8_t i = 0;
	for (i = 0; i < MAX_HCI_CONNECTIONS; i++) {
		if (hci_connections[i].handle == handle) return &hci_connections[i];
	}
	return NULL;
}

uint16_t hci_get_handle_from_address(uint8_t * addr) {
	uint8_t i = 0;
	for (i = 0; i < MAX_HCI_CONNECTIONS; i++) {
		if (!memcmp(hci_connections[i].addr, addr, 6)) return hci_connections[i].handle;
	}
	return 0;
}

void hci_queue_evt(uint8_t evt, uint16_t opcode, uint16_t handle) {
	evt_queue[(evt_queue_pos + evt_queue_num) % HCI_EVT_QUEUE_SIZE] = evt;
	evt_queue_opcode[(evt_queue_pos + evt_queue_num) % HCI_EVT_QUEUE_SIZE] = opcode;
	evt_queue_handle[(evt_queue_pos + evt_queue_num) % HCI_EVT_QUEUE_SIZE] = handle;
	evt_queue_num++;
}

void hci_send_evt(int32_t len) {
	evt_length = len;
	evt_offset = 0;
}

uint8_t hci_send_pending_evt() {
	if (evt_queue_num) {
		struct hci_evt * result = (struct hci_evt *)evt_buffer;
		uint16_t opcode = evt_queue_opcode[evt_queue_pos];
		uint16_t handle = evt_queue_handle[evt_queue_pos];
		uint16_t ogf = opcode >> 10;
		uint16_t ocf = opcode & 0x3FF;
		struct hci_connection * conn = hci_get_connection_from_handle(handle);
		wiimote_t * wiimote = get_wiimote_from_handle(handle);
		
		result->code = evt_queue[evt_queue_pos];

		switch(evt_queue[evt_queue_pos]) {
			case HCI_COMMAND_COMPLETE: {
				struct hci_evt_cmd_complete * cmd_complete = (struct hci_evt_cmd_complete *)(result->data);

				switch (ogf) {
					case OGF_LINK_CONTROL:
						switch (ocf) {
							case HCI_LINK_KEY_REQUEST_REPLY: 
							case HCI_LINK_KEY_REQUEST_NEGATIVE_REPLY:
							case HCI_PIN_CODE_REQUEST_REPLY: {
								result->param_len = 10;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status
								if (conn) memcpy(cmd_complete->data + 1, conn->addr, 6);

								hci_send_evt(12);
								break;
							}
							case HCI_INQUIRY_CANCEL:
								result->param_len = 4;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								hci_send_evt(6);
								break;
						}
						break;

					case OGF_LINK_POLICY:
						switch (ocf) {
							case HCI_WRITE_LINK_POLICY_SETTINGS:
								result->param_len = 6;

								cmd_complete->data[0] = 0x00; //status

								memcpy(cmd_complete->data + 1, &handle, 2);

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								hci_send_evt(8);
								break;

							case HCI_WRITE_DEFAULT_LINK_POLICY_SETTINGS:
								result->param_len = 4;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								hci_send_evt(6);
								break;
						}
						break;

					case OGF_CONTROLLER_BASEBAND:
						switch (ocf) {
							case HCI_RESET:
								result->param_len = 4;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								hci_send_evt(6);
								break;
							case HCI_READ_STORED_LINK_KEY:
								result->param_len = 8;

								result->data[0] = 1;
								result->data[1] = 0x0D;
								result->data[2] = 0x0C;

								result->data[3] = 0x00; //status

								//max num keys
								result->data[4] = 0x10;
								result->data[5] = 0x00;

								//number of link keys read
								result->data[6] = 0x04;
								result->data[7] = 0x00;

								hci_send_evt(10);
								break;
							case HCI_WRITE_STORED_LINK_KEY:
								result->param_len = 5;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								//status
								cmd_complete->data[0] = 0x00;

								//number of keys written
								cmd_complete->data[1] = 0x01;

								hci_send_evt(7);
								break;
							case HCI_DELETE_STORED_LINK_KEY:
								result->param_len = 6;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//number of link keys deleted
								result->data[4] = 0x00;
								result->data[5] = 0x00;

								hci_send_evt(8);
								break;
							case HCI_READ_LOCAL_NAME:
								result->param_len = 252;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								strcpy(result->data + 4, local_name);

								hci_send_evt(254);
								break;
							case HCI_WRITE_SCAN_ENABLE:
								result->param_len = 4;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								hci_send_evt(6);
								break;
							case HCI_READ_PAGE_SCAN_ACTIVITY:
								result->param_len = 8;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//device class
								result->data[4] = 0x00;
								result->data[5] = 0x01;
								result->data[6] = 0x2c;
								result->data[7] = 0x00;

								hci_send_evt(10);
								break;
							case HCI_READ_CLASS_OF_DEVICE:
								result->param_len = 7;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//device class
								result->data[4] = 0x00;
								result->data[5] = 0x00;
								result->data[6] = 0x00;

								hci_send_evt(9);
								break;
							case HCI_READ_VOICE_SETTING:
								result->param_len = 6;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//voice setting (initial value copied from BCM2045A)
								result->data[4] = 0x60;
								result->data[5] = 0x00;

								hci_send_evt(8);
								break;
							case HCI_WRITE_LINK_SUPERVISION_TIMEOUT:
								result->param_len = 6;

								cmd_complete->data[0] = 0x00; //status

								memcpy(cmd_complete->data + 1, &handle, 2);

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								hci_send_evt(8);
								break;
							case HCI_READ_NUMBER_OF_SUPPORTED_IAC:
								result->param_len = 5;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//inquiry access codes supported (copied from BCM2045A)
								result->data[4] = 0x01;

								hci_send_evt(7);
								break;
							case HCI_READ_CURRENT_IAC_LAP:
								result->param_len = 8;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//num current IAC, default GIAC
								result->data[4] = 0x01;
								result->data[5] = 0x33;
								result->data[6] = 0x8b;
								result->data[7] = 0x9e;

								hci_send_evt(10);
								break;
							case HCI_READ_PAGE_SCAN_TYPE:
								result->param_len = 5;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//interlaced scan (default on BCM2045A)
								result->data[4] = 0x01;

								hci_send_evt(7);
								break;

							// Unimplemented commands
							case HCI_SET_EVENT_MASK:
							case HCI_SET_EVENT_FILTER:
							case HCI_WRITE_PIN_TYPE:
							case HCI_WRITE_LOCAL_NAME:
							case HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT:
							case HCI_WRITE_PAGE_TIMEOUT:
							case HCI_WRITE_PAGE_SCAN_ACTIVITY:
							case HCI_WRITE_CLASS_OF_DEVICE:
							case HCI_HOST_BUFFER_SIZE:
							case HCI_WRITE_INQUIRY_MODE:
							case HCI_WRITE_INQUIRY_SCAN_TYPE:
							case HCI_WRITE_PAGE_SCAN_TYPE:
								result->param_len = 4;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								hci_send_evt(6);
								break;
						}
						break;

					case OGF_INFORMATIONAL_PARAMETERS:
						switch (ocf) {
							case HCI_READ_LOCAL_VERSION_INFORMATION:
								result->param_len = 12;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//version info (copied from BCM2045A)
								result->data[4] = 0x03;
								result->data[5] = 0xa7;
								result->data[6] = 0x40;
								result->data[7] = 0x03;
								result->data[8] = 0x0f;
								result->data[9] = 0x00;
								result->data[10] = 0x0e;
								result->data[11] = 0x43;

								hci_send_evt(14);
								break;
							case HCI_READ_LOCAL_SUPPORTED_COMMANDS:
								result->param_len = 68;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//commands list (copied from BCM2045A)
								result->data[4] = 0xff;
								result->data[5] = 0xff;
								result->data[6] = 0xff;
								result->data[7] = 0x03;
								result->data[8] = 0xfe;
								result->data[9] = 0xff;
								result->data[10] = 0xcf;
								result->data[11] = 0xff;
								result->data[12] = 0xff;
								result->data[13] = 0xff;
								result->data[14] = 0xff;
								result->data[15] = 0x1f;
								result->data[16] = 0xf2;
								result->data[17] = 0x0f;
								result->data[18] = 0xf8;
								result->data[19] = 0xff;
								result->data[20] = 0x3f;
								result->data[21] = 0;
								result->data[22] = 0;
								result->data[23] = 0;
								result->data[24] = 0;
								result->data[25] = 0;

								hci_send_evt(70);
								break;
							case HCI_READ_LOCAL_SUPPORTED_FEATURES:
								result->param_len = 12;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//features list (copied from BCM2045A)
								result->data[4] = 0xff;
								result->data[5] = 0xff;
								result->data[6] = 0x8d;
								result->data[7] = 0xfe;
								result->data[8] = 0x9b;
								result->data[9] = 0xf9;
								result->data[10] = 0x00;
								result->data[11] = 0x80;

								hci_send_evt(14);
								break;
							case HCI_READ_LOCAL_EXTENDED_FEATURES:
								result->param_len = 14;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//extended features list (copied from BCM2045A)
								result->data[4] = 0x01;
								result->data[5] = 0x00;
								result->data[6] = 0x00;
								result->data[7] = 0x00;
								result->data[8] = 0x00;
								result->data[9] = 0x00;
								result->data[10] = 0x00;
								result->data[11] = 0x00;
								result->data[12] = 0x00;
								result->data[13] = 0x00;

								hci_send_evt(16);
								break;
							case HCI_READ_BUFFER_SIZE: 
								result->param_len = 11;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								//buffer sizes (copied from BCM2045A)
								result->data[4] = 0x53;
								result->data[5] = 0x01;
								result->data[6] = 0x40;
								result->data[7] = 0x0a;
								result->data[8] = 0x00;
								result->data[9] = 0x00;
								result->data[10] = 0x00;

								hci_send_evt(13);
								break;
							case HCI_READ_BD_ADDR: 
								result->param_len = 10;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								memcpy(result->data + 4, host_addr, 6);

								hci_send_evt(12);
								break;
						}
						break;

					case OGF_VENDOR_SPECIFIC:
						switch (ocf) {
							case 0x004C:
							case 0x004F:
								result->param_len = 4;

								cmd_complete->allowed_pkts = 1;
								cmd_complete->opcode = opcode;

								cmd_complete->data[0] = 0x00; //status

								hci_send_evt(6);
								break;
						}
						break;

					default:
						break;
				}
				break;
			}
			case HCI_COMMAND_STATUS: {
				struct hci_evt_cmd_status * cmd_status = (struct hci_evt_cmd_status *)(result->data);

				switch (ogf) {
					case OGF_LINK_CONTROL:
						switch (ocf) {
							case HCI_REMOTE_NAME_REQUEST:
							case HCI_INQUIRY:
							case HCI_CREATE_CONNECTION:
							case HCI_DISCONNECT:
							case HCI_ACCEPT_CONNECTION_REQUEST:
							case HCI_AUTHENTICATION_REQUESTED:
							case HCI_READ_REMOTE_SUPPORTED_FEATURES:
							case HCI_READ_REMOTE_VERSION_INFORMATION:
							case HCI_READ_CLOCK_OFFSET:
								result->param_len = 4;

								cmd_status->status = 0x0;
								cmd_status->allowed_pkts = 1;
								cmd_status->opcode = opcode;

								hci_send_evt(6);
								break;
							case HCI_CHANGE_CONNECTION_PACKET_TYPE:
								// Wii sends invalid parameters - return an error
								result->param_len = 4;

								cmd_status->status = 0x12;
								cmd_status->allowed_pkts = 1;
								cmd_status->opcode = opcode;

								hci_send_evt(6);
								break;
						}
						break;
					case OGF_LINK_POLICY:
						switch (ocf) {
							case HCI_SNIFF_MODE:
								result->param_len = 4;

								cmd_status->status = 0x0;
								cmd_status->allowed_pkts = 1;
								cmd_status->opcode = opcode;

								hci_send_evt(6);
								break;
						}
						break;
					default:
						break;
				}
				break;
			}
			case HCI_CONNECTION_PACKET_TYPE_CHANGED:
				result->param_len = 5;
				
				result->data[0] = 0x00;	// Status
				memcpy(result->data + 1, &handle, 2);
				result->data[3] = 0x00;
				result->data[4] = 0x00;
				
				hci_send_evt(7);
				break;
			case HCI_MODE_CHANGE:
				result->param_len = 6;

				//status
				result->data[0] = 0x00;

				memcpy(result->data + 1, &handle, 2);

				result->data[3] = 0x02;
				result->data[4] = 0x08;
				result->data[5] = 0x00;

				hci_send_evt(8);
				break;
			case HCI_ROLE_CHANGE:
				result->param_len = 8;

				//status
				result->data[0] = 0x00;

				if (conn) memcpy(result->data + 1, conn->addr, 6);

				// Current role (master))
				result->data[7] = 0x00;

				hci_send_evt(10);
				break;
			case HCI_CONNECTION_COMPLETE: {
				result->param_len = 11;

				//status
				result->data[0] = 0x00;

				memcpy(result->data + 1, &handle, 2);
				if (conn) memcpy(result->data + 3, conn->addr, 6);

				//link type (ACL)
				result->data[9] = 0x01;

				//encryption enabled (no)
				result->data[10] = 0x0;

				hci_send_evt(13);
				
				if (wiimote) {
					if (wiimote->sys.l2cap_role == 1) {
						connection_request_timer = main_timer;
						connection_request_queued = 1;
					}
				}
				break;
			}
			case HCI_DISCONNECTION_COMPLETE:
				result->param_len = 4;

				// Status
				result->data[0] = 0x00;

				// Connection handle
				memcpy(result->data + 1, &handle, 2);

				// Reason for disconnect (copied from Wiimote being rejected due to not being synced)
				result->data[3] = 0x16;

				hci_send_evt(6);
				
				if (wiimote) wiimote->sys.hci_connection_failed = 1;
				break;
			case HCI_PIN_CODE_REQUEST:
				result->param_len = 6;

				if (conn) memcpy(result->data, conn->addr, 6);

				hci_send_evt(8);
				break;
			case HCI_RETURN_LINK_KEYS:
				result->param_len = 89;

				result->data[0] = 0x04; // Number of link keys returned

				memcpy(result->data + 1, remote_addr[0], 6);
				memcpy(result->data + 7, link_key, 16);
				memcpy(result->data + 23, remote_addr[1], 6);
				memcpy(result->data + 29, link_key, 16);
				memcpy(result->data + 45, remote_addr[2], 6);
				memcpy(result->data + 51, link_key, 16);
				memcpy(result->data + 67, remote_addr[3], 6);
				memcpy(result->data + 73, link_key, 16);

				hci_send_evt(91);
				break;
			case HCI_LINK_KEY_NOTIFICATION:
				result->param_len = 23;

				if (conn) memcpy(result->data, conn->addr, 6);
				memcpy(result->data + 6, link_key, 16);

				hci_send_evt(25);
				break;
			case HCI_AUTHENTICATION_COMPLETE:
				result->param_len = 3;

				//status
				result->data[0] = 0x00;

				memcpy(result->data + 1, &handle, 2);

				hci_send_evt(5);
				break;
			case HCI_LINK_KEY_REQUEST:
				result->param_len = 6;

				if (conn) memcpy(result->data, conn->addr, 6);

				hci_send_evt(8);
				break;
			case HCI_REMOTE_NAME_REQUEST_COMPLETE:
				result->param_len = 255;

				//status
				result->data[0] = 0x00;

				if (conn) memcpy(result->data + 1, conn->addr, 6);

				strcpy(result->data + 7, "Nintendo RVL-CNT-01");

				hci_send_evt(257);
				break;
			case HCI_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE:
				result->param_len = 11;

				//status
				result->data[0] = 0x00;

				memcpy(result->data + 1, &handle, 2);

				//features
				result->data[3] = 0xBC;
				result->data[4] = 0x02;
				result->data[5] = 0x04;
				result->data[6] = 0x38;
				result->data[7] = 0x08;
				result->data[8] = 0x00;
				result->data[9] = 0x00;
				result->data[10] = 0x00;

				hci_send_evt(13);
				break;
			case HCI_READ_REMOTE_VERSION_INFORMATION_COMPLETE:
				result->param_len = 8;

				//status
				result->data[0] = 0x00;

				memcpy(result->data + 1, &handle, 2);

				//version
				result->data[3] = 0x03;

				//manufacturer name
				result->data[4] = 0x0F;
				result->data[5] = 0x00;

				//subversion
				result->data[6] = 0x1C;
				result->data[7] = 0x03;

				hci_send_evt(10);
				break;
			case HCI_READ_CLOCK_OFFSET_COMPLETE:
				result->param_len = 5;

				//status
				result->data[0] = 0x00;

				memcpy(result->data + 1, &handle, 2);

				//clock offset
				result->data[3] = 0xE9;
				result->data[4] = 0x43;

				hci_send_evt(7);
				break;
			case HCI_INQUIRY_COMPLETE:
				result->param_len = 1;
				result->data[0] = 0x00;   // Inquiry success

				hci_send_evt(3);

				syncing = 0;   // New inquiry can now begin
				break;
			case HCI_INQUIRY_RESULT_WITH_RSSI:
				if (syncing) {
					result->param_len = 15;

					//num responses
					result->data[0] = 0x01;

					if (conn) memcpy(result->data + 1, conn->addr, 6);

					//page scan repetition mode
					result->data[7] = 0x01;

					//reserved
					result->data[8] = 0x00;

					//remote class
					result->data[9] = 0x04;
					result->data[10] = 0x25;
					result->data[11] = 0x00;

					//clock offset
					result->data[12] = 0xEA;
					result->data[13] = 0x43;

					//rssi
					result->data[14] = 0xBF;

					hci_send_evt(17);
				} else {
					result->param_len = 1;
					result->data[0] = 0;
					hci_send_evt(3);
				}
				break;
			case HCI_CONNECTION_REQUEST:
				result->param_len = 10;
				if (conn) memcpy(result->data, conn->addr, 6);
				result->data[6] = 0x04;
				result->data[7] = 0x25;
				result->data[8] = 0x00;
				result->data[9] = 0x01;
				hci_send_evt(12);

				connection_request_handle = handle;
				break;
			case 0xFF: //Sync button press
				if (!syncing) {
					result->param_len = 1;
					result->data[0] = 0x08;

					syncing = 1;
					sync_handle = handle;

					hci_send_evt(3);
				}
				break;
			default:
				break;
		}
		evt_queue_pos = (evt_queue_pos + 1) % HCI_EVT_QUEUE_SIZE;
		evt_queue_num--;
		return 1;
	}
	return 0;
}

void hci_process_cmd() {
	struct hci_cmd * header = (struct hci_cmd *)cmd_buffer;
	uint16_t opcode = header->opcode;
	uint16_t handle = 0;
	uint16_t ogf = opcode >> 10;
	uint16_t ocf = opcode & 0x3FF;

	switch (ogf) {
		case OGF_LINK_CONTROL:
			switch (ocf) {
				case HCI_INQUIRY:
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_INQUIRY_RESULT_WITH_RSSI, opcode, sync_handle);
					hci_queue_evt(HCI_INQUIRY_COMPLETE, opcode, 0);
					break;
				case HCI_INQUIRY_CANCEL:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, 0);
					break;
				case HCI_CREATE_CONNECTION:
					handle = hci_get_handle_from_address(header->data);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_CONNECTION_COMPLETE, opcode, handle);
					break;
				case HCI_DISCONNECT:
					handle = header->data[0] | (header->data[1] << 8);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_DISCONNECTION_COMPLETE, opcode, handle);
					break;
				case HCI_ACCEPT_CONNECTION_REQUEST:
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					if (header->data[6] == 0) hci_queue_evt(HCI_ROLE_CHANGE, opcode, connection_request_handle);
					hci_queue_evt(HCI_CONNECTION_COMPLETE, opcode, connection_request_handle);
					break;
				case HCI_LINK_KEY_REQUEST_REPLY:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, authentication_handle);
					hci_queue_evt(HCI_AUTHENTICATION_COMPLETE, opcode, authentication_handle);
					break;
				case HCI_LINK_KEY_REQUEST_NEGATIVE_REPLY:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, authentication_handle);
					hci_queue_evt(HCI_PIN_CODE_REQUEST, opcode, authentication_handle);
					break;
				case HCI_PIN_CODE_REQUEST_REPLY:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, authentication_handle);
					hci_queue_evt(HCI_LINK_KEY_NOTIFICATION, opcode, authentication_handle);
					hci_queue_evt(HCI_AUTHENTICATION_COMPLETE, opcode, authentication_handle);
					break;
				case HCI_CHANGE_CONNECTION_PACKET_TYPE:
					handle = header->data[0] | (header->data[1] << 8);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					//hci_queue_evt(HCI_CONNECTION_PACKET_TYPE_CHANGED, opcode, handle);
					break;
				case HCI_AUTHENTICATION_REQUESTED:
					authentication_handle = header->data[0] | (header->data[1] << 8);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_LINK_KEY_REQUEST, opcode, authentication_handle);
					break;
				case HCI_REMOTE_NAME_REQUEST:
					handle = hci_get_handle_from_address(header->data);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_REMOTE_NAME_REQUEST_COMPLETE, opcode, handle);
					break;
				case HCI_READ_REMOTE_SUPPORTED_FEATURES:
					handle = header->data[0] | (header->data[1] << 8);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_READ_REMOTE_SUPPORTED_FEATURES_COMPLETE, opcode, handle);
					break;
				case HCI_READ_REMOTE_VERSION_INFORMATION:
					handle = header->data[0] | (header->data[1] << 8);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_READ_REMOTE_VERSION_INFORMATION_COMPLETE, opcode, handle);
					break;
				case HCI_READ_CLOCK_OFFSET:
					handle = header->data[0] | (header->data[1] << 8);
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, 0);
					hci_queue_evt(HCI_READ_CLOCK_OFFSET_COMPLETE, opcode, handle);
					break;
			}
			break;

		case OGF_LINK_POLICY:
			switch (ocf) {
				case HCI_SNIFF_MODE:
					hci_queue_evt(HCI_COMMAND_STATUS, opcode, handle);
					hci_queue_evt(HCI_MODE_CHANGE, opcode, handle);
					break;
				case HCI_WRITE_LINK_POLICY_SETTINGS:
				case HCI_WRITE_DEFAULT_LINK_POLICY_SETTINGS:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, handle);
					break;
			}
			break;

		case OGF_CONTROLLER_BASEBAND:
			switch (ocf) {
				case HCI_RESET:
					hci_reset();
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, handle);
					break;
				case HCI_WRITE_SCAN_ENABLE:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, handle);
					if ((header->data[0] == 0x02) && !strcmp(local_name, "Wii")) connectable = 1;	// Wii is ready to accept connections
					break;
				case HCI_READ_STORED_LINK_KEY:
					hci_queue_evt(HCI_RETURN_LINK_KEYS, opcode, handle);
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, handle);
					break;
				case HCI_WRITE_LOCAL_NAME:
					memcpy(local_name, header->data, 250);
				case HCI_WRITE_STORED_LINK_KEY:
				case HCI_DELETE_STORED_LINK_KEY:
				case HCI_READ_LOCAL_NAME:
				case HCI_READ_PAGE_SCAN_ACTIVITY:
				case HCI_READ_CLASS_OF_DEVICE:
				case HCI_READ_VOICE_SETTING:
				case HCI_WRITE_LINK_SUPERVISION_TIMEOUT:
				case HCI_READ_NUMBER_OF_SUPPORTED_IAC:
				case HCI_READ_CURRENT_IAC_LAP:
				case HCI_READ_PAGE_SCAN_TYPE:
				case HCI_SET_EVENT_MASK:
				case HCI_SET_EVENT_FILTER:
				case HCI_WRITE_PIN_TYPE:
				case HCI_WRITE_CONNECTION_ACCEPT_TIMEOUT:
				case HCI_WRITE_PAGE_TIMEOUT:
				case HCI_WRITE_PAGE_SCAN_ACTIVITY:
				case HCI_WRITE_CLASS_OF_DEVICE:
				case HCI_HOST_BUFFER_SIZE:
				case HCI_WRITE_INQUIRY_MODE:
				case HCI_WRITE_INQUIRY_SCAN_TYPE:
				case HCI_WRITE_PAGE_SCAN_TYPE:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, handle);
					break;
			}
			break;

		case OGF_INFORMATIONAL_PARAMETERS:
			switch (ocf) {
				case HCI_READ_LOCAL_VERSION_INFORMATION:
				case HCI_READ_LOCAL_SUPPORTED_COMMANDS:
				case HCI_READ_LOCAL_SUPPORTED_FEATURES:
				case HCI_READ_LOCAL_EXTENDED_FEATURES:
				case HCI_READ_BUFFER_SIZE:
				case HCI_READ_BD_ADDR:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, handle);
					break;
			}
			break;

		case OGF_VENDOR_SPECIFIC:
			switch (ocf) {
				case 0x004C:
				case 0x004F:
					hci_queue_evt(HCI_COMMAND_COMPLETE, opcode, handle);
					break;
			}
			break;

		default:
			break;
	}
#ifdef HCI_DUMP
	uart_transmit("CMD <= ", 0);
	uart_hexdump(cmd_buffer, cmd_length);
#endif
	
	header = (struct hci_cmd *)cmd_buffer;
	uint16_t new_opcode = header->opcode;
	if (new_opcode != opcode) hci_process_cmd();	// New command was sent while this function was running - process again
}

/*
 * Commands are received by passing cmd_buffer to the USB functions when
 * requested. Once the buffer is filled with valid data, its length will be
 * passed to hci_recv_command.
 */
uint8_t * hci_get_cmd_buffer() {
	return cmd_buffer;
}

void hci_recv_command(int32_t len) {
	cmd_length = len;
}

int32_t hci_get_event(uint8_t * buf) {
	int32_t len = 0;

	if (evt_length - evt_offset > 0) {
		//there is buffered data to send
		len = evt_length - evt_offset;
		if (len > 16) len = 16;

		memcpy(buf, evt_buffer + evt_offset, len);

		evt_offset += len;
#ifdef HCI_DUMP
		uart_transmit("EVT => ", 0);
		uart_hexdump(buf, len);
#endif
	} else if (hci_send_pending_evt()) {
		// Nothing to do here
	} else if (connection_request_queued && (main_timer - connection_request_timer >= 100)) {
		l2cap_request_connection(connection_request_handle, 0x11);	// Open HID control channel
		connection_request_queued = 0;
	} else {
		uint16_t packets_flushed = 0;
		uint8_t i;
		for (i = 0; i < MAX_HCI_CONNECTIONS; i++) packets_flushed += hci_connections[i].data_packets_flushed;

		if (packets_flushed >= 2) {
			struct hci_evt * result = (struct hci_evt *)buf;
			result->code = 0x13;
			result->data[0] = 0;    // Number of connections included

			for (i = 0; i < MAX_HCI_CONNECTIONS; i++) {
				if (hci_connections[i].data_packets_flushed) {
					memcpy(result->data + 1 + (4 * result->data[0]), &hci_connections[i].handle, 2);
					result->data[3 + (4 * result->data[0])] = hci_connections[i].data_packets_flushed & 0xFF;
					result->data[4 + (4 * result->data[0])] = 0;
					result->data[0]++;
					hci_connections[i].data_packets_flushed = 0;
				}
			}

			result->param_len = 4 * result->data[0] + 1;

			len = result->param_len + 2;
#ifdef HCI_DUMP
			uart_transmit("EVT => ", 0);
			uart_hexdump(buf, len);
#endif
		}
	} 

	if (cmd_length > 0) {
		hci_process_cmd();
		cmd_length = 0;
	}
	return len;
}

/*
 * HCI data packet received on the USB bulk endpoint
 * This is called from an interrupt handler and is not meant to do much other
 * than copy the received data to the appropriate place for processing.
 */
void hci_recv_data(uint8_t * buf, int32_t len) {
	struct hci_acl * header = (struct hci_acl *)buf;
	int i = 0;

	//search for the connection to pass incoming data to
	for (i = 0; i < MAX_HCI_CONNECTIONS; i++) {
		if (hci_connections[i].handle == header->handle) {
			//note: it is possible for l2cap data to be sent across multiple
			//HCI transfers, but in this case it is not known to happen

			memcpy(hci_connections[i].l2cap_recv, header->data, header->data_len);
			hci_connections[i].l2cap_recv_len = header->data_len;
#ifdef ACL_DUMP
			uart_transmit("ACL <= ", 0);
			uart_hexdump(header, hci_connections[i].l2cap_recv_len + 4);
#endif
			if (hci_connections[i].l2cap_recv_len > 0) {
				// Deliver received data if there is any
				l2cap_recv_data(hci_connections[i].handle, hci_connections[i].l2cap_recv, hci_connections[i].l2cap_recv_len);
				hci_connections[i].l2cap_recv_len = 0;
				hci_connections[i].data_packets_flushed++;
			}
			break;
		}
	}
}

int32_t hci_get_data(uint8_t * buf) {
	int32_t len = 0;
	struct hci_connection * conn = &hci_connections[cur_connection];

	// Check which connections are active and process only those
	if (conn->active) {
		if (conn->l2cap_send_len - conn->l2cap_send_offset <= 0) {
			// Provide opportunity to generate data if there is none buffered
			conn->l2cap_send_len = l2cap_get_data(conn->handle, conn->l2cap_send);

			if (conn->l2cap_send_len > 0)conn->l2cap_send_offset = 0;
		}

		if (conn->l2cap_send_len - conn->l2cap_send_offset > 0) {
			// There is buffered data to send
			struct hci_acl * header = (struct hci_acl *)buf;

			header->broadcast = 0;
			header->packet_boundary = (conn->l2cap_send_offset > 0) ? 1 : 2;
			header->handle = conn->handle;

			len = conn->l2cap_send_len - conn->l2cap_send_offset;
			if (len > 27) len = 27;
			header->data_len = len;

			memcpy(header->data, conn->l2cap_send + conn->l2cap_send_offset, len);

			conn->l2cap_send_offset += len;
			len += sizeof(struct hci_acl);
#ifdef ACL_DUMP
			uart_transmit("ACL => ", 0);
			uart_hexdump(buf, len);
#endif
		}
	}
	cur_connection = (cur_connection + 1) % MAX_HCI_CONNECTIONS;
	return len;
}