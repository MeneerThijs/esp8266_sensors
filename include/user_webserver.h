/*
 * user_webserver.h
 *
 *  Created on: May 23, 2015
 *      Author: esp8266
 */

#ifndef __USER_WEBSERVER_H_
#define __USER_WEBSERVER_H_

#define SERVER_PORT 80

#define URLSize 10

typedef enum _ParmType {
	SWITCH_STATUS = 0,
	INFOMATION,
	WIFI,
	SCAN,
	REBOOT,
	DEEP_SLEEP,
	LIGHT_STATUS,
	CONNECT_STATUS,
	TRANSFER_DATA,
	USER_BIN
} ParmType;

typedef enum ProtocolType {
	GET = 0, POST,
} ProtocolType;

typedef struct URL_Frame {
	enum ProtocolType Type;
	char pSelect[URLSize];
	char pCommand[URLSize];
	char pFilename[URLSize];
} URL_Frame;

void user_webserver_init();

#endif
