#include "c_types.h"
#include "mem.h"
#include "user_interface.h"
#include "espconn.h"
#include "osapi.h"
#include "uart.h"

#include "flash_param.h"
#include "server.h"
#include "io.h"
#include "rboot-ota.h"
#include "config.h"

#ifdef CONFIG_GPIO

#endif

#define MSG_OK "OK"
#define MSG_ERROR "ERROR"
#define MSG_INVALID_CMD "UNKNOWN COMMAND"

#ifdef CONFIG_PARSE_TEST_UNIT
#endif

bool doflash = true;

// Temp store for new station config
struct station_config stconf;
// Temp store for new ap config
//struct softap_config apconf;

// reset timer changes back to STA+AP if we can't associate
#define RESET_TIMEOUT 15000 // 15 seconds
#define RESET_TIMEOUT_WIFI_MODE 225000 // 225 seconds (3 minutes + 45 seconds)
static ETSTimer resetTimer;
// Reassociate timer to delay change of association so the original request can finish
static ETSTimer reassTimer;


char ICACHE_FLASH_ATTR *my_strdup(char *str) {
	size_t len;
	char *copy;

	len = strlen(str) + 1;
	copy = (char*)os_malloc((u_int)len);
	if ( copy ) {
		os_memcpy(copy, str, len);
		return (copy);
	} else {
		return (NULL);
	}
}

char ICACHE_FLASH_ATTR **config_parse_args(char *buf, uint16 *argc) {
	const char delim[] = " \t";
	char *save, *tok;
	char **argv = (char **)os_malloc(sizeof(char *) * MAX_ARGS);	// note fixed length
	os_memset(argv, 0, sizeof(char *) * MAX_ARGS);

	*argc = 0;
	for (; *buf == ' ' || *buf == '\t'; ++buf); // absorb leading spaces
	for (tok = strtok_r(buf, delim, &save); tok; tok = strtok_r(NULL, delim, &save)) {
		argv[*argc] = my_strdup(tok);
		(*argc)++;
		if (*argc == MAX_ARGS) {
			break;
		}
	}
	return argv;
}

void ICACHE_FLASH_ATTR config_parse_args_free(uint8_t argc, char *argv[]) {
	uint8_t i;
	for (i = 0; i <= argc; ++i) {
		if (argv[i])
		{
			os_free(argv[i]);
		}
	}
	os_free(argv);
}

void ICACHE_FLASH_ATTR config_cmd_reset(serverConnData *conn, uint8_t argc, char *argv[]) {
	espbuffsentstring(conn, MSG_OK);
	system_restart();
}

void ICACHE_FLASH_ATTR config_cmd_restore(serverConnData *conn, uint8_t argc, char *argv[]) {
	espbuffsentstring(conn, MSG_OK);
	system_restore();
	system_restart();
}

#ifdef CONFIG_GPIO

#endif

