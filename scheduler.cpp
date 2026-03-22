#include "config.h"
#include <Arduino.h>

void ledTask(void*);
void netTask(void*);

void schedulerStart(){
  xTaskCreatePinnedToCore(ledTask, "LED_CORE", 32000, NULL, 1, NULL, 1);
  xTaskCreatePinnedToCore(netTask, "NET_CORE", 16000, NULL, 1, NULL, 0);
}