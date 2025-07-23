#include <errno.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <eps_mqtt.h>

#define TOPIC_MAX_LEN 32

eps_mqtt_config_t* get_mqtt_plugin_config(uint32_t jid)
{
  char* host = getenv("EPS_MQTT_HOST");
  if (!host) {
    printf("warning: missing EPS_MQTT_HOST\n");
    return NULL;
  }

  char* port = getenv("EPS_MQTT_PORT");
  if (!port) {
    printf("warning: missing EPS_MQTT_PORT\n");
    return NULL;
  }
  char* endptr = NULL;
  errno = 0;
  unsigned long res = strtoul(port, &endptr, 10);
  if (errno != 0 || *endptr != '\0' || endptr == port || res > UINT16_MAX)
  {
    printf("warning: failed to conver %s to uint16_t\n", port);
    return NULL;
  }

  char topic[TOPIC_MAX_LEN];
  snprintf(topic, TOPIC_MAX_LEN, "%u", jid);

  eps_mqtt_config_t* config = malloc(sizeof(eps_mqtt_config_t));
  config->host = strdup(host);
  config->port = (uint16_t)res;
  config->topic = strdup(topic);

  return config;
}

void free_mqtt_plugin_config(eps_mqtt_config_t* config)
{
  if (!config) return;
  free(config->host);
  free(config->topic);
  free(config);
}
