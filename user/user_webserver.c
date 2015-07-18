/*
 * user_webserver.c
 *
 *  Created on: May 23, 2015
 *      Author: esp8266
 */

#include "user_interface.h"
#include "espconn.h"
#include "osapi.h"
#include "mem.h"

#include "user_esp_platform.h"
#include "user_webserver.h"
#include "user_json.h"

LOCAL char *precvbuffer;

LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend) {
	uint16 length = 0;
	char *pbuf = NULL;
	char httphead[256];
	struct espconn *ptrespconn = arg;
	os_memset(httphead, 0, 256);

	if (responseOK) {
		os_sprintf(httphead,
				"HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\n",
				psend ? os_strlen(psend) : 0);

		if (psend) {
			os_sprintf(httphead + os_strlen(httphead),
					"Content-type: application/json\r\nExpires: Fri, 10 Apr 2008 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
			length = os_strlen(httphead) + os_strlen(psend);
			pbuf = (char *) os_zalloc(length + 1);
			os_memcpy(pbuf, httphead, os_strlen(httphead));
			os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
		} else {
			os_sprintf(httphead + os_strlen(httphead), "\n");
			length = os_strlen(httphead);
		}
	} else {
		os_sprintf(httphead,
				"HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
		length = os_strlen(httphead);
	}

	if (psend) {
#ifdef SERVER_SSL_ENABLE
		espconn_secure_sent(ptrespconn, pbuf, length);
#else
		espconn_sent(ptrespconn, pbuf, length);
#endif
	} else {
#ifdef SERVER_SSL_ENABLE
		espconn_secure_sent(ptrespconn, httphead, length);
#else
		espconn_sent(ptrespconn, httphead, length);
#endif
	}

	if (pbuf) {
		os_free(pbuf);
		pbuf = NULL;
	}
}

LOCAL int ICACHE_FLASH_ATTR
version_get(struct jsontree_context *js_ctx) {
	const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
	char string[32];

	if (os_strncmp(path, "hardware", 8) == 0) {
		os_sprintf(string, "0.1");
	} else if (os_strncmp(path, "sdk_version", 11) == 0) {
		os_sprintf(string, "%s", system_get_sdk_version());
	} else if (os_strncmp(path, "iot_version", 11) == 0) {
//		os_sprintf(string, "%s%d.%d.%dt%d(%s)", VERSION_TYPE, IOT_VERSION_MAJOR,
//				IOT_VERSION_MINOR, IOT_VERSION_REVISION, device_type,
//				UPGRADE_FALG);
	}

	jsontree_write_string(js_ctx, string);

	return 0;
}

LOCAL int ICACHE_FLASH_ATTR
device_get(struct jsontree_context *js_ctx) {
	const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);

	if (os_strncmp(path, "manufacturer", 12) == 0) {
		jsontree_write_string(js_ctx, "Espressif Systems");
	} else if (os_strncmp(path, "product", 7) == 0) {
		jsontree_write_string(js_ctx, "Humiture");
		jsontree_write_string(js_ctx, "Flammable Gas");
		jsontree_write_string(js_ctx, "Plug");
		jsontree_write_string(js_ctx, "Light");
	}

	return 0;
}

LOCAL struct jsontree_callback version_callback = JSONTREE_CALLBACK(version_get,
		NULL);

LOCAL struct jsontree_callback device_callback =
JSONTREE_CALLBACK(device_get, NULL);

JSONTREE_OBJECT(device_tree, JSONTREE_PAIR("product", &device_callback),
		JSONTREE_PAIR("manufacturer", &device_callback));
JSONTREE_OBJECT(version_tree, JSONTREE_PAIR("hardware", &version_callback),
		JSONTREE_PAIR("sdk_version", &version_callback),
		JSONTREE_PAIR("iot_version", &version_callback),);
JSONTREE_OBJECT(info_tree, JSONTREE_PAIR("Version", &version_tree),
		JSONTREE_PAIR("Device", &device_tree));

JSONTREE_OBJECT(INFOTree, JSONTREE_PAIR("info", &info_tree));

/**
 * Return JSON object if KWH sensor is connected
 */
#ifdef KWH_SENSOR
LOCAL int ICACHE_FLASH_ATTR
rtc_get(struct jsontree_context *js_ctx) {
//	jsontree_write_int(js_ctx, user_get_sample_count());
	int bla[] = { 1, 2, 3, 4, 5, 6 };
	jsontree_write_atom(js_ctx, "[");
	jsontree_write_int_array(js_ctx, bla, 5);
	jsontree_write_atom(js_ctx, "]");
	return 0;
}

LOCAL int ICACHE_FLASH_ATTR
data_get(struct jsontree_context *js_ctx) {
	struct sample** sample_data;
	int i;
	uint16 sample_count;
	if (!(sample_count = user_get_sample_count())) {
		return 0;
	};
	DEBUG_HEAP;
	sample_data = (struct sample**) os_malloc(
			sizeof(struct sample*) * sample_count);
	sample_count = user_get_samples(0, sample_count, sample_data);
	os_printf("Got %d samples\r\n", sample_count);

	for (i = 0; i < sample_count; i++) {
		jsontree_write_int(js_ctx, sample_data[i]->data);
	}
	DEBUG_HEAP;
	os_free(sample_data);
	return 0;
}

LOCAL struct jsontree_callback rtc_callback =
JSONTREE_CALLBACK(rtc_get, NULL);

LOCAL struct jsontree_callback data_callback =
JSONTREE_CALLBACK(data_get, NULL);

JSONTREE_OBJECT(status_tree, JSONTREE_PAIR("rtc", &rtc_callback),
		JSONTREE_PAIR("data", &data_callback));

JSONTREE_ARRAY(status_array, JSONTREE_PAIR_ARRAY(&status_tree));

JSONTREE_OBJECT(response_tree, JSONTREE_PAIR("Response", &status_array));
JSONTREE_OBJECT(switch_tree, JSONTREE_PAIR("switch", &response_tree));

JSONTREE_OBJECT(DataTree, JSONTREE_PAIR("data", &switch_tree));
#endif // KWH_SENSOR

LOCAL void ICACHE_FLASH_ATTR
json_send(void *arg, ParmType ParmType) {
	char *pbuf = NULL;
	pbuf = (char *) os_zalloc(jsonSize);
	struct espconn *ptrespconn = arg;

	switch (ParmType) {
//#if LIGHT_DEVICE
//
//	case LIGHT_STATUS:
//	json_ws_send((struct jsontree_value *)&PwmTree, "light", pbuf);
//	break;
//#endif
//
//#if PLUG_DEVICE
//
//	case SWITCH_STATUS:
//	json_ws_send((struct jsontree_value *)&StatusTree, "switch", pbuf);
//	break;
//#endif

	case INFOMATION:
		json_ws_send((struct jsontree_value *) &INFOTree, "info", pbuf);
		break;

	case TRANSFER_DATA:
		json_ws_send((struct jsontree_value *) &DataTree, "data", pbuf);
		break;

//	case WIFI:
//		json_ws_send((struct jsontree_value *) &wifi_info_tree, "wifi", pbuf);
//		break;
//
//	case CONNECT_STATUS:
//		json_ws_send((struct jsontree_value *) &con_status_tree, "info", pbuf);
//		break;
//
//	case USER_BIN:
//		json_ws_send((struct jsontree_value *) &userinfo_tree, "user_info",
//				pbuf);
//		break;
//	case SCAN: {
//		u8 i = 0;
//		u8 scancount = 0;
//		struct bss_info *bss = NULL;
//		bss = STAILQ_FIRST(pscaninfo->pbss);
//
//		if (bss == NULL) {
//			os_free(pscaninfo);
//			pscaninfo = NULL;
//			os_sprintf(pbuf, "{\n\"successful\": false,\n\"data\": null\n}");
//		} else {
//			do {
//				if (pscaninfo->page_sn == pscaninfo->pagenum) {
//					pscaninfo->page_sn = 0;
//					os_sprintf(pbuf,
//							"{\n\"successful\": false,\n\"meessage\": \"repeated page\"\n}");
//					break;
//				}
//
//				scancount = scannum - (pscaninfo->pagenum - 1) * 8;
//
//				if (scancount >= 8) {
//					pscaninfo->data_cnt += 8;
//					pscaninfo->page_sn = pscaninfo->pagenum;
//
//					if (pscaninfo->data_cnt > scannum) {
//						pscaninfo->data_cnt -= 8;
//						os_sprintf(pbuf,
//								"{\n\"successful\": false,\n\"meessage\": \"error page\"\n}");
//						break;
//					}
//
//					json_ws_send((struct jsontree_value *) &scan_tree, "scan",
//							pbuf);
//				} else {
//					pscaninfo->data_cnt += scancount;
//					pscaninfo->page_sn = pscaninfo->pagenum;
//
//					if (pscaninfo->data_cnt > scannum) {
//						pscaninfo->data_cnt -= scancount;
//						os_sprintf(pbuf,
//								"{\n\"successful\": false,\n\"meessage\": \"error page\"\n}");
//						break;
//					}
//
//					char *ptrscanbuf = (char *) os_zalloc(jsonSize);
//					char *pscanbuf = ptrscanbuf;
//					os_sprintf(pscanbuf, ",\n\"ScanResult\": [\n");
//					pscanbuf += os_strlen(pscanbuf);
//
//					for (i = 0; i < scancount; i++) {
//						JSONTREE_OBJECT(page_tree,
//								JSONTREE_PAIR("page", &scaninfo_tree));
//						json_ws_send((struct jsontree_value *) &page_tree,
//								"page", pscanbuf);
//						os_sprintf(pscanbuf + os_strlen(pscanbuf), ",\n");
//						pscanbuf += os_strlen(pscanbuf);
//					}
//
//					os_sprintf(pscanbuf - 2, "]\n");
//					JSONTREE_OBJECT(scantree,
//							JSONTREE_PAIR("TotalPage", &scan_callback),
//							JSONTREE_PAIR("PageNum", &scan_callback));
//					JSONTREE_OBJECT(scanres_tree,
//							JSONTREE_PAIR("Response", &scantree));
//					JSONTREE_OBJECT(scan_tree,
//							JSONTREE_PAIR("scan", &scanres_tree));
//					json_ws_send((struct jsontree_value *) &scan_tree, "scan",
//							pbuf);
//					os_memcpy(pbuf + os_strlen(pbuf) - 4, ptrscanbuf,
//							os_strlen(ptrscanbuf));
//					os_sprintf(pbuf + os_strlen(pbuf), "}\n}");
//					os_free(ptrscanbuf);
//				}
//			} while (0);
//		}
//
//		break;
//	}

	default:
		break;
	}

	data_send(ptrespconn, true, pbuf);
	os_free(pbuf);
	pbuf = NULL;
}

LOCAL void ICACHE_FLASH_ATTR
response_send(void *arg, bool responseOK) {
	struct espconn *ptrespconn = arg;

	data_send(ptrespconn, responseOK, NULL);
}

LOCAL void ICACHE_FLASH_ATTR
parse_url(char *precv, URL_Frame *purl_frame) {
	char *str = NULL;
	uint8 length = 0;
	char *pbuffer = NULL;
	char *pbufer = NULL;

	if (purl_frame == NULL || precv == NULL) {
		return;
	}

	pbuffer = (char *) os_strstr(precv, "Host:");

	if (pbuffer != NULL) {
		length = pbuffer - precv;
		pbufer = (char *) os_zalloc(length + 1);
		pbuffer = pbufer;
		os_memcpy(pbuffer, precv, length);
		os_memset(purl_frame->pSelect, 0, URLSize);
		os_memset(purl_frame->pCommand, 0, URLSize);
		os_memset(purl_frame->pFilename, 0, URLSize);

		if (os_strncmp(pbuffer, "GET ", 4) == 0) {
			purl_frame->Type = GET;
			pbuffer += 4;
		} else if (os_strncmp(pbuffer, "POST ", 5) == 0) {
			purl_frame->Type = POST;
			pbuffer += 5;
		}

		pbuffer++;
		str = (char *) os_strstr(pbuffer, "?");

		if (str != NULL) {
			length = str - pbuffer;
			os_memcpy(purl_frame->pSelect, pbuffer, length);
			str++;
			pbuffer = (char *) os_strstr(str, "=");

			if (pbuffer != NULL) {
				length = pbuffer - str;
				os_memcpy(purl_frame->pCommand, str, length);
				pbuffer++;
				str = (char *) os_strstr(pbuffer, "&");

				if (str != NULL) {
					length = str - pbuffer;
					os_memcpy(purl_frame->pFilename, pbuffer, length);
				} else {
					str = (char *) os_strstr(pbuffer, " HTTP");

					if (str != NULL) {
						length = str - pbuffer;
						os_memcpy(purl_frame->pFilename, pbuffer, length);
					}
				}
			}
		}

		os_free(pbufer);
	} else {
		return;
	}
}

LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;
LOCAL bool save_data(char *precv, uint16 length) {
	bool flag = false;
	char length_buf[10] = { 0 };
	char *ptemp = NULL;
	char *pdata = NULL;
	uint16 headlength = 0;
	static uint32 totallength = 0;

	ptemp = (char *) os_strstr(precv, "\r\n\r\n");

	if (ptemp != NULL) {
		length -= ptemp - precv;
		length -= 4;
		totallength += length;
		headlength = ptemp - precv + 4;
		pdata = (char *) os_strstr(precv, "Content-Length: ");

		if (pdata != NULL) {
			pdata += 16;
			precvbuffer = (char *) os_strstr(pdata, "\r\n");

			if (precvbuffer != NULL) {
				os_memcpy(length_buf, pdata, precvbuffer - pdata);
				dat_sumlength = atoi(length_buf);
			}
		} else {
			if (totallength != 0x00) {
				totallength = 0;
				dat_sumlength = 0;
				return false;
			}
		}
		if ((dat_sumlength + headlength) >= 1024) {
			precvbuffer = (char *) os_zalloc(headlength + 1);
			os_memcpy(precvbuffer, precv, headlength + 1);
		} else {
			precvbuffer = (char *) os_zalloc(dat_sumlength + headlength + 1);
			os_memcpy(precvbuffer, precv, os_strlen(precv));
		}
	} else {
		if (precvbuffer != NULL) {
			totallength += length;
			os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
		} else {
			totallength = 0;
			dat_sumlength = 0;
			return false;
		}
	}

	if (totallength == dat_sumlength) {
		totallength = 0;
		dat_sumlength = 0;
		return true;
	} else {
		return false;
	}
}

LOCAL void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length) {
	URL_Frame *pURL_Frame = NULL;
	char *pParseBuffer = NULL;
	bool parse_flag = false;
	struct espconn *ptrespconn = arg;

	parse_flag = save_data(pusrdata, length);
	if (parse_flag == false) {
		response_send(ptrespconn, false);
	}

//        os_printf(precvbuffer);
	pURL_Frame = (URL_Frame *) os_zalloc(sizeof(URL_Frame));
	parse_url(precvbuffer, pURL_Frame);

	switch (pURL_Frame->Type) {
	case GET:
		os_printf("We have a GET request.\n");

		if (os_strcmp(pURL_Frame->pSelect, "client") == 0 &&
		os_strcmp(pURL_Frame->pCommand, "command") == 0) {
			if (os_strcmp(pURL_Frame->pFilename, "info") == 0) {
				json_send(ptrespconn, INFOMATION);
			}

			if (os_strcmp(pURL_Frame->pFilename, "getdata") == 0) {
				json_send(ptrespconn, TRANSFER_DATA);
			}

			if (os_strcmp(pURL_Frame->pFilename, "status") == 0) {
				json_send(ptrespconn, CONNECT_STATUS);
//				} else if (os_strcmp(pURL_Frame->pFilename, "scan") == 0) {
//					char *strstr = NULL;
//					strstr = (char *) os_strstr(pusrdata, "&");
//
//					if (strstr == NULL) {
//						if (pscaninfo == NULL) {
//							pscaninfo = (scaninfo *) os_zalloc(
//									sizeof(scaninfo));
//						}
//
//						pscaninfo->pespconn = ptrespconn;
//						pscaninfo->pagenum = 0;
//						pscaninfo->page_sn = 0;
//						pscaninfo->data_cnt = 0;
//						wifi_station_scan(NULL, json_scan_cb);
//					} else {
//						strstr++;
//
//						if (os_strncmp(strstr, "page", 4) == 0) {
//							if (pscaninfo != NULL) {
//								pscaninfo->pagenum = *(strstr + 5);
//								pscaninfo->pagenum -= 0x30;
//
//								if (pscaninfo->pagenum > pscaninfo->totalpage
//										|| pscaninfo->pagenum == 0) {
//									response_send(ptrespconn, false);
//								} else {
//									json_send(ptrespconn, SCAN);
//								}
//							} else {
//								response_send(ptrespconn, false);
//							}
//						} else {
//							response_send(ptrespconn, false);
//						}
//					}
			} else {
				response_send(ptrespconn, false);
			}
//			} else if (os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
//			os_strcmp(pURL_Frame->pCommand, "command") == 0) {
//				if (os_strcmp(pURL_Frame->pFilename, "wifi") == 0) {
//					ap_conf = (struct softap_config *) os_zalloc(
//							sizeof(struct softap_config));
//					sta_conf = (struct station_config *) os_zalloc(
//							sizeof(struct station_config));
//					json_send(ptrespconn, WIFI);
//					os_free(sta_conf);
//					os_free(ap_conf);
//					sta_conf = NULL;
//					ap_conf = NULL;
//				}
//
//#if PLUG_DEVICE
//				else if (os_strcmp(pURL_Frame->pFilename, "switch") == 0) {
//					json_send(ptrespconn, SWITCH_STATUS);
//				}
//
//#endif
//
//#if LIGHT_DEVICE
//				else if (os_strcmp(pURL_Frame->pFilename, "light") == 0) {
//					json_send(ptrespconn, LIGHT_STATUS);
//				}
//
//#endif
//
//				else if (os_strcmp(pURL_Frame->pFilename, "reboot") == 0) {
//					json_send(ptrespconn, REBOOT);
//				} else {
//					response_send(ptrespconn, false);
//				}
//			} else if (os_strcmp(pURL_Frame->pSelect, "upgrade") == 0 &&
//			os_strcmp(pURL_Frame->pCommand, "command") == 0) {
//				if (os_strcmp(pURL_Frame->pFilename, "getuser") == 0) {
//					json_send(ptrespconn, USER_BIN);
//				}
		} else {
			response_send(ptrespconn, false);
		}

		break;

//		case POST:
//			os_printf("We have a POST request.\n");
//			pParseBuffer = (char *) os_strstr(precvbuffer, "\r\n\r\n");
//
//			if (pParseBuffer == NULL) {
//				break;
//			}
//
//			pParseBuffer += 4;
//
//			if (os_strcmp(pURL_Frame->pSelect, "config") == 0 &&
//			os_strcmp(pURL_Frame->pCommand, "command") == 0) {
//#if SENSOR_DEVICE
//
//				if (os_strcmp(pURL_Frame->pFilename, "sleep") == 0) {
//#else
//
//				if (os_strcmp(pURL_Frame->pFilename, "reboot") == 0) {
//#endif
//
//					if (pParseBuffer != NULL) {
//						if (restart_10ms != NULL) {
//							os_timer_disarm(restart_10ms);
//						}
//
//						if (rstparm == NULL) {
//							rstparm = (rst_parm *) os_zalloc(sizeof(rst_parm));
//						}
//
//						rstparm->pespconn = ptrespconn;
//#if SENSOR_DEVICE
//						rstparm->parmtype = DEEP_SLEEP;
//#else
//						rstparm->parmtype = REBOOT;
//#endif
//
//						if (restart_10ms == NULL) {
//							restart_10ms = (os_timer_t *) os_malloc(
//									sizeof(os_timer_t));
//						}
//
//						os_timer_setfn(restart_10ms,
//								(os_timer_func_t *) restart_10ms_cb, NULL);
//						os_timer_arm(restart_10ms, 10, 0); // delay 10ms, then do
//
//						response_send(ptrespconn, true);
//					} else {
//						response_send(ptrespconn, false);
//					}
//				} else if (os_strcmp(pURL_Frame->pFilename, "wifi") == 0) {
//					if (pParseBuffer != NULL) {
//						struct jsontree_context js;
//						user_esp_platform_set_connect_status(DEVICE_CONNECTING);
//
//						if (restart_10ms != NULL) {
//							os_timer_disarm(restart_10ms);
//						}
//
//						if (ap_conf == NULL) {
//							ap_conf = (struct softap_config *) os_zalloc(
//									sizeof(struct softap_config));
//						}
//
//						if (sta_conf == NULL) {
//							sta_conf = (struct station_config *) os_zalloc(
//									sizeof(struct station_config));
//						}
//
//						jsontree_setup(&js,
//								(struct jsontree_value *) &wifi_req_tree,
//								json_putchar);
//						json_parse(&js, pParseBuffer);
//
//						if (rstparm == NULL) {
//							rstparm = (rst_parm *) os_zalloc(sizeof(rst_parm));
//						}
//
//						rstparm->pespconn = ptrespconn;
//						rstparm->parmtype = WIFI;
//
//						if (sta_conf->ssid[0] != 0x00
//								|| ap_conf->ssid[0] != 0x00) {
//							ap_conf->ssid_hidden = 0;
//							ap_conf->max_connection = 4;
//
//							if (restart_10ms == NULL) {
//								restart_10ms = (os_timer_t *) os_malloc(
//										sizeof(os_timer_t));
//							}
//
//							os_timer_disarm(restart_10ms);
//							os_timer_setfn(restart_10ms,
//									(os_timer_func_t *) restart_10ms_cb, NULL);
//							os_timer_arm(restart_10ms, 10, 0); // delay 10ms, then do
//						} else {
//							os_free(ap_conf);
//							os_free(sta_conf);
//							os_free(rstparm);
//							sta_conf = NULL;
//							ap_conf = NULL;
//							rstparm = NULL;
//						}
//
//						response_send(ptrespconn, true);
//					} else {
//						response_send(ptrespconn, false);
//					}
//				}
//
//#if PLUG_DEVICE
//				else if (os_strcmp(pURL_Frame->pFilename, "switch") == 0) {
//					if (pParseBuffer != NULL) {
//						struct jsontree_context js;
//						jsontree_setup(&js, (struct jsontree_value *)&StatusTree, json_putchar);
//						json_parse(&js, pParseBuffer);
//						response_send(ptrespconn, true);
//					} else {
//						response_send(ptrespconn, false);
//					}
//				}
//
//#endif
//
//#if LIGHT_DEVICE
//				else if (os_strcmp(pURL_Frame->pFilename, "light") == 0) {
//					if (pParseBuffer != NULL) {
//						struct jsontree_context js;
//
//						jsontree_setup(&js, (struct jsontree_value *)&PwmTree, json_putchar);
//						json_parse(&js, pParseBuffer);
//						response_send(ptrespconn, true);
//					} else {
//						response_send(ptrespconn, false);
//					}
//				}
//
//#endif
//				else {
//					response_send(ptrespconn, false);
//				}
//			} else if (os_strcmp(pURL_Frame->pSelect, "upgrade") == 0 &&
//			os_strcmp(pURL_Frame->pCommand, "command") == 0) {
//				if (os_strcmp(pURL_Frame->pFilename, "start") == 0) {
//					response_send(ptrespconn, true);
//					os_printf("local upgrade start\n");
//					upgrade_lock = 1;
//					system_upgrade_init();
//					system_upgrade_flag_set(UPGRADE_FLAG_START);
//					os_timer_disarm(&upgrade_check_timer);
//					os_timer_setfn(&upgrade_check_timer,
//							(os_timer_func_t *) upgrade_check_func, NULL);
//					os_timer_arm(&upgrade_check_timer, 120000, 0);
//				} else if (os_strcmp(pURL_Frame->pFilename, "reset") == 0) {
//
//					response_send(ptrespconn, true);
//					os_printf("local upgrade restart\n");
//					system_upgrade_reboot();
//				} else {
//					response_send(ptrespconn, false);
//				}
//			} else {
//				response_send(ptrespconn, false);
//			}
//			break;
//		}
//
//		if (precvbuffer != NULL) {
//			os_free(precvbuffer);
//			precvbuffer = NULL;
	}
	os_free(pURL_Frame);
	pURL_Frame = NULL;
}

LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg) {
	struct espconn *pesp_conn = arg;

	espconn_regist_recvcb(pesp_conn, webserver_recv);
//	espconn_regist_reconcb(pesp_conn, webserver_recon);
//	espconn_regist_disconcb(pesp_conn, webserver_discon);
}

void ICACHE_FLASH_ATTR
user_webserver_init() {
	LOCAL struct espconn esp_conn;
	LOCAL esp_tcp esptcp;

	esp_conn.type = ESPCONN_TCP;
	esp_conn.state = ESPCONN_NONE;
	esp_conn.proto.tcp = &esptcp;
	esp_conn.proto.tcp->local_port = SERVER_PORT;
	espconn_regist_connectcb(&esp_conn, webserver_listen);

#ifdef SERVER_SSL_ENABLE
	espconn_secure_accept(&esp_conn);
#else
	espconn_accept(&esp_conn);
#endif
}
