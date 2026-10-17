// source/libraries/ui/widgets/ui_number_input.c
// Number input step logic for the Apex ui library
// https://github.com/is-nobody/apex-lang
// MIT license

#include "ui_widgets.h"
#include "ui_core.h"
#include "ui_text.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// steps a number_input by the given amount
void ui_number_step(UiContext* ctx, Widget* w, double step) {
    char buf[64];
    int len = w->input_value.len < 63 ? w->input_value.len : 63;
    if (len > 0 && w->input_value.ptr) memcpy(buf, w->input_value.ptr, (size_t)len);
    buf[len] = 0;
    
    // parse current value
    double val = atof(buf);
    
    // apply step
    val += step;
    
    // clamp to bounds
    if (w->num_min > w->num_max) { double t = w->num_min; w->num_min = w->num_max; w->num_max = t; }
    if (w->num_min != 0 || w->num_max != 0) {
        if (val < w->num_min) val = w->num_min;
        if (val > w->num_max) val = w->num_max;
    }

    // %g automatically strips trailing zeros and unnecessary decimals
    snprintf(buf, sizeof(buf), "%g", val);
    
    int new_len = (int)strlen(buf);
    
    // hand off a heap copy to the state
    char* new_buf = (char*)malloc((size_t)new_len + 1);
    memcpy(new_buf, buf, (size_t)new_len + 1);
    
    input_set_buffer(ctx, w, new_buf, new_len);
    
    // Fetch state AFTER input_set_buffer, because the array may have reallocated
    WidgetState* s = ui_state_get(ctx, w->id);
    s->cursor = new_len;
    s->sel_anchor = new_len;
    
    // emit change event
    UiEvent ev; memset(&ev, 0, sizeof(ev));
    ev.kind = EV_CHANGE;
    ev.widget_id = w->id;
    strncpy(ev.widget_type, "number_input", 15);
    if (w->key_str) strncpy(ev.key_name, w->key_str, 63);
    ev.num_value = val;
    
    ui_event_push(ctx, &ev);
    ctx->redraw_needed = true;
}

// checks if the given string is a valid number and within bounds
bool ui_number_is_valid_text(const char* s, int len, double min, double max) {
    if (len <= 0) return true; // empty or invalid is valid while typing
    bool has_digit = false;
    bool has_dot = false;
    
    for (int i = 0; i < len; i++) {
        char c = s[i];
        if (c >= '0' && c <= '9') {
            has_digit = true;
            continue;
        }
        
        if (c == '-') {
            if (i == 0) continue; // only allow a leading minus sign
            return false;
        }
        
        if (c == '.') {
            if (has_dot) return false; // only one dot allowed
            has_dot = true;
            continue;
        }
        
        return false; // reject any other characters
    }
    
    // only enforce min/max bounds if the user has typed at least one actual digit
    if (has_digit) {
        char tmp[64];
        int n = len < 63 ? len : 63;
        memcpy(tmp, s, n);
        tmp[n] = 0;
        
        double v = atof(tmp);
        if (v < min || v > max) return false;
    }
    
    return true;
}