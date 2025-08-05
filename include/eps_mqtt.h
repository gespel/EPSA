#ifndef _EPS_MQTT_H
#define _EPS_MQTT_H

#include <stdint.h>

typedef struct {
  char* host;
  uint16_t port;
  char* topic;
} eps_mqtt_config_t;

eps_mqtt_config_t* get_mqtt_plugin_config(uint32_t jobid);
void free_mqtt_plugin_config(eps_mqtt_config_t* config);

#endif
