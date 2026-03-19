
#include "config.h"

void schedulerStart();
void loadConfig();
void ledsInit();
void setupFS();

void setup(){
  Serial.begin(115200);
  loadConfig();
  setupFS();
  ledsInit();
  schedulerStart();
}

void loop(){}
