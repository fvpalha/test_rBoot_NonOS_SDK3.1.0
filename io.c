#include "c_types.h"
#include "osapi.h"
#include "ets_sys.h"
#include "io.h"
#include "gpio16.h"
#include "user_interface.h"
#include "flash_param.h"

static ETSTimer resetBtntimer;
//uint8_t decibel = 0;

#define MSG_ERROR "ERROR\r\n"

void ICACHE_FLASH_ATTR config_cmd_gpio2(serverConnData *conn, uint8_t argc, char *argv[]) {
	int laststate = 1;
	int i;

	if (argc == 0) {
		laststate = gpio_read(GPIO_2_PIN);
		espbuffsentprintf(conn, "GPIO2=%d\r\n", laststate);
	}
	else {
		uint16_t gpiodelay = 100;
		if (argc == 2) {
			gpiodelay = atoi(argv[2]);
		}
		uint8_t value = atoi(argv[1]);
		if (value < 3) {
			if (value == 0) {
				gpio_write(GPIO_2_PIN, value);
				espbuffsentstring(conn, "LOW\r\n");
			}
			if (value == 1) {
				gpio_write(GPIO_2_PIN, value);
				espbuffsentstring(conn, "HIGH\r\n");
			}
			if (value == 2) {
				gpio_pulse(GPIO_2_PIN, gpiodelay*1000);
				espbuffsentstring(conn, "RESET\r\n");
			}
		} else {
			espbuffsentstring(conn, MSG_ERROR);
		}
	}
}

void ICACHE_FLASH_ATTR config_cmd_mapping(serverConnData *conn, uint8_t argc, char *argv[]) {

	while (gpio_read(GPIO_14_PIN)); // Aguardar enquanto pino estiver ocupado - ARM está calculando Média
	if (argc > 0) {
		uint8_t pulse_size;
		uint8_t value = atoi(argv[1]);
		set_gpio_mode(GPIO_14_PIN, GPIO_OUTPUT, GPIO_FLOAT);
		gpio_write(GPIO_14_PIN, 0);
		if (value < 3) {
			switch (value) {
				case CMDMAP30: // 0 - 30 minutos
					pulse_size = 10;
					espbuffsentstring(conn, "MAPPING 30 min");
				break;
				case CMDMAPNT: // 1	- Contínuo
					pulse_size = 20;
					espbuffsentstring(conn, "MAPPING continuous");
				break;
				case CMDMAPCMP: // 2 - Complementar
					pulse_size = 30;
					espbuffsentprintf(conn, "MAPPING complement");
				break;
				default:
				break;
			}

			gpio_write(GPIO_14_PIN, 1);
			os_delay_us(pulse_size * 1000);  // largura de pulso mapeamento
			gpio_write(GPIO_14_PIN, 0);
		} else {
			espbuffsentstring(conn, MSG_ERROR);
		}

		set_gpio_mode(GPIO_14_PIN, GPIO_INPUT, GPIO_FLOAT);
	}
}

void ICACHE_FLASH_ATTR config_cmd_reset_dsp(serverConnData *conn, uint8_t argc, char *argv[]) {

	while (gpio_read(GPIO_14_PIN)); // Aguardar enquanto pino estiver ocupado - ARM está calculando Média
	if (argc == 0) {
		set_gpio_mode(GPIO_14_PIN, GPIO_OUTPUT, GPIO_FLOAT);
		gpio_write(GPIO_14_PIN, 1);
		os_delay_us(40000); // largura de pulso para reset geral
		gpio_write(GPIO_14_PIN, 0);
		espbuffsentstring(conn, "RESET DSP");

		set_gpio_mode(GPIO_14_PIN, GPIO_INPUT, GPIO_FLOAT);
	} else {
		espbuffsentstring(conn, MSG_ERROR);
	}
}

void ICACHE_FLASH_ATTR config_cmd_gpio15(serverConnData *conn, uint8_t argc, char *argv[]) {
	if (argc == 0)
		espbuffsentprintf(conn, "Args: 0=low, 1=high, 2 <delay in ms>=reset (delay optional).\r\n");
	else {
		uint16_t gpiodelay = 100;
		if (argc == 2) {
			gpiodelay = atoi(argv[2]);
		}
		uint8_t value = atoi(argv[1]);
		if (value < 3) {
			if (value == 0) {
				gpio_write(GPIO_15_PIN, value);
				espbuffsentstring(conn, "LOW\r\n");
			}
			if (value == 1) {
				gpio_write(GPIO_15_PIN, value);
				espbuffsentstring(conn, "HIGH\r\n");
			}
			if (value == 2) {
				gpio_pulse(GPIO_15_PIN, gpiodelay*1000);
				espbuffsentstring(conn, "RESET\r\n");
			}
		} else {
			espbuffsentstring(conn, MSG_ERROR);
		}
	}
}

