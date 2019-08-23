#include <inttypes.h>
#include <stdio.h>
#include "btstack.h"
#include "connect.h"
#include "hid_controller.h"
#include "gamepad.h"
#include "wiimote.h"
#include "pair.h"
#include "uart_controller.h"

void l2cap_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size) {
    uint8_t event;
	uint8_t event_addr[6] = {0};
    uint16_t l2cap_cid;
	uint16_t psm;
	hid_controller_t * controller = NULL;

	switch (packet_type) {
		case HCI_EVENT_PACKET:
			event = hci_event_packet_get_type(packet);
			switch (event) {
				case HCI_EVENT_CONNECTION_COMPLETE:
					//printf("Time 4 connection\n");
					//hci_event_connection_complete_get_bd_addr(packet, event_addr);
					//l2cap_create_channel(l2cap_event_handler, event_addr, HID_CONTROL_PSM, 64, &general_cid);
					break;
				case HCI_EVENT_PIN_CODE_REQUEST:
					printf("Pin code request\n");
					hci_event_pin_code_request_get_bd_addr(packet, event_addr);
					gap_pin_code_response(event_addr, host_mac_string);
					break;        
				case L2CAP_EVENT_INCOMING_CONNECTION:
					printf("Connection requested\n");
					l2cap_event_incoming_connection_get_address(packet, event_addr);
					psm = l2cap_event_incoming_connection_get_psm(packet); 
					l2cap_cid = l2cap_event_incoming_connection_get_local_cid(packet);
					if (joycon_right.connected && !memcmp(event_addr, joycon_right.address, 6)) l2cap_decline_connection(l2cap_cid);	// Reject wireless connections from wired controllers
					else if (joycon_left.connected && !memcmp(event_addr, joycon_left.address, 6)) l2cap_decline_connection(l2cap_cid);
					controller = get_controller_from_addr(event_addr);
					if (!controller) controller = register_controller(CNT_NONE, event_addr);
					if (!controller) {
						printf("Device rejected - too many other controllers connected or already connected\n");
						l2cap_decline_connection(l2cap_cid);
						return;
					}
					if (controller->type == CNT_NONE) {
						printf("Device rejected - controller not paired\n");
						controller_reset(controller);
						l2cap_decline_connection(l2cap_cid);
						return;
					}
					switch (psm) {
						case HID_CONTROL_PSM:
							if (controller->l2cap_control_cid == 0) {
								controller->l2cap_control_cid = l2cap_cid;
								l2cap_accept_connection(l2cap_cid);
							} else l2cap_decline_connection(l2cap_cid);
							break;
						case HID_INTERRUPT_PSM:
							if (controller->l2cap_interrupt_cid == 0) {
								controller->l2cap_interrupt_cid = l2cap_cid;
								l2cap_accept_connection(l2cap_cid);
							} else l2cap_decline_connection(l2cap_cid);
							break;
						default:
							l2cap_decline_connection(l2cap_cid);
							break;
					}
					break;
				case L2CAP_EVENT_CHANNEL_OPENED: 
					l2cap_cid = l2cap_event_channel_opened_get_local_cid(packet);
					controller = get_controller_from_cid(l2cap_cid);
					if (controller) {
						if (l2cap_cid == controller->l2cap_control_cid) {
							if (controller->pairing) l2cap_create_channel(l2cap_event_handler, controller->address, HID_INTERRUPT_PSM, 64, &controller->l2cap_interrupt_cid);
							printf("HID Control established on channel %d\n", controller->l2cap_control_cid);
						}     					
						else if (l2cap_cid == controller->l2cap_interrupt_cid){
							controller->handle = l2cap_event_channel_opened_get_handle(packet);
							//if(hci_can_send_command_packet_now()) hci_send_cmd(&hci_write_automatic_flush_timeout, connection_handle, 0x0400);
							//else write_flush_timeout_queued = 1;
							controller->connected = 1;
							printf("HID Interrupt established on channel %d\n", controller->l2cap_interrupt_cid);
							controller_setup(controller);
						}
					}
					break;
				case L2CAP_EVENT_CHANNEL_CLOSED: 
					l2cap_cid = l2cap_event_channel_closed_get_local_cid(packet);
					controller = get_controller_from_cid(l2cap_cid);
					if (controller) {
						if (l2cap_cid == controller->l2cap_control_cid) controller->l2cap_control_cid = 0;
						if (l2cap_cid == controller->l2cap_interrupt_cid) controller->l2cap_interrupt_cid = 0;
						if (controller->l2cap_control_cid == 0 && controller->l2cap_interrupt_cid == 0) {
							if (controller->pairing) end_pair(controller);
							gamepad_reset(&gamepads[controller->gamepad_num - 1]);
							gamepad_set_type(controller->gamepad_num);
							gamepad_set_extension(controller->gamepad_num);
							wiimote_reset(&wiimotes[controller->gamepad_num - 1]);
							controller_reset(controller);
							printf("Device disconnected\n");
						}
					}
					break;
				case L2CAP_EVENT_CAN_SEND_NOW:
					hid_packet_handler(packet_type, channel, packet, size);
					break;
				default:
					break;
			}
		case L2CAP_DATA_PACKET:
			hid_packet_handler(packet_type, channel, packet, size);
			break;
		default:
			break;
    }
}