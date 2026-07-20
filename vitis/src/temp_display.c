/**
 * temp_display.c
 * Integration layer — formats temperature data and renders it to the OLED
 * Depends on: oled.h, adt7420.h
 */

#include "temp_display.h"
#include <stdio.h>

void oled_display_temp(oledControl *oled, float t) {
    char line_c[OLED_LINE_LEN + 1];
    char line_f[OLED_LINE_LEN + 1];

    if (t <= ADT7420_SENTINEL_THRESHOLD) {
        oled_print_line(oled, "=== TEMP SENSOR=");
        oled_print_line(oled, "                ");
        oled_print_line(oled, "C:  ERROR       ");
        oled_print_line(oled, "F:  CHECK WIRING");
        return;
    }

    TempParts c = ADT7420_DecomposeTemp(t);
    TempParts f = ADT7420_DecomposeTemp(celsius_to_fahrenheit(t));

    snprintf(line_c, sizeof(line_c), "C: %s%d.%02dC",
             (c.sign < 0 ? "-" : "+"), c.whole, c.frac);
    snprintf(line_f, sizeof(line_f), "F: %s%d.%02dF",
             (f.sign < 0 ? "-" : "+"), f.whole, f.frac);

    oled_print_line(oled, "=== TEMP SENSOR=");
    oled_print_line(oled, "                ");
    oled_print_line(oled, line_c);
    oled_print_line(oled, line_f);
}


void oled_display_temp_parts(oledControl *oled, const TempParts *c, const TempParts *f){
    char line_c[OLED_LINE_LEN + 1];
    char line_f[OLED_LINE_LEN + 1];

    snprintf(line_c, sizeof(line_c), "C: %s%d.%02dC",
             (c->sign < 0 ? "-" : "+"), c->whole, c->frac);
    snprintf(line_f, sizeof(line_f), "F: %s%d.%02dF",
             (f->sign < 0 ? "-" : "+"), f->whole, f->frac);

    oled_print_line(oled, "=== TEMP SENSOR=");
    oled_print_line(oled, "                ");
    oled_print_line(oled, line_c);
    oled_print_line(oled, line_f);
    
}