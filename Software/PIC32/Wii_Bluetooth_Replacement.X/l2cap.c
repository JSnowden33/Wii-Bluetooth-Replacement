#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include "hci.h"
#include "l2cap.h"
#include "sdp.h"
#include "uart.h"
#include "wiimote.h"

uint16_t cid_counter;
uint16_t cmd_id_counter;

static struct l2cap_connection * curr_connections;

void l2cap_init_connections(struct l2cap_connection connections[]) {
	//signaling channel (always active)
	connections[0].active = true;
	connections[0].cid = 0x0001;
	connections[0].recv_data = l2cap_recv_command;  // Add HCI handle argument to these functions to allow referencing certain Wiimotes
	connections[0].get_data = l2cap_get_command;

	//sdp psm
	connections[1].active = false;
	connections[1].psm = 0x0001;
	connections[1].recv_data = sdp_recv_data;
	connections[1].get_data = sdp_get_data;

	//hid ctrl psm
	connections[2].active = false;
	connections[2].psm = 0x0011;
	connections[2].recv_data = _wiimote_recv_ctrl;
	connections[2].get_data = _wiimote_get_ctrl;

	//hid interrupt psm
	connections[3].active = false;
	connections[3].psm = 0x0013;
	connections[3].recv_data = _wiimote_recv_data;
	connections[3].get_data = _wiimote_get_data;
}

void l2cap_init_counters() {
	// Connections only work if initial CID matches Wii's initial CID (0x40)
	// There is probably a bug with how CIDs are handled below
	cid_counter = 0x40;
	cmd_id_counter = 0x01;
}

struct l2cap_connection * l2cap_get_connection_from_cid(struct l2cap_connection connections[], uint16_t cid) {
	uint8_t i = 0;
	for (i = 0; i < NUM_L2CAP_CHANNELS; i++) {
		if (connections[i].cid == cid) return &connections[i];
	}
	return NULL;
}

struct l2cap_connection * l2cap_get_connection_from_psm(struct l2cap_connection connections[], uint16_t psm) {
	uint8_t i = 0;
	for (i = 0; i < NUM_L2CAP_CHANNELS; i++) {
		if (connections[i].psm == psm) return &connections[i];
	}
	return NULL;
}

void l2cap_queue_cmd(uint16_t hci_handle, uint8_t cmd, uint16_t cid, uint16_t id) {
	struct hci_connection * hci_conn = hci_get_connection_from_handle(hci_handle);
	if (hci_conn) {
		hci_conn->l2cap_cmd_queue[(hci_conn->l2cap_cmd_queue_pos + hci_conn->l2cap_cmd_queue_num) % L2CAP_CMD_QUEUE_SIZE] = cmd;
		hci_conn->l2cap_cmd_queue_cid[(hci_conn->l2cap_cmd_queue_pos + hci_conn->l2cap_cmd_queue_num) % L2CAP_CMD_QUEUE_SIZE] = cid;
		hci_conn->l2cap_cmd_queue_id[(hci_conn->l2cap_cmd_queue_pos + hci_conn->l2cap_cmd_queue_num) % L2CAP_CMD_QUEUE_SIZE] = id;
		hci_conn->l2cap_cmd_queue_num++;
	}
}

void l2cap_request_connection(uint16_t hci_handle, uint16_t psm) {
	struct hci_connection * hci_conn = hci_get_connection_from_handle(hci_handle);
	if (hci_conn) {
		struct l2cap_connection * conn = l2cap_get_connection_from_psm(hci_conn->connections, psm);
		if (conn) {
			conn->cid = cid_counter++;
			l2cap_queue_cmd(hci_handle, L2CAP_CONNECTION_REQUEST, conn->cid, 0);
		}
	}
}

