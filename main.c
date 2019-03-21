#include "c_types.h"
#include "osapi.h"
#include "user_interface.h"
#include "main.h"
#include "flash_param.h"
#include "gpio16.h"
#include "config.h"
#include "server.h"
#include "uart.h"
#include <rboot-api.h>

/*
#define SDK_RF_CAL_ADDR 0x3FB000
#define SDK_PHY_DATA_ADDR 0x3FC000
#define SDK_PARAM_ADDR 0x3FD000
#define SPI_FLASH_SIZE_MAP  FLASH_SIZE_32M_MAP_1024_1024 // 2
#define SDK_PRIV_PARAM_ADDR 0x7C000
#define SYSTEM_PARTITION_CUSTOMER_PRIV_PARAM SYSTEM_PARTITION_CUSTOMER_BEGIN
*/

#define FLASH_SIZE_SDK				FLASH_SIZE_32M_MAP_1024_1024
#define RFCAL_OFFSET				0xfb000
#define RFCAL_SIZE					0x1000
#define PHYDATA_OFFSET				0x1fc000
#define PHYDATA_SIZE				0x1000
#define SYSTEM_CONFIG_OFFSET		0x3FD000
#define SYSTEM_CONFIG_SIZE			0x3000
#define USER_CONFIG_OFFSET			0xfa000
#define USER_CONFIG_SIZE			0x1000
#define OFFSET_OTA_BOOT				0x000000
#define SIZE_OTA_BOOT				0x1000
#define OFFSET_OTA_RBOOT_CFG		0x1000
#define SIZE_OTA_RBOOT_CFG			0x1000
#define OFFSET_OTA_IMG_0			0x002000
#define OFFSET_OTA_IMG_1			0x202000
#define SIZE_OTA_IMG				0xF0000

static const partition_item_t p_table[] = {
		{	SYSTEM_PARTITION_RF_CAL, 				RFCAL_OFFSET,				RFCAL_SIZE,				},
		{	SYSTEM_PARTITION_PHY_DATA,				PHYDATA_OFFSET,				PHYDATA_SIZE,			},
		{	SYSTEM_PARTITION_SYSTEM_PARAMETER,		SYSTEM_CONFIG_OFFSET,		SYSTEM_CONFIG_SIZE,		},
		{	SYSTEM_PARTITION_CUSTOMER_BEGIN + 0,	USER_CONFIG_OFFSET,			USER_CONFIG_SIZE,		},
		{	SYSTEM_PARTITION_CUSTOMER_BEGIN + 1,	OFFSET_OTA_BOOT,			SIZE_OTA_BOOT,			},
		{	SYSTEM_PARTITION_CUSTOMER_BEGIN + 2,	OFFSET_OTA_RBOOT_CFG,		SIZE_OTA_RBOOT_CFG,		},
		{	SYSTEM_PARTITION_CUSTOMER_BEGIN + 3,	OFFSET_OTA_IMG_0,			SIZE_OTA_IMG,			},
		{	SYSTEM_PARTITION_CUSTOMER_BEGIN + 4,	OFFSET_OTA_IMG_1,			SIZE_OTA_IMG,			},
};

void user_pre_init(void);
void user_pre_init(void)
{
    if(!system_partition_table_regist(p_table, sizeof(p_table) / sizeof(*p_table), FLASH_SIZE_SDK)) {
        os_printf("system_partition_table_regist fail\r\n");
        while(1);
    }
}

void ICACHE_FLASH_ATTR user_rf_pre_init() {
	system_phy_set_powerup_option(3); // 3: full RF calibration on reset (200ms)
}
/*
uint32 user_iram_memory_is_enabled(void);
iram attr_const uint32 user_iram_memory_is_enabled(void)
{
    return(0);
}
*/

void ICACHE_FLASH_ATTR user_init(void) {
	char msg[50];
	struct station_config stconf;
	struct softap_config apconf;

	//disable global interrupt
	ETS_GPIO_INTR_DISABLE();

	system_set_os_print(0); // Disable logging
	//SET higher CPU freq & disable wifi sleep
	system_update_cpu_freq(SYS_CPU_160MHZ);
	wifi_fpm_set_sleep_type(NONE_SLEEP_T);
	wifi_set_sleep_type(NONE_SLEEP_T); // Turn off WiFi modem sleep - https://github.com/mongoose-os-libs/wifi/commit/752dac619cb318394a28ff518d3e67900445d34f

	flash_param_t *flash_param;
	flash_param_init();
	flash_param = flash_param_get();

	// Jumper boot LK1 - Select Mode (AP/STA)
//	set_gpio_mode(GPIO_5_PIN, GPIO_INPUT, GPIO_FLOAT);

/*	if (gpio_read(GPIO_5_PIN)) { // SEM JUMPER - UM - de seleção de modo de rede
		os_bzero(&stconf, sizeof(struct station_config));
		wifi_station_get_config_default(&stconf);
		setup_wifi_station_mode(stconf.ssid, stconf.password); //
		wifi_set_opmode_current(STATION_MODE);// Modo STATION
	} else { // COM JUMPER - ZERO
*/		// http://www.esp8266.com/viewtopic.php?f=32&t=10620
		wifi_softap_get_config(&apconf);
		apconf.max_connection = 1; // limit to one device connected
		wifi_softap_set_config(&apconf);
		wifi_set_opmode_current(SOFTAP_MODE);// Modo AP - Access Point
//	}

	serverInit(21); // only AT commands
	ioTimerInit();
	ioInit();
	uart_init(flash_param->baud, flash_param->baud);

	// RF TX Power reduction - 50% -- ~300 mA (65 ~ 490)
	//system_phy_set_rfoption(1);
	system_phy_set_max_tpw(57);// 0 ~ 82 (hex) - 0,25 dBm unit (12.5 decibeis)
}
