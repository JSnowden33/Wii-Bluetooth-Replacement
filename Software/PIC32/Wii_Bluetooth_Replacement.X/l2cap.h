#ifndef L2CAP_H
#define	L2CAP_H

#include "wiimote.h"

enum L2CAP_SIGNALING_COMMAND_CODES {
	L2CAP_COMMAND_REJECT = 0x01,
	L2CAP_CONNECTION_REQUEST = 0x02,
	L2CAP_CONNECTION_RESPONSE = 0x03,
	L2CAP_CONFIGURATION_REQUEST = 0x04,
	L2CAP_CONFIGURATION_RESPONSE = 0x05,
	L2CAP_DISCONNECTION_REQUEST = 0x06,
	L2CAP_DISCONNECTION_RESPONSE = 0x07,
	L2CAP_ECHO_REQUEST = 0x08,
	L2CAP_ECHO_RESPONSE = 0x09,
	L2CAP_INFORMATION_REQUEST = 0x0A,
	L2CAP_INFORMATION_RESPONSE = 0x0B,
	L2CAP_CREATE_CHANNEL_REQUEST = 0x0C,
	L2CAP_CREATE_CHANNEL_RESPONSE = 0x0D,
	L2CAP_MOVE_CHANNEL_REQUEST = 0x0E,
	L2CAP_MOVE_CHANNEL_RESPONSE = 0x0F,
	L2CAP_MOVE_CHANNEL_CONFIRMATION = 0x10,
	L2CAP_MOVE_CHANNEL_CONFIRMATION_RESPONSE = 0x11,
	L2CAP_CONNECTION_PARAMETER_UPDATE_REQUEST = 0x12,
	L2CAP_CONNECTION_PARAMETER_UPDATE_RESPONSE = 0x13,
	L2CAP_LE_CREDIT_BASED_CONNECTION_REQUEST = 0x14,
	L2CAP_LE_CREDIT_BASED_CONNECTION_RESPONSE = 0x15,
	L2CAP_LE_FLOW_CONTROL_CREDIT = 0x16
};

struct l2cap_header {
  uint16_t length;
  uint16_t channel;
  uint8_t data[];
} __attribute__((packed));

struct l2cap_command {
	uint8_t code;
	uint8_t identifier;
	uint16_t length;
	uint8_t data[];
} __attribute__((packed));

struct l2cap_connection_request {
	uint16_t psm;
	uint16_t source_cid;
} __attribute__((packed));

struct l2cap_connection_response {
	uint16_t dest_cid;
	uint16_t source_cid;
	uint16_t result;
	uint16_t status;
} __attribute__((packed));

struct l2cap_config_request {
	uint16_t dest_cid;
	uint16_t flags;
	uint8_t config[4];
};

struct l2cap_config_response {
	uint16_t source_cid;
	uint16_t flags;
	uint16_t result;
	uint8_t config[8];
};

struct l2cap_disconnection_request {
	uint16_t dest_cid;
	uint16_t source_cid;
} __attribute__((packed));

struct l2cap_disconnection_response {
	uint16_t dest_cid;
	uint16_t source_cid;
} __attribute__((packed));

struct l2cap_connection {
	bool active;
	uint16_t cid;
	uint16_t host_cid;
	uint16_t psm;
	void (*recv_data)(uint16_t hci_handle, uint8_t *, int32_t len);
	int32_t (*get_data)(uint16_t hci_handle, uint8_t *);
};

#define NUM_L2CAP_CHANNELS 4
#define L2CAP_CMD_QUEUE_SIZE 8

void l2cap_init_connections(struct l2cap_connection connections[]);
void l2cap_init_counters();
void l2cap_queue_cmd(uint16_t hci_handle, uint8_t cmd, uint16_t cid, uint16_t id);
void l2cap_request_connection(uint16_t hci_handle, uint16_t psm);

void l2cap_recv_command(uint16_t hci_handle, uint8_t * buf, int32_t len);
int32_t l2cap_get_command(uint16_t hci_handle, uint8_t * buf);

void l2cap_recv_data(uint16_t hci_handle, uint8_t * buf, int32_t len);
int32_t l2cap_get_data(uint16_t hci_handle, uint8_t * buf);

#endif	/* L2CAP_H */