void l2cap_recv_command(uint16_t hci_handle, uint8_t * buf, int32_t len) {
	struct l2cap_command * command = (struct l2cap_command *)buf;
	wiimote_t * wiimote = get_wiimote_from_handle(hci_handle);
	int i = 0;

	switch (command->code) {
		case L2CAP_CONNECTION_REQUEST: {
			struct l2cap_connection_request * request = (struct l2cap_connection_request *)command->data;
			struct l2cap_connection * conn = l2cap_get_connection_from_psm(curr_connections, request->psm);
			if (conn) {
				//conn->active = true;
				conn->cid = cid_counter++;
				conn->host_cid = request->source_cid;

				// Data for response
				l2cap_queue_cmd(hci_handle, L2CAP_CONNECTION_RESPONSE, conn->cid, command->identifier);
			}
			break;
		}
		case L2CAP_CONNECTION_RESPONSE: {
			struct l2cap_connection_response * response = (struct l2cap_connection_response *)command->data;

			if (response->result == 0) {
				struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, response->source_cid);
				if (conn) {
					conn->host_cid = response->dest_cid;
					l2cap_queue_cmd(hci_handle, L2CAP_CONFIGURATION_REQUEST, response->source_cid, 0);
				}
			} else if (wiimote) wiimote->sys.l2cap_connection_failed = 1;

			break;
		}
		case L2CAP_CONFIGURATION_REQUEST: {
			struct l2cap_config_request * request = (struct l2cap_config_request *)command->data;

			l2cap_queue_cmd(hci_handle, L2CAP_CONFIGURATION_RESPONSE, request->dest_cid, command->identifier);
			break;
		}
		case L2CAP_CONFIGURATION_RESPONSE: {
			struct l2cap_config_response * response = (struct l2cap_config_response *)command->data;
			struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, response->source_cid);

			if (conn) {
				conn->active = true;
				if (wiimote) {
					switch (conn->psm) {
						case 0x11:
							if (wiimote->sys.l2cap_role == 1) l2cap_request_connection(hci_handle, 0x13);	// Open HID interrupt channel
							break;
						case 0x13:
							wiimote->sys.connected = 1;
							uart_transmit("Wiimote connected", 1);
							break;
						default:
							break;
					}
				}
			}
			break;
		}
		case L2CAP_DISCONNECTION_REQUEST: {
			struct l2cap_disconnection_request * request = (struct l2cap_disconnection_request *)command->data;
			struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, request->dest_cid);

			if (conn) {
				conn->active = false;
				l2cap_queue_cmd(hci_handle, L2CAP_DISCONNECTION_RESPONSE, request->dest_cid, command->identifier);
			}        
			break;
		}
		case L2CAP_DISCONNECTION_RESPONSE:
			break;
		default:
			break;
	}
}

