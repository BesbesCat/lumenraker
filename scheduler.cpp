#include "config.h"
#include <Arduino.h>

void ledTask(void*);
void netTask(void*);

void schedulerStart(){
  xTaskCreatePinnedToCore(ledTask, "LED_CORE", 16000, NULL, 3, NULL, 1);
  xTaskCreatePinnedToCore(netTask, "NET_CORE", 16000, NULL, 3, NULL, 0);
}