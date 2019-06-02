#include <Arduino.h>

#include "esp32-webserver.hpp"

fk::Graph<uint16_t, 2, 10000> adc_log({"X", "Y"});

void setup(void)
{
    analogReadResolution(10);

    fk::startServer("******", "******");
    fk::addGraph(adc_log);
}

void loop(void)
{
    uint16_t x = analogRead(35);
    uint16_t y = analogRead(34);
    adc_log.push({x,y});
    delay(1);
}