int32_t l2cap_get_command(uint16_t hci_handle, uint8_t * buf) {
	struct hci_connection * hci_conn = hci_get_connection_from_handle(hci_handle);
	int32_t len = 0;

	if (hci_conn) {
		uint8_t cmd = hci_conn->l2cap_cmd_queue[hci_conn->l2cap_cmd_queue_pos];
		uint8_t cid = hci_conn->l2cap_cmd_queue_cid[hci_conn->l2cap_cmd_queue_pos];
		uint8_t cmd_id = hci_conn->l2cap_cmd_queue_id[hci_conn->l2cap_cmd_queue_pos];
		
		if (hci_conn->l2cap_cmd_queue_num) {
			switch (cmd) {
				case L2CAP_CONNECTION_REQUEST: {
					struct l2cap_command * command = (struct l2cap_command *)buf;
					struct l2cap_connection_request * request = (struct l2cap_connection_request *)command->data;
					struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, cid);
					if (conn) {
						request->psm = conn->psm;
						request->source_cid = conn->cid;
					}

					len = sizeof(struct l2cap_connection_request);

					command->code = cmd;
					command->identifier = cmd_id_counter++;
					command->length = len;

					len += sizeof(struct l2cap_command);
					break;
				}
				case L2CAP_CONNECTION_RESPONSE: {
					struct l2cap_command * command = (struct l2cap_command *)buf;
					struct l2cap_connection_response * response = (struct l2cap_connection_response *)command->data;
					struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, cid);
					if (conn) {
						response->dest_cid = conn->cid;
						response->source_cid = conn->host_cid;
						response->result = 0x0000; //connection successful
						response->status = 0x0000; //no further information available

						len = sizeof(struct l2cap_connection_response);

						command->code = cmd;
						command->identifier = cmd_id;
						command->length = len;

						len += sizeof(struct l2cap_command);
						l2cap_queue_cmd(hci_handle, L2CAP_CONFIGURATION_REQUEST, conn->cid, 0);
					}
					break;
				}
				case L2CAP_CONFIGURATION_REQUEST: {
					struct l2cap_command * command = (struct l2cap_command *)buf;
					struct l2cap_config_request * request = (struct l2cap_config_request *)command->data;
					struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, cid);
					if (conn) {
						request->dest_cid = conn->host_cid;
						request->flags = 0x0000; //none
						request->config[0] = 0x01;
						request->config[1] = 0x02;
						request->config[2] = 0xB9;
						request->config[3] = 0x00;

						len = sizeof(struct l2cap_config_request);

						command->code = cmd;
						command->identifier = cmd_id_counter++;
						command->length = len;

						len += sizeof(struct l2cap_command);
					}
					break;
				}
				case L2CAP_CONFIGURATION_RESPONSE: {
					struct l2cap_command * command = (struct l2cap_command *)buf;
					struct l2cap_config_response * response = (struct l2cap_config_response *)command->data;
					struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, cid);
					if (conn) {
						response->source_cid = conn->host_cid;
						response->flags = 0x0000; //none
						response->result = 0x0000; //success
						response->config[0] = 0x01; //copy the provided config
						response->config[1] = 0x02;
						response->config[2] = 0x80;
						response->config[3] = 0x02;
						response->config[4] = 0x02;
						response->config[5] = 0x02;
						response->config[6] = 0xFF;
						response->config[7] = 0xFF;

						len = sizeof(struct l2cap_config_response);

						command->code = cmd;
						command->identifier = cmd_id;
						command->length = len;

						len += sizeof(struct l2cap_command);
					}
					break;
				}
				case L2CAP_DISCONNECTION_RESPONSE: {
					struct l2cap_command * command = (struct l2cap_command *)buf;
					struct l2cap_disconnection_response * response = (struct l2cap_disconnection_response *)command->data;
					struct l2cap_connection * conn = l2cap_get_connection_from_cid(curr_connections, cid);
					if (conn) {
						response->dest_cid = conn->cid;
						response->source_cid = conn->host_cid;

						len = sizeof(struct l2cap_disconnection_response);

						command->code = cmd;
						command->identifier = cmd_id;
						command->length = len;

						len += sizeof(struct l2cap_command);
					}
					break;
				}
			}
			hci_conn->l2cap_cmd_queue_pos = (hci_conn->l2cap_cmd_queue_pos + 1) % L2CAP_CMD_QUEUE_SIZE;
			hci_conn->l2cap_cmd_queue_num--;
		}
	}
	return len;
}

void l2cap_recv_data(uint16_t hci_handle, uint8_t * buf, int32_t len) {
	struct l2cap_header * header = (struct l2cap_header *)buf;
	struct hci_connection * hci_conn = hci_get_connection_from_handle(hci_handle);
	int i = 0;

	if (hci_conn) {
		curr_connections = hci_conn->connections;
		for (i = 0; i < NUM_L2CAP_CHANNELS; i++){
			if (hci_conn->connections[i].active && hci_conn->connections[i].cid == header->channel){
				hci_conn->connections[i].recv_data(hci_handle, header->data, header->length);
			}
		}
	}
}

int32_t l2cap_get_data(uint16_t hci_handle, uint8_t * buf) {   
	struct l2cap_header * header = (struct l2cap_header *)buf;
	struct hci_connection * hci_conn = hci_get_connection_from_handle(hci_handle);
	int32_t len = 0;
	int i = 0;

	// Only one connection processed per loop, lower index has higher priority
	if (hci_conn) {
		curr_connections = hci_conn->connections;
		for (i = 0; i < NUM_L2CAP_CHANNELS; i++) {
			if (hci_conn->connections[i].active) {
				len = hci_conn->connections[i].get_data(hci_handle, header->data);
				if (len > 0) {
					header->length = len;
					header->channel = hci_conn->connections[i].cid;
					len += sizeof(struct l2cap_header);
					break;
				}
			}
		}
	}
	return len;
}