void ICACHE_FLASH_ATTR config_cmd_baud(serverConnData *conn, uint8_t argc, char *argv[]) {
	flash_param_t *flash_param = flash_param_get();
	UartBitsNum4Char data_bits = GETUART_DATABITS(flash_param->uartconf0);
	UartParityMode parity = GETUART_PARITYMODE(flash_param->uartconf0);
	UartStopBitsNum stop_bits = GETUART_STOPBITS(flash_param->uartconf0);
	const char *stopbits[4] = { "?", "1", "1.5", "2" };
	const char *paritymodes[4] = { "E", "O", "N", "?" };
	if (argc == 0)
		espbuffsentprintf(conn, "BAUD %d %d %s %s"MSG_OK, flash_param->baud, data_bits + 5, paritymodes[parity], stopbits[stop_bits]);
	else {
		uint32_t baud = atoi(argv[1]);
		if ((baud > (UART_CLK_FREQ / 16)) || baud == 0) {
			espbuffsentstring(conn, MSG_ERROR);
			return;
		}
		if (argc > 1) {
			data_bits = atoi(argv[2]);
			if ((data_bits < 5) || (data_bits > 8)) {
				espbuffsentstring(conn, MSG_ERROR);
				return;
			}
			data_bits -= 5;
		}
		if (argc > 2) {
			if (strcmp(argv[3], "N") == 0)
				parity = NONE_BITS;
			else if (strcmp(argv[3], "O") == 0)
				parity = ODD_BITS;
			else if (strcmp(argv[3], "E") == 0)
				parity = EVEN_BITS;
			else {
				espbuffsentstring(conn, MSG_ERROR);
				return;
			}
		}
		if (argc > 3) {
			if (strcmp(argv[4], "1")==0)
				stop_bits = ONE_STOP_BIT;
			else if (strcmp(argv[4], "2")==0)
				stop_bits = TWO_STOP_BIT;
			else if (strcmp(argv[4], "1.5") == 0)
				stop_bits = ONE_HALF_STOP_BIT;
			else {
				espbuffsentstring(conn, MSG_ERROR);
				return;
			}
		}
		// pump and dump fifo
		while (TRUE) {
			uint32_t fifo_cnt = READ_PERI_REG(UART_STATUS(0)) & (UART_TXFIFO_CNT << UART_TXFIFO_CNT_S);
			if ((fifo_cnt >> UART_TXFIFO_CNT_S & UART_TXFIFO_CNT) == 0) {
				break;
			}
		}
		os_delay_us(10000);
		uart_div_modify(UART0, UART_CLK_FREQ / baud);
		flash_param->baud = baud;
		flash_param->uartconf0 = CALC_UARTMODE(data_bits, parity, stop_bits);
		WRITE_PERI_REG(UART_CONF0(UART0), flash_param->uartconf0);
		if (doflash) {
			if (flash_param_set())
				espbuffsentstring(conn, MSG_OK);
			else
				espbuffsentstring(conn, MSG_ERROR);
		}
		else
			espbuffsentstring(conn, MSG_OK);
	}
}

void ICACHE_FLASH_ATTR config_cmd_flash(serverConnData *conn, uint8_t argc, char *argv[]) {
	bool err = false;

	if (argc == 0)
		espbuffsentprintf(conn, "FLASH=%d", doflash);
	else if (argc != 1)
		err=true;
	else {
		if (strcmp(argv[1], "1") == 0)
			doflash = true;
		else if (strcmp(argv[1], "0") == 0)
			doflash = false;
		else
			err=true;
	}
	if (err)
		espbuffsentstring(conn, MSG_ERROR);
	else
		espbuffsentstring(conn, MSG_OK);
}

void ICACHE_FLASH_ATTR config_cmd_port(serverConnData *conn, uint8_t argc, char *argv[]) {
	flash_param_t *flash_param = flash_param_get();

	if (argc == 0)
		espbuffsentprintf(conn, "PORT %d"MSG_OK, flash_param->port);
	else if (argc != 1)
		espbuffsentstring(conn, MSG_ERROR);
	else {
		uint32_t port = atoi(argv[1]);
		if ((port == 0)||(port>65535)) {
			espbuffsentstring(conn, MSG_ERROR);
		} else {
			if (port != flash_param->port) {
				flash_param->port = port;
				if (flash_param_set())
					espbuffsentstring(conn, MSG_OK);
				else
					espbuffsentstring(conn, MSG_ERROR);
				os_delay_us(10000);
				system_restart();
			} else {
				espbuffsentstring(conn, MSG_OK);
			}
		}
	}
}

void ICACHE_FLASH_ATTR config_cmd_mode(serverConnData *conn, uint8_t argc, char *argv[]) {
	uint8_t mode;

	if (argc == 0) {
		espbuffsentprintf(conn, "MODE=%d"MSG_OK, wifi_get_opmode());
	} else if (argc != 1) {
		espbuffsentstring(conn, MSG_ERROR);
	} else {
		mode = atoi(argv[1]);
		if (mode >= 1 && mode <= 3) {
			if (wifi_get_opmode() != mode) {
				wifi_set_opmode(mode);
				system_restart();
			} else {
				espbuffsentstring(conn, MSG_OK);
			}
		} else {
			espbuffsentstring(conn, MSG_ERROR);
		}
	}
}

// This routine is ran some time after a connection attempt to an access point. If
// the connect succeeds, this gets the module in STA-only mode. If it fails, it ensures
// that the module is in STA+AP mode so the user has a chance to recover.
static void ICACHE_FLASH_ATTR resetTimerCb(void *arg) {
	uint8_t station_count;
	int wifi_mode = wifi_get_opmode();

	if (wifi_mode == SOFTAP_MODE) return; // 2=AP, in AP-only mode we don't do any auto-switching

	if (wifi_mode == STATIONAP_MODE) {
		station_count = wifi_softap_get_station_num();
		if (!station_count) {
			wifi_set_opmode_current(STATION_MODE); // Modo STA only
		}
	}

	os_timer_disarm(&resetTimer);
	os_timer_setfn(&resetTimer, resetTimerCb, NULL);
	os_timer_arm(&resetTimer, RESET_TIMEOUT, 0); // check one more time*/
}

