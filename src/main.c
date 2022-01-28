/*
 * Copyright (c) 2021 Golioth, Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <logging/log.h>
LOG_MODULE_REGISTER(golioth_lightdb_stream, LOG_LEVEL_DBG);

#include <drivers/sensor.h>
#include <net/coap.h>
#include <net/golioth/system_client.h>
#include <net/golioth/wifi.h>
#include <qcbor/qcbor.h>
#include <stdlib.h>
#include <modem/at_cmd.h>

#define CONN_STAT_BUFFER_SIZE 64

static struct golioth_client *client = GOLIOTH_SYSTEM_CLIENT_GET();

static int get_sensor_value(const struct device *dev, struct sensor_value *val,
                            const enum sensor_channel channel) {
  int err;

  err = sensor_sample_fetch(dev);
  if (err) {
    LOG_ERR("Failed to fetch sensor data: %d", err);
    return err;
  }

  err = sensor_channel_get(dev, channel, val);
  if (err) {
    LOG_ERR("Failed to get sensor data: %d", err);
    return err;
  }
  return 0;
}

void main(void) {
  char buf[256];
  size_t cbor_size = 0;
  UsefulBuf cbor_buffer = {.ptr = buf, .len = 256};

#if DT_NODE_HAS_STATUS(DT_ALIAS(temp0), okay)
  struct sensor_value temp;
  const struct device *temp_dev = DEVICE_DT_GET(DT_ALIAS(temp0));
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(hum0), okay)
  struct sensor_value humidity;
  const struct device *hum_dev = DEVICE_DT_GET(DT_ALIAS(hum0));
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(press0), okay)
  struct sensor_value pressure;
  const struct device *press_dev = DEVICE_DT_GET(DT_ALIAS(press0));
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(gas0), okay)
  struct sensor_value gas;
  const struct device *gas_dev = DEVICE_DT_GET(DT_ALIAS(gas0));
#endif
#if DT_NODE_HAS_STATUS(DT_ALIAS(accel0), okay)
  struct sensor_value accel[3];
  const struct device *accel_dev = DEVICE_DT_GET(DT_ALIAS(accel0));
#endif
  int err;

  LOG_DBG("Start Light DB stream sample");

  if (IS_ENABLED(CONFIG_GOLIOTH_SAMPLE_WIFI)) {
    LOG_INF("Connecting to WiFi");
    wifi_connect();
  }

  golioth_system_client_start();

  int ret = 0;
	enum at_cmd_state at_state;
  static uint8_t connStatBuffer[CONN_STAT_BUFFER_SIZE];
	const char* cmd = "AT%XCONNSTAT?";
  
  LOG_DBG("Enable nrf9160 connection statistics gathering");

	at_cmd_write("AT%XCONNSTAT=1", NULL, 0, NULL);
	if (err != 0) {
		printk("Could not enable connection statistics, error: %d\n", err);
	}

  while (true) {
    QCBOREncodeContext ec;
    bool hasData = false;
    QCBOREncode_Init(&ec, cbor_buffer);
    QCBOREncode_OpenMap(&ec);

#if DT_NODE_HAS_STATUS(DT_ALIAS(temp0), okay)
    err = get_sensor_value(temp_dev, &temp, SENSOR_CHAN_AMBIENT_TEMP);
    if (!err) {
      QCBOREncode_AddDoubleToMap(&ec, "temp", sensor_value_to_double(&temp));
      hasData = true;
    }
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(hum0), okay)
    err = get_sensor_value(hum_dev, &humidity, SENSOR_CHAN_HUMIDITY);
    if (!err) {
      QCBOREncode_AddDoubleToMap(&ec, "humidity",
                                 sensor_value_to_double(&humidity));
      hasData = true;
    }
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(press0), okay)
    err = get_sensor_value(press_dev, &pressure, SENSOR_CHAN_PRESS);
    if (!err) {
      QCBOREncode_AddDoubleToMap(&ec, "pressure",
                                 sensor_value_to_double(&pressure) * 1000);
      hasData = true;
    }
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(gas0), okay)
    err = get_sensor_value(gas_dev, &gas, SENSOR_CHAN_GAS_RES);
    if (!err) {
      QCBOREncode_AddDoubleToMap(&ec, "gas", sensor_value_to_double(&gas));
      hasData = true;
    }
#endif

#if DT_NODE_HAS_STATUS(DT_ALIAS(accel0), okay)
    err = get_sensor_value(accel_dev, &accel, SENSOR_CHAN_ACCEL_XYZ);
    if (!err) {
      QCBOREncode_AddDoubleToMap(&ec, "accel_x",
                                 sensor_value_to_double(&accel[0]));
      QCBOREncode_AddDoubleToMap(&ec, "accel_y",
                                 sensor_value_to_double(&accel[1]));
      QCBOREncode_AddDoubleToMap(&ec, "accel_z",
                                 sensor_value_to_double(&accel[2]));
      hasData = true;
    }
#endif

    QCBOREncode_CloseMap(&ec);
    QCBORError qcerr = QCBOREncode_FinishGetSize(&ec, &cbor_size);

    if (qcerr) {
      LOG_WRN("Failed to encode cbor: %d", err);
      k_sleep(K_SECONDS(1));
      continue;
    }

    if (!hasData) {
      LOG_WRN("No data collected");
      k_sleep(K_SECONDS(1));
      continue;
    }

    err = golioth_lightdb_set(client, GOLIOTH_LIGHTDB_STREAM_PATH("sensor"),
                              COAP_CONTENT_FORMAT_APP_CBOR, buf, cbor_size);
    if (err) {
      LOG_WRN("Failed to send data: %d", err);
    }


    QCBOREncode_Init(&ec, cbor_buffer);
    QCBOREncode_OpenMap(&ec);

    ret = at_cmd_write(cmd, connStatBuffer, CONN_STAT_BUFFER_SIZE, &at_state);
    if (ret) {
      printk("at_cmd_write [%s] error:%d, at_state: %d\n",
        cmd, ret, at_state);
      strncpy(connStatBuffer, "error", CONN_STAT_BUFFER_SIZE);
    }
  
    char * token = strtok(connStatBuffer, " ");

    token = strtok(NULL, ",");
    QCBOREncode_AddSZStringToMap(&ec, "SMS_Tx", token);
    token = strtok(NULL, ",");
    QCBOREncode_AddSZStringToMap(&ec, "SMS_Rx", token);
    token = strtok(NULL, ",");
    QCBOREncode_AddSZStringToMap(&ec, "Data_Tx(kB)", token);
    token = strtok(NULL, ",");
    QCBOREncode_AddSZStringToMap(&ec, "Data_Rx(kB)", token);
    token = strtok(NULL, ",");
    QCBOREncode_AddSZStringToMap(&ec, "Packet_Max_Size(B)", token);
    token = strtok(NULL, ",");
    QCBOREncode_AddSZStringToMap(&ec, "Packet_Min_Size(B)", token);

    LOG_DBG("Connection stats: %s | Uptime: %d seconds\n", connStatBuffer, k_uptime_get_32() / 1000);

    QCBOREncode_CloseMap(&ec);
    qcerr = QCBOREncode_FinishGetSize(&ec, &cbor_size);

    err = golioth_lightdb_set(client, GOLIOTH_LIGHTDB_STREAM_PATH("statistics"),
                              COAP_CONTENT_FORMAT_APP_CBOR, buf, cbor_size);
    if (err) {
      LOG_WRN("Failed to send data: %d", err);
    }

    k_sleep(K_SECONDS(60));
  }
}
