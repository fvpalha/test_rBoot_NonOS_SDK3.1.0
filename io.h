#ifndef IO_H
#define IO_H

#include "server.h"
#include "gpio16.h"

#define CMDMAP30    0 	// 30 minutos
#define PULSEMAP30  5
#define CMDMAPNT    1	// Contínuo
#define PULSEMAPNT  10
#define CMDMAPCMP   2	// Comlementar
#define PULSEMAPCMP 15

void ICACHE_FLASH_ATTR config_cmd_gpio2(serverConnData *conn, uint8_t argc, char *argv[]);
//void ICACHE_FLASH_ATTR config_cmd_gpio4(serverConnData *conn, uint8_t argc, char *argv[]);
//void ICACHE_FLASH_ATTR config_cmd_gpio5(serverConnData *conn, uint8_t argc, char *argv[]);
//void ICACHE_FLASH_ATTR config_cmd_gpio12(serverConnData *conn, uint8_t argc, char *argv[]);
//void ICACHE_FLASH_ATTR config_cmd_gpio13(serverConnData *conn, uint8_t argc, char *argv[]);
//void ICACHE_FLASH_ATTR config_cmd_gpio14(serverConnData *conn, uint8_t argc, char *argv[]);
void ICACHE_FLASH_ATTR config_cmd_mapping(serverConnData *conn, uint8_t argc, char *argv[]);
void ICACHE_FLASH_ATTR config_cmd_reset_dsp(serverConnData *conn, uint8_t argc, char *argv[]);
void ICACHE_FLASH_ATTR config_cmd_gpio15(serverConnData *conn, uint8_t argc, char *argv[]);
void ICACHE_FLASH_ATTR config_cmd_gpio16(serverConnData *conn, uint8_t argc, char *argv[]);
void ICACHE_FLASH_ATTR intr_callback(unsigned pin, unsigned level);
static void ICACHE_FLASH_ATTR pulseTimer(void *arg);
static void ICACHE_FLASH_ATTR enableInterrupt(unsigned pin, unsigned gpio_type, gpio_intr_handler icb);
void ioTimerInit(void);
void ioInit(void);
ETSTimer* getResetBtntimer(void);

#endif