// Callback actually doing reassociation
static void ICACHE_FLASH_ATTR reassTimerCb(void *arg) {
	//DBG("Wifi changing association\n");
	wifi_station_disconnect();
	stconf.bssid_set = 0;
	wifi_station_set_config(&stconf);
	wifi_station_connect();
	// Schedule check, we give some extra time (4x) 'cause the reassociation can cause the AP
	// to have to change channel, and then the client needs to follow before it can see the
	// IP address
	os_timer_disarm(&resetTimer);
	os_timer_setfn(&resetTimer, resetTimerCb, NULL);
	os_timer_arm(&resetTimer, RESET_TIMEOUT_WIFI_MODE, 0);
}

/**
 *
 */
void ICACHE_FLASH_ATTR setup_wifi_station_mode(char *ssid, char *pwd)
{
	struct station_config stconf;
	struct softap_config apconf;

	wifi_station_disconnect();
	wifi_set_opmode_current(STATION_MODE); // Modo STA (Station mode)
	wifi_station_get_config_default(&stconf);

	if (os_strcmp(ssid, "") == 0) { // Primeira execução se não tem SSID na flash
		os_strncpy(stconf.ssid, WIFI_SSID, sizeof(stconf.ssid));
		os_strncpy(stconf.password, WIFI_PWD, sizeof(stconf.password));
		stconf.bssid_set = 0;
		wifi_station_set_config(&stconf);
	} else if ((os_strcmp(stconf.ssid, ssid) != 0) || (os_strcmp(stconf.password, pwd) != 0)) { // Não são iguais
		os_strncpy(stconf.ssid, ssid, sizeof(stconf.ssid));
		os_strncpy(stconf.password, pwd, sizeof(stconf.password));
		stconf.bssid_set = 0;
		wifi_station_set_config(&stconf);
	}

	wifi_station_connect();

	wifi_softap_get_config(&apconf);
	apconf.max_connection = 1; // limit to one device connected
	wifi_softap_set_config(&apconf);
	wifi_station_set_auto_connect(1);

	// check on the wifi in a few seconds to see whether we need to switch mode
	os_timer_disarm(&resetTimer);
	os_timer_setfn(&resetTimer, resetTimerCb, NULL);
	os_timer_arm(&resetTimer, RESET_TIMEOUT_WIFI_MODE, 0);
}

// spaces are not supported in the ssid or password
void ICACHE_FLASH_ATTR config_cmd_sta(serverConnData *conn, uint8_t argc, char *argv[]) {
	char *ssid = argv[1], *password = argv[2];
	struct station_config sta_conf;

	os_bzero(&sta_conf, sizeof(struct station_config));

	if (argc == 0) {
		wifi_station_get_config(&sta_conf);
		espbuffsentprintf(conn, "CURRENT SSID=%s PASSWORD=%s", sta_conf.ssid, sta_conf.password);
	}
	else if (argc == 1) {
		wifi_station_get_config_default(&sta_conf);
		espbuffsentprintf(conn, "FLASH SSID=%s PASSWORD=%s", sta_conf.ssid, sta_conf.password);
	}
	else if (argc > 2) {
		espbuffsentstring(conn, MSG_ERROR);
	} else {
		espbuffsentstring(conn, MSG_OK);

		setup_wifi_station_mode(ssid, password);
	}
}

