#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "server.h"

static const char *VERSION_EAS = "2019MAR20_SDK3";

// values will be passed in from the Makefile
#ifndef WIFI_SSID
#define WIFI_SSID "NAU_RT_EAS"
#endif
#ifndef WIFI_PWD
#define WIFI_PWD "!1A-*-2b#"
#endif

#define MAX_ARGS 65

typedef struct config_commands {
	char *command;
	void(*function)(serverConnData *conn, uint8_t argc, char *argv[]);
} config_commands_t;

void ICACHE_FLASH_ATTR setup_wifi_station_mode(char *ssid, char *pwd);
void ICACHE_FLASH_ATTR config_parse(serverConnData *conn, char *buf, uint16 len);

#define CONFIG_GPIO
#ifdef CONFIG_GPIO

#endif

#endif /* __CONFIG_H__ */