void ICACHE_FLASH_ATTR config_cmd_gpio16(serverConnData *conn, uint8_t argc, char *argv[]) {
	if (argc == 0)
		espbuffsentprintf(conn, "Args: 0=low, 1=high, 2 <delay in ms>=reset (delay optional).\r\n");
	else {
		uint16_t gpiodelay = 100;
		if (argc == 2) {
			gpiodelay = atoi(argv[2]);
		}
		uint8_t value = atoi(argv[1]);
		if (value < 3) {
			if (value == 0) {
				gpio_write(GPIO_16_PIN, value);
				espbuffsentstring(conn, "LOW\r\n");
			}
			if (value == 1) {
				gpio_write(GPIO_16_PIN, value);
				espbuffsentstring(conn, "HIGH\r\n");
			}
			if (value == 2) {
				gpio_pulse(GPIO_16_PIN, gpiodelay*1000);
				espbuffsentstring(conn, "RESET\r\n");
			}
		} else {
			espbuffsentstring(conn, MSG_ERROR);
		}
	}
}

void ICACHE_FLASH_ATTR intr_callback(unsigned pin, unsigned level) {

	// Get the pin reading.
	int reading = gpio_read(pin);

	switch (pin) {
	case GPIO_0_PIN:
		break;
	default:
		break;
	}
}

static void ICACHE_FLASH_ATTR pulseTimer(void *arg) {
	static uint8_t alarmCmd = 0x9E;
	char *data;
	unsigned short len;

	if (wifi_get_opmode() == SOFTAP_MODE) {
		//system_phy_set_max_tpw(decibel);// 0 ~ 82 (hex) - 0,25 dBm unit

		struct station_info *stationInfo = wifi_softap_get_station_info();
		if (!gpio_read(GPIO_5_PIN)) {
			data = &alarmCmd;
			len = 0x01;
			uart0_tx_buffer(data, len);

			os_timer_disarm(&resetBtntimer);
			deep_sleep_set_option( 0 );
			system_deep_sleep( 0 ); // Sleep forever
		}
		wifi_softap_free_station_info(); // Free it by calling functions
/*
 	 	// Rampa de potência de 1 decibel  (440 segundos)
 		if ((countLKModeDelay % 85) == 0)
		{
			decibel += 1;
			if (decibel > 82) {
				decibel = 0;
			}
		}
*/
	}

}

static void ICACHE_FLASH_ATTR enableInterrupt(unsigned pin, unsigned gpio_type, gpio_intr_handler icb) {
	set_gpio_mode(pin, GPIO_INT, GPIO_FLOAT);
	gpio_intr_init(pin, gpio_type);
	gpio_intr_attach(icb);
}

void ioTimerInit(void) {

	os_timer_disarm(&resetBtntimer);
	os_timer_setfn(&resetBtntimer, pulseTimer, NULL);
	os_timer_arm(&resetBtntimer, 62, 1);
}


ETSTimer* getResetBtntimer(void)
{
	return &resetBtntimer;
}

/*
#define GPIO_0_PIN  3  // GPIO0		- RET_ALARME
#define GPIO_1_PIN  10 // GPIO1		- TX
#define GPIO_2_PIN  4  // GPIO2
#define GPIO_3_PIN  9  // GPIO3		- RX
#define GPIO_4_PIN  2  // GPIO4		- INTCOM
#define GPIO_5_PIN  1  // GPIO5		- LK1 - wifi mode
#define GPIO_12_PIN 6  // GPIO12	- TX ENABLE
#define GPIO_13_PIN 7  // GPIO13
#define GPIO_14_PIN 5  // GPIO14	- ARM
#define GPIO_15_PIN 8  // GPIO15	-
#define GPIO_16_PIN 0  // GPIO16	-
*/
void ioInit(void) {

	// Configurar entradas
	// RET_ALARM - GPIO0 - Pin 18 - indicação de alarme da antena - borda negativa (queda do sinal)
	while (gpio_read(GPIO_0_PIN) == 0);
	enableInterrupt(GPIO_0_PIN, GPIO_PIN_INTR_NEGEDGE, intr_callback);
	set_gpio_mode(GPIO_2_PIN, GPIO_INPUT, GPIO_FLOAT);
	set_gpio_mode(GPIO_12_PIN, GPIO_INPUT, GPIO_FLOAT);
	set_gpio_mode(GPIO_13_PIN, GPIO_INPUT, GPIO_FLOAT);
	// GPIO14 - Pin 5 - Comunicação com o ARM - a interrupção recebe indicação do ARM sobre estar ocupado
	set_gpio_mode(GPIO_14_PIN, GPIO_INPUT, GPIO_FLOAT);
	set_gpio_mode(GPIO_15_PIN, GPIO_INPUT, GPIO_FLOAT);
	set_gpio_mode(GPIO_16_PIN, GPIO_INPUT, GPIO_FLOAT);

}