// spaces are not supported in the ssid or password
void ICACHE_FLASH_ATTR config_cmd_ap(serverConnData *conn, uint8_t argc, char *argv[]) {
	char *ssid = argv[1], *password = argv[2];
	struct softap_config ap_conf;
//#define MAXAUTHMODES 5
	os_bzero(&ap_conf, sizeof(struct softap_config));
	wifi_softap_get_config(&ap_conf);
	if (argc == 0) {
		espbuffsentprintf(conn, "SSID=%s PASSWORD=%s AUTHMODE=%d IS_HIDDEH_SSID=%d CHANNEL=%d"MSG_OK, ap_conf.ssid, ap_conf.password, ap_conf.authmode, ap_conf.ssid_hidden, ap_conf.channel);
	} else if (argc > 5) {
		espbuffsentstring(conn, MSG_ERROR);
	} else { // argc > 0 && arg < 5
		os_strncpy(ap_conf.ssid, ssid, sizeof(ap_conf.ssid));
		ap_conf.ssid_len = strlen(ssid); //without set ssid_len, no connection to AP is possible
		if (argc == 1) { //  no password
			os_bzero(ap_conf.password, sizeof(ap_conf.password));
			ap_conf.authmode = AUTH_OPEN;
		} else { // with password
			os_strncpy(ap_conf.password, password, sizeof(ap_conf.password));
			if (argc > 2) { // authmode
				int amode = atoi(argv[3]);
				if ((amode < 1) || (amode > 4)) {
					espbuffsentstring(conn, MSG_ERROR);
					return;
				}
				ap_conf.authmode = amode;
			}
			if (argc > 3) { //ssid_hidden
				int ssid_hidden = atoi(argv[4]);
				if ((ssid_hidden > 1) || (ssid_hidden < 0)){
					espbuffsentstring(conn, MSG_ERROR);
					return;
				}
				ap_conf.ssid_hidden = ssid_hidden;
				if (argc > 4) { //channel
					int chan = atoi(argv[5]);
					if ((chan < 1) || (chan>13)){
						espbuffsentstring(conn, MSG_ERROR);
						return;
					}
					ap_conf.channel = chan;
				}
			}
		}
		espbuffsentstring(conn, MSG_OK);
		ETS_UART_INTR_DISABLE();
		wifi_softap_set_config(&ap_conf);
		ETS_UART_INTR_ENABLE();
	}
}

// spaces are not supported in the ssid or password
void ICACHE_FLASH_ATTR config_cmd_ap_name(serverConnData *conn, uint8_t argc, char *argv[]) {
	struct softap_config ap_conf;

	os_bzero(&ap_conf, sizeof(struct softap_config));
	wifi_softap_get_config(&ap_conf);
	if (argc == 0) {
		espbuffsentprintf(conn, "%s", ap_conf.ssid);
	}
}

/**
 * Set maximum value of RF Tx Power; unit : 0.25 dBm.
 * maximum value of RF Tx Power, unit: 0.25 dBm, range [0, 82].
 * It can be set by referring to the 34th byte (target_power_qdb_0) of
 * esp_init_data_default.bin (0 ~ 127 bytes).
 */
void ICACHE_FLASH_ATTR config_cmd_rfpower(serverConnData *conn, uint8_t argc, char *argv[]) {
	uint8_t power;

	if (argc == 0) {
		espbuffsentprintf(conn, "AT RFPOWER [0,1,2,3,4]");
	} else
	if (argc == 1) {
		power = atoi(argv[1]);
		switch (power) {
			case 0: // 0%
				system_phy_set_max_tpw(0);
				espbuffsentprintf(conn, "RFPOWER = 0");
				break;
			case 1: // 25%
				system_phy_set_max_tpw(38);
				espbuffsentprintf(conn, "RFPOWER = 25%");
				break;
			case 2: // 50%
				system_phy_set_max_tpw(57);
				espbuffsentprintf(conn, "RFPOWER = 50%");
				break;
			case 3: // 75%
				system_phy_set_max_tpw(73);
				espbuffsentprintf(conn, "RFPOWER = 75%");
				break;
			case 4: // 100%
				system_phy_set_max_tpw(82);
				espbuffsentprintf(conn, "RFPOWER = 100%");
				break;
			default:
				system_phy_set_max_tpw(power);
				espbuffsentprintf(conn, "RFPOWER = %d", power);
				break;
		}
	} else {
		espbuffsentstring(conn, MSG_ERROR);
	}
}

