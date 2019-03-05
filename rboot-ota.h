#ifndef __RBOOT_OTA_H__
#define __RBOOT_OTA_H__

//////////////////////////////////////////////////
// rBoot OTA sample code for ESP8266.
// Copyright 2015 Richard A Burton
// richardaburton@gmail.com
// See license.txt for license terms.
// OTA code based on SDK sample from Espressif.
//////////////////////////////////////////////////

#include "rboot-api.h"

#ifdef __cplusplus
extern "C" {
#endif

// ota server details
//#define OTA_HOST "192.168.1.107"
//#define OTA_PORT 8080
#define OTA_ROM0 "rom0.bin"
#define OTA_ROM1 "rom1.bin"
// OTA_FILE is not required, but is part of the example
// code for writing arbitrary files to flash
#define OTA_FILE "file.bin"

// general http header
#define HTTP_HEADER "Connection: keep-alive\r\n\
Cache-Control: no-cache\r\n\
User-Agent: rBoot-Sample/1.0\r\n\
Accept: */*\r\n\r\n"
/* this comment to keep notepad++ happy */

// timeout for the initial connect and each recv (in ms)
#define OTA_NETWORK_TIMEOUT  10000

// used to indicate a non-rom flash
#define FLASH_BY_ADDR 0xff

// callback method should take this format
typedef void (*ota_callback)(bool result, uint8 rom_slot);

void ICACHE_FLASH_ATTR setOtaHostIp (char *value);
char* ICACHE_FLASH_ATTR getOtaHostIp (void);
void ICACHE_FLASH_ATTR setOtaHostPort (int value);

// function to perform the ota update
bool ICACHE_FLASH_ATTR rboot_ota_start(ota_callback callback);

typedef struct {
	uint8 rom_slot;   // rom slot to update, or FLASH_BY_ADDR
	ota_callback callback;  // user callback when completed
	uint32 total_len;
	uint32 content_len;
	struct espconn *conn;
	ip_addr_t ip;
	rboot_write_status write_status;
} upgrade_status;

#ifdef __cplusplus
}
#endif

#endif
