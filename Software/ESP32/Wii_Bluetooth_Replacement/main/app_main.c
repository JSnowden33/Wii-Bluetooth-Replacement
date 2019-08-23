#define BTSTACK_FILE__ "app_main.c"

#include <inttypes.h>
#include <stdio.h>
#include "btstack_config.h"
#include "btstack.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "rom/ets_sys.h"
#include "esp_system.h"
#include "driver/timer.h"
#include "connect.h"
#include "gamepad.h"
#include "hid_controller.h"
#include "pair.h"
#include "spi.h"
#include "uart_controller.h"
#include "timer.h"
#include "wiimote.h"

#define PAIR_PIN GPIO_NUM_14

static btstack_timer_source_t app_loop;
uint64_t app_timer;	// App time running in ms
uint32_t prev_time = 0;
uint8_t spi_en = 0;

static btstack_packet_callback_registration_t hci_event_callback_registration;
static btstack_packet_callback_registration_t l2cap_event_callback_registration;
static btstack_packet_callback_registration_t gap_event_callback_registration;
 
static void hci_event_handler (uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size);
static void hci_event_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet, uint16_t size)
{
    uint8_t event;
	uint8_t i;

    if (packet_type == HCI_EVENT_PACKET) {
		event = hci_event_packet_get_type(packet);
		switch (event) {            
			case BTSTACK_EVENT_STATE:
				if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
					gap_local_bd_addr(host_mac_addr);	// Get host MAC address
					for (i = 0; i < 6; i++) host_mac_addr_rev[i] = host_mac_addr[5 - i];
					memcpy(host_mac_string, host_mac_addr_rev, 6);
					host_mac_string[6] = 0;
					l2cap_register_service(l2cap_event_handler, HID_CONTROL_PSM, 64, LEVEL_2);
					l2cap_register_service(l2cap_event_handler, HID_INTERRUPT_PSM, 64, LEVEL_2);
				}
				break;
			default:
				break;
		}
    }
}

void app_loop_handler(btstack_timer_source_t *ts) {
    UNUSED(ts);
	timer_get_counter_value(TIMER_GROUP_1, TIMER_1, &app_timer);
	app_timer = app_timer / 1000;	// Convert us to ms

	if (!gpio_get_level(PAIR_PIN) && app_timer >= 1000) start_scan();
	pair_timeout_check();

	// Check device inquiry results
	if (ready_to_pair()) {
		enum HID_DEVICE controller_type;
		uint8_t * controller_addr;
		controller_addr = get_next_discovered_controller_addr(&controller_type);
		hid_controller_t * new_controller = register_controller(controller_type, controller_addr);
		if (new_controller) begin_pair(new_controller);
		else {
			printf("Unable to pair - too many other controllers connected or already connected\n");
			end_pair(NULL);
		}
	}

	uart_joycon_handle(&joycon_right);
	uart_joycon_handle(&joycon_left);

	controller_handle(1);
	controller_handle(2);
	controller_handle(3);
	controller_handle(4);
	controller_handle(5);
	controller_handle(6);
	controller_handle(7);
	controller_handle(8);

	gamepad_handle(1);
	gamepad_handle(2);
	gamepad_handle(3);
	gamepad_handle(4);

	if ((app_timer >= 1500) && !spi_en) {
		spi_en = 1;
		gpio_set_level(SPI_EN, 1);	// Accept SPI transfers
	}
	
	if (app_timer - prev_time >= 15) {
		wiimote_spi_receive();
		wiimote_spi_send(1);
		wiimote_spi_send(2);
		wiimote_spi_send(3);
		wiimote_spi_send(4);
		prev_time = app_timer;
	}

	// Re-register timer
	btstack_run_loop_set_timer(&app_loop, APP_LOOP_PERIOD_MS);
	btstack_run_loop_add_timer(&app_loop);
} 

int btstack_main(int argc, const char * argv[]);
int btstack_main(int argc, const char * argv[]) {
    (void)argc;
    (void)argv;

	spi_slave_init(GPIO_NUM_19, GPIO_NUM_21, GPIO_NUM_17, GPIO_NUM_18);
	uart_init();

	init_controllers();
	init_gamepads();
	init_wiimotes();
	
	gpio_set_direction(PAIR_PIN, GPIO_MODE_INPUT);
	gpio_set_pull_mode(PAIR_PIN, GPIO_PULLUP_ONLY);
	gpio_set_direction(SPI_EN, GPIO_MODE_OUTPUT);
	gpio_set_level(SPI_EN, 0);

    l2cap_init();
	
	// Name must contain "Nintendo" to keep Switch controllers connected? But "Nintendo Switch" causes button latency
	gap_set_local_name("Nintendo");
	
	hci_set_inquiry_mode(INQUIRY_MODE_RSSI_AND_EIR);
	hci_set_master_slave_policy(0);	// Always begin connection as master

    hci_event_callback_registration.callback = &hci_event_handler;
    hci_add_event_handler(&hci_event_callback_registration);
	l2cap_event_callback_registration.callback = &l2cap_event_handler;
    hci_add_event_handler(&l2cap_event_callback_registration);
	gap_event_callback_registration.callback = &gap_event_handler;
    hci_add_event_handler(&gap_event_callback_registration);

    setbuf(stdout, NULL);
	//if (!gpio_get_level(PAIR_PIN)) hci_dump_open(NULL, HCI_DUMP_STDOUT);	// Dump HCI and ACL logs if button is held
    hci_power_control(HCI_POWER_ON);

	timer_config_t config = {
		.alarm_en = false,
		.counter_en = false,
		.counter_dir = TIMER_COUNT_UP,
		.divider = 80	// 1 us per tick - ticks per second is (80 MHz / divider)
    };
    
	app_timer = 0;
    timer_init(TIMER_GROUP_1, TIMER_1, &config);
    timer_set_counter_value(TIMER_GROUP_1, TIMER_1, 0);
    timer_start(TIMER_GROUP_1, TIMER_1);
	
	app_loop.process = &app_loop_handler;
    btstack_run_loop_set_timer(&app_loop, APP_LOOP_PERIOD_MS);
    btstack_run_loop_add_timer(&app_loop);
	
    return 0;
}