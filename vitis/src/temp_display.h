/**
 * temp_display.h
 * Integration layer between the ADT7420 temperature sensor and the OLED display
 * Owns all logic that involves both drivers together
 */

#ifndef TEMP_DISPLAY_H
#define TEMP_DISPLAY_H

#include "oled.h"
#include "adt7420.h"

// Format and write a temperature reading to all 4 OLED pages
// Page 0: header, Page 1: blank, Page 2: Celsius, Page 3: Fahrenheit
// Displays an error state if t <= ADT7420_SENTINEL_THRESHOLD
void oled_display_temp(oledControl *oled, float t);

void oled_display_temp_parts(oledControl *oled, const TempParts *c, const TempParts *f);

#endif /* TEMP_DISPLAY_H */