void ICACHE_FLASH_ATTR config_cmd_info_sdk(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "SDK: v%s", system_get_sdk_version());
/*
    os_sprintf(msg, "Free Heap: %d", system_get_free_heap_size());
*/
    espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_info_heap(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "Free Heap: %d", system_get_free_heap_size());

    espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_info_cpu(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "CPU Frequency: %d MHz", system_get_cpu_freq());

    espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_info_chip_id(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "System Chip ID: 0x%x", system_get_chip_id());

    espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_info_flash_id(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "SPI Flash ID: 0x%x", spi_flash_get_id());

    espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_info_flash_size(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "SPI Flash Size: %d", (1 << ((spi_flash_get_id() >> 16) & 0xff)));

    espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_info_rom(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "Currently running rom %d.", rboot_get_current_rom());

    espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_switch(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];
	uint8 before, after;

	//disable global interrupt
	ETS_GPIO_INTR_DISABLE();

	before = rboot_get_current_rom();
	after = !before;
	if (rboot_set_current_rom(after)) {
		os_sprintf(msg, "SUCESS - Swapping from rom %d to rom %d.", before, after);
	} else {
		os_sprintf(msg, "FAIL - Swapping from rom %d to rom %d.", before, after);
	}
	espbuffsentstring(conn, msg);

	system_restart();
}

static void ICACHE_FLASH_ATTR OtaUpdate_CallBack(bool result, uint8 rom_slot) {
	char msg[50];

	if(result == true) {
		// success
		if (rom_slot == FLASH_BY_ADDR) {
			os_sprintf(msg, "Write successful.");
		} else {
			// set to boot new rom and then reboot
			os_sprintf(msg, "Firmware updated, rebooting to rom %d...", rom_slot);
			rboot_set_current_rom(rom_slot);
			os_delay_us(10000);
			system_restart();
		}
	} else {
		// fail
		os_sprintf(msg, "Firmware update failed!");
	}
}

//static void ICACHE_FLASH_ATTR OtaUpdate() {
void ICACHE_FLASH_ATTR config_cmd_ota(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];
	char *val, *val2;
	char *host = argv[1], *port = argv[2];

	if (argc == 0) {
		val = "192.168.1.107";
		setOtaHostIp (val);
		setOtaHostPort (8080);
	} else if (argc == 1) {
		setOtaHostIp (host);
		setOtaHostPort (8080);
	} else if (argc == 2) {
		setOtaHostIp (host);
		setOtaHostPort (atoi(port));
	}

	//disable global interrupt
	ETS_GPIO_INTR_DISABLE();
	// start the upgrade process
	if (rboot_ota_start((ota_callback)OtaUpdate_CallBack)) {
		os_sprintf(msg, "Updating...");
	} else {
		os_sprintf(msg, "Updating failed!");
	}

	espbuffsentstring(conn, msg);
}

static void ICACHE_FLASH_ATTR tcpclient_sent_cb(void *arg)
{
	struct espconn *pespconn = arg;
	espconn_disconnect(pespconn);
}

LOCAL void ICACHE_FLASH_ATTR tcp_con_cb(void *arg) {
    // connect call back
	struct espconn *pespconn = arg;
	char payload[128];

	espconn_regist_sentcb(pespconn, tcpclient_sent_cb);
	//os_sprintf(payload, MACSTR ",%s", MAC2STR(macaddr), "ESP8266");
	os_sprintf(payload, "ESP8266");
	sint8 espsent_status = espconn_sent(pespconn, payload, strlen(payload));
}

void ICACHE_FLASH_ATTR config_cmd_log_alarm(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[500];
	char tcpserverip[15];
	struct ip_info info;
	struct espconn Conn1;
	esp_tcp ConnTcp;

	if (wifi_get_ip_info(STATION_IF, &info))
    {
		struct espconn *pespconn = (struct espconn *)os_zalloc(sizeof(struct espconn));
		pespconn->type = ESPCONN_TCP;
		pespconn->state = ESPCONN_NONE;
		pespconn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
		pespconn->proto.tcp->local_port = espconn_port();
		pespconn->proto.tcp->remote_port = 80;

		uint32_t ip = ipaddr_addr("192.168.1.1");
		os_memcpy(pespconn->proto.tcp->remote_ip, &ip, 4);

		espconn_regist_connectcb(pespconn, tcp_con_cb);
		int8_t res = espconn_connect(pespconn);
		os_sprintf(msg, "---X--- espconn_connect() returned %d", res);
	}
	else
	{
		os_sprintf(msg, "wifi_get_ip_info failed.");
	}

	espbuffsentstring(conn, msg);
}

void ICACHE_FLASH_ATTR config_cmd_deep_sleep(serverConnData *conn, uint8_t argc, char *argv[]) {
	char msg[50];

    os_sprintf(msg, "DEEP SLEEP");
    espbuffsentstring(conn, msg);
    deep_sleep_set_option( 0 );
    system_deep_sleep( 90 * 1000 * 1000 );
}

