/**
 * temp_display.c
 * Integration layer — formats temperature data and renders it to the OLED
 * Depends on: oled.h, adt7420.h
 */

#include "temp_display.h"
#include <stdio.h>

void OLED_DisplayTemp(OLED_Control_t *oled, float temp) {
    char line_c[OLED_LINE_LEN + 1];
    char line_f[OLED_LINE_LEN + 1];

    if (temp <= ADT7420_SENTINEL_THRESHOLD) {
        OLED_PrintLine(oled, "=== TEMP SENSOR=");
        OLED_PrintLine(oled, "                ");
        OLED_PrintLine(oled, "C:  ERROR       ");
        OLED_PrintLine(oled, "F:  CHECK WIRING");
        return;
    }

    TempParts temp_c = ADT7420_DecomposeTemp(temp);
    TempParts temp_f = ADT7420_DecomposeTemp(celsius_to_fahrenheit(temp));

    snprintf(line_c, sizeof(line_c), "C: %s%d.%02dC",
             (temp_c.sign < 0 ? "-" : "+"), temp_c.whole, temp_c.frac);
    snprintf(line_f, sizeof(line_f), "F: %s%d.%02dF",
             (temp_f.sign < 0 ? "-" : "+"), temp_f.whole, temp_f.frac);

    OLED_PrintLine(oled, "=== TEMP SENSOR=");
    OLED_PrintLine(oled, "                ");
    OLED_PrintLine(oled, line_c);
    OLED_PrintLine(oled, line_f);
}


void OLED_DisplayTempParts(OLED_Control_t *oled, const TempParts *temp_c, const TempParts *temp_f){
    char line_c[OLED_LINE_LEN + 1];
    char line_f[OLED_LINE_LEN + 1];

    snprintf(line_c, sizeof(line_c), "C: %s%d.%02dC",
             (temp_c->sign < 0 ? "-" : "+"), temp_c->whole, temp_c->frac);
    snprintf(line_f, sizeof(line_f), "F: %s%d.%02dF",
             (temp_f->sign < 0 ? "-" : "+"), temp_f->whole, temp_f->frac);

    OLED_PrintLine(oled, "=== TEMP SENSOR=");
    OLED_PrintLine(oled, "                ");
    OLED_PrintLine(oled, line_c);
    OLED_PrintLine(oled, line_f);
    
}