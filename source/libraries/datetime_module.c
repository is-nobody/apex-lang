// source/libraries/datetime_module.c
// Implementation of Datetime Module for Apex language
// https://github.com/is-nobody/apex-lang
// MIT license

#include "datetime_module.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <sys/timeb.h>
#else
#include <sys/time.h>
#endif

// days in each month (index 1..12); february = 28, leap year handled separately
static const int MONTH_DAYS[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

// short and long english names; strftime-style, no locale support
static const char* const WEEKDAY_SHORT[] = {"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
static const char* const WEEKDAY_LONG[]  = {"Sunday","Monday","Tuesday","Wednesday",
                                            "Thursday","Friday","Saturday"};
static const char* const MONTH_SHORT[]   = {"Jan","Feb","Mar","Apr","May","Jun",
                                            "Jul","Aug","Sep","Oct","Nov","Dec"};
static const char* const MONTH_LONG[]    = {"January","February","March","April",
                                            "May","June","July","August","September",
                                            "October","November","December"};

// dynamic string builder for the format function
typedef struct {
    char* buffer;    // dynamic buffer
    int length;      // used bytes
    int capacity;    // allocated bytes
} StringBuilder;

static void sb_init(StringBuilder* sb) {
    sb->capacity = 64;                                    // modest starting size
    sb->buffer = (char*)malloc(sb->capacity);             // allocate buffer
    if (!sb->buffer) { sb->length = 0; sb->capacity = 0; return; }
    sb->length = 0;                                       // empty
    sb->buffer[0] = '\0';                                 // null terminate
}

static void sb_append(StringBuilder* sb, const char* s, int n) {
    if (!sb->buffer || n <= 0) return;                    // nothing to append
    if (sb->length + n + 1 > sb->capacity) {              // need more space
        int cap = sb->capacity > 0 ? sb->capacity : 64;   // start from current capacity
        while (cap < sb->length + n + 1) cap *= 2;        // double until it fits
        char* nb = (char*)realloc(sb->buffer, cap);       // resize buffer
        if (!nb) return;                                  // realloc failed, keep old
        sb->buffer = nb;                                  // install new buffer
        sb->capacity = cap;                               // update capacity
    }
    memcpy(sb->buffer + sb->length, s, n);                // copy bytes
    sb->length += n;                                      // update length
    sb->buffer[sb->length] = '\0';                        // null terminate
}

static void sb_free(StringBuilder* sb) {
    if (sb->buffer) free(sb->buffer);                     // release buffer
    sb->buffer = NULL;                                    // clear pointer
    sb->length = 0;                                       // reset length
    sb->capacity = 0;                                     // reset capacity
}

// helper to create an interned string value (VM-thread only)
static Value make_string_val(VM* vm, const char* str) {
    int len = (int)strlen(str);                                         // compute string length
    return MAKE_STRING(string_intern(&vm->intern_table, str, len));     // intern and box as value
}

// proleptic gregorian day number; days since 1970-01-01.
// howard hinnant's algorithm; chosen over mktime/timegm because timegm is
// non-portable and mktime applies an unwanted local-time shift
static long long days_from_civil(long long y, int m, int d) {
    y -= m <= 2;                                            // shift jan/feb into prior year
    long long era = (y >= 0 ? y : y - 399) / 400;           // 400-year era
    int yoe = (int)(y - era * 400);                         // year of era
    int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;  // day of march-based year
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;        // day of era
    return era * 146097 + (long long)doe - 719468;          // days since epoch
}

// inverse of days_from_civil
static void civil_from_days(long long z, long long* y_out, int* m_out, int* d_out) {
    z += 719468;                                                    // shift to era origin
    long long era = (z >= 0 ? z : z - 146096) / 146097;             // 400-year era
    int doe = (int)(z - era * 146097);                              // day of era
    int yoe = (doe - doe/1460 + doe/36524 - doe/146096) / 365;      // year of era
    long long y = (long long)yoe + era * 400;                       // raw year
    int doy = doe - (365*yoe + yoe/4 - yoe/100);                    // day of year
    int mp = (5*doy + 2)/153;                                       // march-based month
    int d = doy - (153*mp+2)/5 + 1;                                 // day of month
    int m = mp < 10 ? mp+3 : mp-9;                                  // gregorian month
    *y_out = y + (m <= 2);                                          // unshift jan/feb
    *m_out = m;
    *d_out = d;
}

// leap year check (proleptic gregorian)
static bool is_leap(long long y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}

// days in the given month of the given year
static int days_in_month(long long y, int m) {
    if (m == 2 && is_leap(y)) return 29;                            // february in a leap year
    return MONTH_DAYS[m];                                           // fixed table otherwise
}

// day of year, 1..366
static int day_of_year(long long y, int m, int d) {
    static const int cum[] = {0,31,59,90,120,151,181,212,243,273,304,334};
    int doy = cum[m-1] + d;                                         // days up to month start
    if (m > 2 && is_leap(y)) doy++;                                 // account for feb 29
    return doy;
}

// datetime fields -> seconds since epoch (utc, with fractional milliseconds)
static double dt_to_seconds(long long y, int mo, int d, int h, int mi, int s, int ms) {
    long long days = days_from_civil(y, mo, d);                     // whole days since epoch
    long long secs = days * 86400LL + h * 3600LL + mi * 60LL + s;   // seconds within the day
    return (double)secs + (double)ms / 1000.0;                      // add milliseconds
}

// seconds since epoch -> datetime fields (utc)
static void seconds_to_dt(double secs, long long* y, int* mo, int* d,
                          int* h, int* mi, int* s, int* ms, int* wd) {
    double ipart;
    double frac = modf(secs, &ipart);                               // split integer / fractional part
    long long total = (long long)ipart;
    if (frac < 0) { frac += 1.0; total -= 1; }                      // normalise negative fractions
    int msec = (int)(frac * 1000.0 + 0.5);                          // round to nearest ms
    if (msec >= 1000) { msec -= 1000; total += 1; }                 // carry into seconds

    long long days = total / 86400;                                 // whole days
    long long rem = total % 86400;                                  // seconds within the day
    if (rem < 0) { rem += 86400; days -= 1; }                       // normalise negative remainder

    civil_from_days(days, y, mo, d);                                // date fields
    *h = (int)(rem / 3600);                                         // hour
    rem %= 3600;                                                    // remaining seconds
    *mi = (int)(rem / 60);                                          // minute
    *s = (int)(rem % 60);                                           // second
    *ms = msec;                                                     // millisecond

    // 1970-01-01 was a thursday; sunday = 0
    long long w = (4 + days) % 7;
    if (w < 0) w += 7;
    *wd = (int)w;
}

// build the datetime table from its fields
static Value build_dt_table(VM* vm, long long y, int mo, int d,
                            int h, int mi, int s, int ms, int wd) {
    Table* t = table_create(16);                                                    // result table
    Value k;
    k = make_string_val(vm, "year");        table_set(t, k, MAKE_NUMBER((double)y));  value_decref(k);
    k = make_string_val(vm, "month");       table_set(t, k, MAKE_NUMBER((double)mo)); value_decref(k);
    k = make_string_val(vm, "day");         table_set(t, k, MAKE_NUMBER((double)d));  value_decref(k);
    k = make_string_val(vm, "hour");        table_set(t, k, MAKE_NUMBER((double)h));  value_decref(k);
    k = make_string_val(vm, "minute");      table_set(t, k, MAKE_NUMBER((double)mi)); value_decref(k);
    k = make_string_val(vm, "second");      table_set(t, k, MAKE_NUMBER((double)s));  value_decref(k);
    k = make_string_val(vm, "millisecond"); table_set(t, k, MAKE_NUMBER((double)ms)); value_decref(k);
    k = make_string_val(vm, "weekday");     table_set(t, k, MAKE_NUMBER((double)wd)); value_decref(k);
    return MAKE_TABLE(t);                                                           // box table as value
}

// read a numeric field; fall back to default when the key is missing or not a number
static double read_field(VM* vm, Table* t, const char* key, double default_val) {
    Value k = make_string_val(vm, key);                     // interned lookup key
    Value v;
    double result = default_val;                            // default if not found
    if (table_get(t, k, &v)) {                              // key exists
        if (IS_NUMBER(v)) result = AS_NUMBER(v);            // extract numeric value
        value_decref(v);                                    // release value reference
    }
    value_decref(k);                                        // release key reference
    return result;                                          // return resolved value
}

// strict read: key must exist and hold a number
static bool read_field_strict(VM* vm, Table* t, const char* key, double* out) {
    Value k = make_string_val(vm, key);                     // interned lookup key
    Value v;
    bool ok = false;                                        // default: not found
    if (table_get(t, k, &v)) {                              // key exists
        if (IS_NUMBER(v)) { *out = AS_NUMBER(v); ok = true; }  // accept numeric value
        value_decref(v);                                    // release value reference
    }
    value_decref(k);                                        // release key reference
    return ok;                                              // caller decides how to react
}

// read a datetime table into individual fields; false on missing/invalid fields
static bool read_dt(VM* vm, Value v, long long* y, int* mo, int* d,
                    int* h, int* mi, int* s, int* ms) {
    if (!IS_TABLE(v)) return false;                         // must be a table
    Table* t = AS_TABLE(v);                                 // unwrap table

    double vy, vmo, vd;                                     // required numeric fields
    if (!read_field_strict(vm, t, "year",  &vy))  return false;
    if (!read_field_strict(vm, t, "month", &vmo)) return false;
    if (!read_field_strict(vm, t, "day",   &vd))  return false;

    double vh  = read_field(vm, t, "hour",        0);       // optional fields default to 0
    double vmi = read_field(vm, t, "minute",      0);
    double vs  = read_field(vm, t, "second",      0);
    double vms = read_field(vm, t, "millisecond", 0);

    if (vmo < 1 || vmo > 12) return false;                  // month range
    if (vd  < 1 || vd  > 31) return false;                  // day range
    if (vh  < 0 || vh  > 23) return false;                  // hour range
    if (vmi < 0 || vmi > 59) return false;                  // minute range
    if (vs  < 0 || vs  > 60) return false;                  // second range (leap second allowed)
    if (vms < 0 || vms > 999) return false;                 // millisecond range

    *y  = (long long)vy;                                    // commit fields
    *mo = (int)vmo;
    *d  = (int)vd;
    *h  = (int)vh;
    *mi = (int)vmi;
    *s  = (int)vs;
    *ms = (int)vms;
    return true;                                            // all fields valid
}

// read exactly n digits and advance the pointer; false on any non-digit
static bool read_digits(const char** p, int n, int* out) {
    int v = 0;                                              // accumulator
    for (int i = 0; i < n; i++) {                           // exactly n digits required
        char c = **p;                                       // current char
        if (c < '0' || c > '9') return false;               // not a digit
        v = v * 10 + (c - '0');                             // shift in
        (*p)++;                                             // advance pointer
    }
    *out = v;                                               // return parsed value
    return true;                                            // success
}

// parse an iso-8601 datetime string into fields; false on any syntax error
static bool parse_iso(const char* str, long long* y, int* mo, int* d,
                      int* h, int* mi, int* s, int* ms) {
    const char* p = str;                                    // cursor over input
    int yr, mr, dr;
    if (!read_digits(&p, 4, &yr)) return false;             // year
    if (*p++ != '-')             return false;              // separator
    if (!read_digits(&p, 2, &mr)) return false;             // month
    if (*p++ != '-')             return false;              // separator
    if (!read_digits(&p, 2, &dr)) return false;             // day

    if (mr < 1 || mr > 12) return false;                    // month range
    if (dr < 1 || dr > days_in_month(yr, mr)) return false; // day range (leap-aware)

    *h = 0; *mi = 0; *s = 0; *ms = 0;                       // default time part

    if (*p == '\0') { *y = yr; *mo = mr; *d = dr; return true; }  // date only
    if (*p != 'T' && *p != ' ') return false;               // T or space separator
    p++;                                                    // skip separator

    int hr, mir;
    if (!read_digits(&p, 2, &hr)) return false;             // hour
    if (*p++ != ':')              return false;             // colon
    if (!read_digits(&p, 2, &mir)) return false;            // minute
    if (hr > 23 || mir > 59) return false;                  // range check

    int sr = 0, msr = 0;
    if (*p == ':') {                                        // optional seconds
        p++;
        if (!read_digits(&p, 2, &sr)) return false;         // second
        if (sr > 60) return false;                          // range (leap second allowed)
        if (*p == '.') {                                    // optional fractional part
            p++;
            int digits = 0;                                 // digits captured
            int val = 0;                                    // accumulator
            while (digits < 3 && *p >= '0' && *p <= '9') {  // capture up to 3 digits
                val = val * 10 + (*p - '0');
                p++;
                digits++;
            }
            while (*p >= '0' && *p <= '9') p++;             // ignore extra sub-ms digits
            while (digits < 3) { val *= 10; digits++; }     // pad to three digits
            msr = val;                                      // store milliseconds
        }
    }
    if (*p != '\0') return false;                           // trailing garbage

    *y  = yr;  *mo = mr;  *d  = dr;                         // commit fields
    *h  = hr;  *mi = mir; *s  = sr; *ms = msr;
    return true;                                            // success
}

// append a formatted datetime to the builder; strftime-like tokens
static void format_dt(StringBuilder* sb, long long y, int mo, int d,
                      int h, int mi, int s, int ms, int wd, const char* fmt) {
    char buf[32];                                           // scratch buffer for numbers
    const char* p = fmt;                                    // cursor over format
    while (*p) {                                            // iterate tokens
        if (*p != '%') {                                    // literal char
            sb_append(sb, p, 1);
            p++;
            continue;
        }
        p++;                                                // skip percent
        if (!*p) { sb_append(sb, "%", 1); break; }          // lone trailing percent
        char tok = *p++;                                    // token char
        int n;                                              // formatted length
        switch (tok) {
            case 'Y': n = snprintf(buf, sizeof(buf), "%04lld", y); sb_append(sb, buf, n); break;
            case 'm': n = snprintf(buf, sizeof(buf), "%02d", mo);  sb_append(sb, buf, n); break;
            case 'd': n = snprintf(buf, sizeof(buf), "%02d", d);   sb_append(sb, buf, n); break;
            case 'H': n = snprintf(buf, sizeof(buf), "%02d", h);   sb_append(sb, buf, n); break;
            case 'M': n = snprintf(buf, sizeof(buf), "%02d", mi);  sb_append(sb, buf, n); break;
            case 'S': n = snprintf(buf, sizeof(buf), "%02d", s);   sb_append(sb, buf, n); break;
            case 'f': n = snprintf(buf, sizeof(buf), "%03d", ms);  sb_append(sb, buf, n); break;
            case 'j': n = snprintf(buf, sizeof(buf), "%03d", day_of_year(y, mo, d)); sb_append(sb, buf, n); break;
            case 'a': sb_append(sb, WEEKDAY_SHORT[wd], 3); break;                      // short weekday
            case 'A': sb_append(sb, WEEKDAY_LONG[wd],  (int)strlen(WEEKDAY_LONG[wd]));  break;  // long weekday
            case 'b': sb_append(sb, MONTH_SHORT[mo-1], 3); break;                      // short month
            case 'B': sb_append(sb, MONTH_LONG[mo-1],  (int)strlen(MONTH_LONG[mo-1]));  break;  // long month
            case '%': sb_append(sb, "%", 1); break;                                    // literal percent
            default:                                                     // unknown token
                sb_append(sb, "%", 1);                                   // emit "%" + char
                sb_append(sb, &tok, 1);
                break;
        }
    }
}

// dispatcher for datetime module built-in functions
bool datetime_call_builtin(VM* vm, const char* name, int arg_count, Value* args, Value* result) {

    if (strcmp(name, "datetime.now") == 0) {                        // current moment in utc
        long long y; int mo, d, h, mi, s, ms, wd;
#ifdef _WIN32
        SYSTEMTIME st;                                              // windows system time
        GetSystemTime(&st);                                         // fetch utc time
        y = st.wYear; mo = st.wMonth; d = st.wDay;
        h = st.wHour; mi = st.wMinute; s = st.wSecond; ms = st.wMilliseconds;
        wd = st.wDayOfWeek;                                         // 0 = sunday, matches
#else
        struct timeval tv;                                          // posix time value
        gettimeofday(&tv, NULL);                                    // fetch wall clock
        struct tm tm_info;                                          // broken-down time
        gmtime_r(&tv.tv_sec, &tm_info);                             // convert to utc
        y = tm_info.tm_year + 1900;
        mo = tm_info.tm_mon + 1;
        d = tm_info.tm_mday;
        h = tm_info.tm_hour; mi = tm_info.tm_min; s = tm_info.tm_sec;
        ms = (int)(tv.tv_usec / 1000);
        wd = tm_info.tm_wday;                                       // 0 = sunday
#endif
        *result = build_dt_table(vm, y, mo, d, h, mi, s, ms, wd);   // pack into a table
        return true;                                                // builtin handled
    }

    if (strcmp(name, "datetime.local") == 0) {                      // current moment in local time
        long long y; int mo, d, h, mi, s, ms, wd;
#ifdef _WIN32
        SYSTEMTIME st;                                              // windows system time
        GetLocalTime(&st);                                          // fetch local time
        y = st.wYear; mo = st.wMonth; d = st.wDay;
        h = st.wHour; mi = st.wMinute; s = st.wSecond; ms = st.wMilliseconds;
        wd = st.wDayOfWeek;
#else
        struct timeval tv;                                          // posix time value
        gettimeofday(&tv, NULL);                                    // fetch wall clock
        struct tm tm_info;                                          // broken-down time
        localtime_r(&tv.tv_sec, &tm_info);                          // convert to local zone
        y = tm_info.tm_year + 1900;
        mo = tm_info.tm_mon + 1;
        d = tm_info.tm_mday;
        h = tm_info.tm_hour; mi = tm_info.tm_min; s = tm_info.tm_sec;
        ms = (int)(tv.tv_usec / 1000);
        wd = tm_info.tm_wday;
#endif
        *result = build_dt_table(vm, y, mo, d, h, mi, s, ms, wd);   // pack into a table
        return true;                                                // builtin handled
    }

    if (strcmp(name, "datetime.timestamp") == 0) {                  // seconds since epoch
#ifdef _WIN32
        struct _timeb tb;                                           // windows time struct
        _ftime(&tb);                                                // fetch time
        *result = MAKE_NUMBER((double)tb.time + (double)tb.millitm / 1000.0);  // seconds + ms
#else
        struct timeval tv;                                          // posix time value
        gettimeofday(&tv, NULL);                                    // fetch wall clock
        *result = MAKE_NUMBER((double)tv.tv_sec + (double)tv.tv_usec / 1000000.0);  // seconds + us
#endif
        return true;                                                // builtin handled
    }

    if (strcmp(name, "datetime.from_timestamp") == 0) {             // seconds -> utc table
        if (arg_count < 1 || !IS_NUMBER(args[0])) {                 // validate number argument
            *result = MAKE_NONE();
            return true;
        }
        long long y; int mo, d, h, mi, s, ms, wd;
        seconds_to_dt(AS_NUMBER(args[0]), &y, &mo, &d, &h, &mi, &s, &ms, &wd);
        *result = build_dt_table(vm, y, mo, d, h, mi, s, ms, wd);   // pack into a table
        return true;                                                // builtin handled
    }

    if (strcmp(name, "datetime.to_timestamp") == 0) {               // table -> seconds utc
        long long y; int mo, d, h, mi, s, ms;
        if (arg_count < 1 || !read_dt(vm, args[0], &y, &mo, &d, &h, &mi, &s, &ms)) {
            *result = MAKE_NONE();                                  // invalid table
            return true;
        }
        *result = MAKE_NUMBER(dt_to_seconds(y, mo, d, h, mi, s, ms));  // seconds since epoch
        return true;                                                   // builtin handled
    }

    if (strcmp(name, "datetime.parse") == 0) {                      // iso-8601 -> table
        if (arg_count < 1 || !IS_STRING(args[0])) {                 // validate string argument
            *result = MAKE_NONE();
            return true;
        }
        long long y; int mo, d, h, mi, s, ms;
        if (!parse_iso(AS_STRING(args[0])->chars, &y, &mo, &d, &h, &mi, &s, &ms)) {
            *result = MAKE_NONE();                                  // malformed input
            return true;
        }
        // weekday is derived from the y/m/d triple; same convention as seconds_to_dt
        long long days = days_from_civil(y, mo, d);
        long long w = (4 + days) % 7;
        if (w < 0) w += 7;
        *result = build_dt_table(vm, y, mo, d, h, mi, s, ms, (int)w);
        return true;                                                // builtin handled
    }

    if (strcmp(name, "datetime.format") == 0) {                     // table + fmt -> string
        long long y; int mo, d, h, mi, s, ms;
        if (arg_count < 2 || !IS_STRING(args[1])) {                 // validate arguments
            *result = MAKE_NONE();
            return true;
        }
        if (!read_dt(vm, args[0], &y, &mo, &d, &h, &mi, &s, &ms)) {
            *result = MAKE_NONE();                                  // invalid table
            return true;
        }
        // derive weekday from the y/m/d triple
        long long days = days_from_civil(y, mo, d);
        long long w = (4 + days) % 7;
        if (w < 0) w += 7;
        StringBuilder sb;                                           // dynamic output
        sb_init(&sb);
        format_dt(&sb, y, mo, d, h, mi, s, ms, (int)w, AS_STRING(args[1])->chars);
        // formatted strings are per-call and typically short, but we still use a
        // fresh refcounted string rather than interning to avoid pinning each result
        *result = MAKE_STRING(string_create(sb.buffer ? sb.buffer : "", sb.length));
        sb_free(&sb);                                               // release builder
        return true;                                                // builtin handled
    }

    if (strcmp(name, "datetime.add") == 0) {                        // dt + n * unit -> dt
        long long y; int mo, d, h, mi, s, ms;
        if (arg_count < 3 || !IS_NUMBER(args[1]) || !IS_STRING(args[2])) {
            *result = MAKE_NONE();                                  // validate args
            return true;
        }
        if (!read_dt(vm, args[0], &y, &mo, &d, &h, &mi, &s, &ms)) {
            *result = MAKE_NONE();                                  // invalid table
            return true;
        }
        double n = AS_NUMBER(args[1]);                              // amount
        const char* unit = AS_STRING(args[2])->chars;               // unit name

        if (strcmp(unit, "year") == 0) {                            // calendar year with day clamp
            y += (long long)n;
            int maxd = days_in_month(y, mo);                        // clamp: 2024-01-31 not valid in feb
            if (d > maxd) d = maxd;
        } else if (strcmp(unit, "month") == 0) {                    // calendar month with day clamp
            long long total = (long long)mo - 1 + (long long)n;     // month index relative to jan
            long long dy = y + total / 12;                          // carry into years
            int dm = (int)(((total % 12) + 12) % 12) + 1;           // normalise to 1..12
            y = dy; mo = dm;                                        // commit
            int maxd = days_in_month(y, mo);                        // clamp day to month length
            if (d > maxd) d = maxd;
        } else {                                                    // fixed-length units: shift seconds
            double secs = dt_to_seconds(y, mo, d, h, mi, s, ms);    // convert to epoch seconds
            double delta;                                           // delta in seconds
            if      (strcmp(unit, "week") == 0)        delta = n * 7.0 * 86400.0;
            else if (strcmp(unit, "day") == 0)         delta = n * 86400.0;
            else if (strcmp(unit, "hour") == 0)        delta = n * 3600.0;
            else if (strcmp(unit, "minute") == 0)      delta = n * 60.0;
            else if (strcmp(unit, "second") == 0)      delta = n;
            else if (strcmp(unit, "millisecond") == 0) delta = n / 1000.0;
            else { *result = MAKE_NONE(); return true; }            // unknown unit
            secs += delta;                                          // apply delta
            int wd;                                                 // recomputed weekday
            seconds_to_dt(secs, &y, &mo, &d, &h, &mi, &s, &ms, &wd);
            *result = build_dt_table(vm, y, mo, d, h, mi, s, ms, wd);
            return true;                                            // builtin handled
        }
        // year/month path landed here: recompute weekday from the adjusted y/m/d
        long long days = days_from_civil(y, mo, d);
        long long w = (4 + days) % 7;
        if (w < 0) w += 7;
        *result = build_dt_table(vm, y, mo, d, h, mi, s, ms, (int)w);
        return true;                                                // builtin handled
    }

    if (strcmp(name, "datetime.diff") == 0) {                       // (a - b) in unit
        long long ay, by;                                           // year fields (long long)
        int am, ad, ah, ami, as_, ams;                              // a fields (int, matching read_dt)
        int bm, bd, bh, bmi, bs, bms;                               // b fields (int, matching read_dt)
        if (arg_count < 3 || !IS_STRING(args[2])) {                 // validate args
            *result = MAKE_NONE();
            return true;
        }
        if (!read_dt(vm, args[0], &ay, &am, &ad, &ah, &ami, &as_, &ams) ||
            !read_dt(vm, args[1], &by, &bm, &bd, &bh, &bmi, &bs, &bms)) {
            *result = MAKE_NONE();                                  // either table invalid
            return true;
        }
        double secs_a = dt_to_seconds(ay, am, ad, ah, ami, as_, ams);  // a in epoch seconds
        double secs_b = dt_to_seconds(by, bm, bd, bh, bmi, bs, bms);   // b in epoch seconds
        double delta = secs_a - secs_b;                                // raw difference
        const char* unit = AS_STRING(args[2])->chars;                  // requested unit
        // only fixed-length units are accepted; month/year are deliberately rejected
        if      (strcmp(unit, "week") == 0)   *result = MAKE_NUMBER(delta / (7.0 * 86400.0));
        else if (strcmp(unit, "day") == 0)    *result = MAKE_NUMBER(delta / 86400.0);
        else if (strcmp(unit, "hour") == 0)   *result = MAKE_NUMBER(delta / 3600.0);
        else if (strcmp(unit, "minute") == 0) *result = MAKE_NUMBER(delta / 60.0);
        else if (strcmp(unit, "second") == 0) *result = MAKE_NUMBER(delta);
        else { *result = MAKE_NONE(); return true; }                   // unknown unit
        return true;                                                   // builtin handled
    }

    return false;                                                      // not a recognized builtin
}