void ICACHE_FLASH_ATTR config_cmd_version(serverConnData *conn, uint8_t argc, char *argv[]) {


	if (argc == 0) {
		espbuffsentprintf(conn, "%s", VERSION_EAS);
	}
	else {
		espbuffsentstring(conn, MSG_ERROR);
	}
}

const config_commands_t config_commands[] = {
	{ "RESET", &config_cmd_reset },
	{ "RESTORE", &config_cmd_restore },
	{ "BAUD", &config_cmd_baud },
	{ "PORT", &config_cmd_port },
	{ "MODE", &config_cmd_mode },
	{ "STA", &config_cmd_sta },
	{ "AP", &config_cmd_ap },
	{ "APNAME", &config_cmd_ap_name },
	{ "FLASH", &config_cmd_flash },
	{ "GPIO2", &config_cmd_gpio2 },
//	{ "GPIO4", &config_cmd_gpio4 },//	{ "GPIO5", &config_cmd_gpio5 },//	{ "GPIO12", &config_cmd_gpio12 },//	{ "GPIO13", &config_cmd_gpio13 },//	{ "GPIO14", &config_cmd_gpio14 },
	{ "MAPA", &config_cmd_mapping },
	{ "RESETDSP", &config_cmd_reset_dsp },
	{ "PISCALED", &config_cmd_gpio15 },
	{ "GPIO15", &config_cmd_gpio15 },
	{ "GPIO16", &config_cmd_gpio16 },
	{ "VERSION", &config_cmd_version },
	{ "RFPOWER", &config_cmd_rfpower },
//----------------------------------------------------------------
	{ "INFO", &config_cmd_info_sdk },
	{ "HEAP", &config_cmd_info_heap },
	{ "CPU", &config_cmd_info_cpu },
	{ "CHIPID", &config_cmd_info_chip_id },
	{ "FLASHID", &config_cmd_info_flash_id },
	{ "FLASHSIZE", &config_cmd_info_flash_size },
	{ "ROM", &config_cmd_info_rom },
	{ "SWITCH", &config_cmd_switch },
	{ "OTA", &config_cmd_ota },
	{ "LOGALARM", &config_cmd_log_alarm},
	{ "DEEP", &config_cmd_deep_sleep},
//----------------------------------------------------------------
	{ NULL, NULL }
};

void ICACHE_FLASH_ATTR config_parse(serverConnData *conn, char *buf, uint16 len) {
	char *lbuf = (char *)os_malloc(len + 1), **argv;
	uint16 i, argc;

	// we need a '\0' end of the string
	os_memcpy(lbuf, buf, len);
	lbuf[len] = '\0';

	// command echo
	//espbuffsent(conn, lbuf, len);

	// remove any CR / LF
	for (i = 0; i < len; ++i)
		if (lbuf[i] == '\n' || lbuf[i] == '\r')
			lbuf[i] = '\0';

	// verify the command prefix
	if (os_strncmp(lbuf, "AT", 2) != 0) {
		os_free(lbuf);
		return;
	}
	// parse out buffer into arguments
	argv = config_parse_args(&lbuf[3], &argc);
#if 0
// debugging
	{
		uint8_t i;
		for (i = 0; i < argc; ++i) {
			espbuffsentprintf(conn, "argument %d: '%s'", i, argv[i]);
		}
	}
// end debugging
#endif
	if (argc == 0) {
		espbuffsentstring(conn, MSG_OK);
	} else {
		argc--;	// to mimic C main() argc argv
		for (i = 0; config_commands[i].command; ++i) {
			if (os_strncmp(argv[0], config_commands[i].command, strlen(argv[0])) == 0) {
				config_commands[i].function(conn, argc, argv);
				break;
			}
		}
		if (!config_commands[i].command) {
			espbuffsentstring(conn, MSG_INVALID_CMD);
		}
	}
	config_parse_args_free(argc, argv);
	os_free(lbuf);
}

#ifdef CONFIG_PARSE_TEST_UNIT
const int max_line = 255;
int main(int argc, char *argv[]) {
	char line[max_line];

	// read lines and feed them to config_parse
	while (fgets(line, max_line, stdin) != NULL) {
		uint8_t len = strlen(line);
		config_parse(NULL, line, len);
	}
}
#endif
