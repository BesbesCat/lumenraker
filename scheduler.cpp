#include <Arduino.h>
#include "config.h"

void ledTask(void*);
void netTask(void*);
void mqttTask(void*);

void schedulerStart(){
  xTaskCreatePinnedToCore(ledTask, "LED_TASK", 16000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(netTask, "NET_TASK", 16000, NULL, 1, NULL, 0);
  xTaskCreatePinnedToCore(mqttTask, "MQTT_TASK", 4096, NULL, 1, NULL, 0);
}