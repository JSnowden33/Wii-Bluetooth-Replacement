#ifndef _CONNECT_H_
#define	_CONNECT_H_

void l2cap_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);

#endif