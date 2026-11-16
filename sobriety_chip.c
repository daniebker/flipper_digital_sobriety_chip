/*
 * Digital Sobriety Chip for Flipper Zero.
 *
 * Shows the number of days since a chosen sobriety date as a "chip",
 * along with the calendar breakdown, the last chip milestone earned
 * and how long until the next one. The date is stored on the SD card.
 *
 * Controls:
 *   Chip screen:  OK = set date, Back = exit
 *   Date editor:  Left/Right = select field, Up/Down = change value,
 *                 OK = save, Back = cancel
 */

#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <storage/storage.h>
#include <flipper_format/flipper_format.h>

#include <stdio.h>

#include "date_math.h"

#define TAG "SobrietyChip"

#define SETTINGS_PATH     APP_DATA_PATH("sobriety.conf")
#define SETTINGS_FILETYPE "Sobriety Chip"
#define SETTINGS_VERSION  1
#define MIN_YEAR          1950

typedef enum {
    ModeChip,
    ModeEdit,
} Mode;

typedef enum {
    FieldYear,
    FieldMonth,
    FieldDay,
    FieldCount,
} Field;

typedef struct {
    FuriMutex* mutex;
    Mode mode;
    bool has_date;
    SobrietyDate start;
    SobrietyDate edit;
    Field field;
} AppState;

/* ---------- helpers ---------- */

static SobrietyDate today_date(void) {
    DateTime dt;
    furi_hal_rtc_get_datetime(&dt);
    SobrietyDate d = {dt.year, dt.month, dt.day};
    return d;
}

static void format_date(const SobrietyDate* d, char* out, size_t size) {
    snprintf(out, size, "%04u-%02u-%02u", (unsigned)d->year, (unsigned)d->month, (unsigned)d->day);
}

/* ---------- persistence ---------- */

static bool settings_load(SobrietyDate* out) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    FuriString* filetype = furi_string_alloc();
    bool ok = false;

    do {
        if(!flipper_format_file_open_existing(ff, SETTINGS_PATH)) break;
        uint32_t version = 0;
        if(!flipper_format_read_header(ff, filetype, &version)) break;
        if(furi_string_cmp_str(filetype, SETTINGS_FILETYPE) != 0) break;
        if(version != SETTINGS_VERSION) break;

        uint32_t year = 0, month = 0, day = 0;
        if(!flipper_format_read_uint32(ff, "Year", &year, 1)) break;
        if(!flipper_format_read_uint32(ff, "Month", &month, 1)) break;
        if(!flipper_format_read_uint32(ff, "Day", &day, 1)) break;

        SobrietyDate d = {(uint16_t)year, (uint8_t)month, (uint8_t)day};
        if(!date_is_valid(&d)) break;
        *out = d;
        ok = true;
    } while(false);

    furi_string_free(filetype);
    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

static bool settings_save(const SobrietyDate* d) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    FlipperFormat* ff = flipper_format_file_alloc(storage);
    bool ok = false;

    do {
        if(!flipper_format_file_open_always(ff, SETTINGS_PATH)) break;
        if(!flipper_format_write_header_cstr(ff, SETTINGS_FILETYPE, SETTINGS_VERSION)) break;
        uint32_t year = d->year, month = d->month, day = d->day;
        if(!flipper_format_write_uint32(ff, "Year", &year, 1)) break;
        if(!flipper_format_write_uint32(ff, "Month", &month, 1)) break;
        if(!flipper_format_write_uint32(ff, "Day", &day, 1)) break;
        ok = true;
    } while(false);

    if(!ok) FURI_LOG_E(TAG, "Failed to save settings");

    flipper_format_free(ff);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

/* ---------- drawing ---------- */

static void draw_chip(Canvas* canvas, const AppState* state) {
    SobrietyDate today = today_date();
    int32_t days = date_diff_days(&state->start, &today);
    if(days < 0) days = 0;

    char buf[32];

    /* The chip: double ring with day count in the middle. */
    const int cx = 31, cy = 32;
    canvas_draw_circle(canvas, cx, cy, 30);
    canvas_draw_circle(canvas, cx, cy, 26);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, cx, cy - 15, AlignCenter, AlignCenter, "SOBER");

    snprintf(buf, sizeof(buf), "%ld", (long)days);
    canvas_set_font(canvas, days < 10000 ? FontBigNumbers : FontPrimary);
    canvas_draw_str_aligned(canvas, cx, cy, AlignCenter, AlignCenter, buf);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, cx, cy + 15, AlignCenter, AlignCenter, days == 1 ? "DAY" : "DAYS");

    /* Info panel on the right. */
    const int x = 66;
    canvas_draw_str(canvas, x, 9, "Sober since");
    format_date(&state->start, buf, sizeof(buf));
    canvas_draw_str(canvas, x, 19, buf);

    SobrietySpan span = date_span(&state->start, &today);
    snprintf(
        buf,
        sizeof(buf),
        "%luy %lum %lud",
        (unsigned long)span.years,
        (unsigned long)span.months,
        (unsigned long)span.days);
    canvas_draw_str(canvas, x, 30, buf);

    canvas_draw_line(canvas, x, 33, 127, 33);

    /* Last chip earned (filled dot) */
    int32_t last = milestone_last_reached(&state->start, &today);
    canvas_draw_disc(canvas, x + 2, 39, 2);
    if(last >= 0) {
        Milestone m = milestone_at((uint32_t)last);
        milestone_label(&m, buf, sizeof(buf));
    } else {
        snprintf(buf, sizeof(buf), "No chip yet");
    }
    canvas_draw_str(canvas, x + 7, 42, buf);

    /* Next chip (hollow dot) and countdown */
    Milestone next = milestone_at((uint32_t)(last + 1));
    canvas_draw_circle(canvas, x + 2, 49, 2);
    milestone_label(&next, buf, sizeof(buf));
    canvas_draw_str(canvas, x + 7, 52, buf);

    SobrietyDate next_date = milestone_date(&state->start, &next);
    int32_t remaining = date_diff_days(&today, &next_date);
    snprintf(buf, sizeof(buf), "in %ld day%s", (long)remaining, remaining == 1 ? "" : "s");
    canvas_draw_str(canvas, x + 7, 62, buf);
}

static void draw_edit(Canvas* canvas, const AppState* state) {
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 2, AlignCenter, AlignTop, "Set Sober Date");

    char year[8], month[4], day[4];
    snprintf(year, sizeof(year), "%04u", (unsigned)state->edit.year);
    snprintf(month, sizeof(month), "%02u", (unsigned)state->edit.month);
    snprintf(day, sizeof(day), "%02u", (unsigned)state->edit.day);
    const char* parts[FieldCount] = {year, month, day};

    canvas_set_font(canvas, FontBigNumbers);
    const int gap = 12;
    const int top = 19;
    const int h = canvas_current_font_height(canvas);
    int widths[FieldCount];
    int total = gap * (FieldCount - 1);
    for(int i = 0; i < FieldCount; i++) {
        widths[i] = canvas_string_width(canvas, parts[i]);
        total += widths[i];
    }

    int x = (128 - total) / 2;
    for(int i = 0; i < FieldCount; i++) {
        if(i > 0) {
            /* dash separator */
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, x - gap / 2 - 2, top + h / 2 - 1, 4, 2);
        }
        if((Field)i == state->field) {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_rbox(canvas, x - 3, top - 3, widths[i] + 6, h + 6, 2);
            canvas_set_color(canvas, ColorWhite);
        } else {
            canvas_set_color(canvas, ColorBlack);
        }
        canvas_draw_str_aligned(canvas, x, top, AlignLeft, AlignTop, parts[i]);
        x += widths[i] + gap;
    }
    canvas_set_color(canvas, ColorBlack);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 46, AlignCenter, AlignBottom, "< > field   ^ v change");
    canvas_draw_str_aligned(
        canvas, 64, 58, AlignCenter, AlignBottom, state->has_date ? "OK save   Back cancel" : "OK save   Back exit");
}

static void draw_callback(Canvas* canvas, void* ctx) {
    AppState* state = ctx;
    furi_check(furi_mutex_acquire(state->mutex, FuriWaitForever) == FuriStatusOk);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    if(state->mode == ModeChip) {
        draw_chip(canvas, state);
    } else {
        draw_edit(canvas, state);
    }
    furi_mutex_release(state->mutex);
}

static void input_callback(InputEvent* event, void* ctx) {
    FuriMessageQueue* queue = ctx;
    furi_message_queue_put(queue, event, FuriWaitForever);
}

/* ---------- editing ---------- */

static void clamp_edit(AppState* state) {
    SobrietyDate today = today_date();
    SobrietyDate* e = &state->edit;

    if(e->year < MIN_YEAR) e->year = MIN_YEAR;
    if(e->year > today.year) e->year = today.year;
    uint8_t dim = date_days_in_month(e->year, e->month);
    if(e->day > dim) e->day = dim;

    /* Never allow a date in the future. */
    if(date_diff_days(e, &today) < 0) *e = today;
}

static void adjust_field(AppState* state, int delta) {
    SobrietyDate* e = &state->edit;
    switch(state->field) {
    case FieldYear:
        e->year = (uint16_t)((int)e->year + delta);
        break;
    case FieldMonth: {
        int m = (int)e->month + delta;
        if(m < 1) m = 12;
        if(m > 12) m = 1;
        e->month = (uint8_t)m;
        break;
    }
    case FieldDay: {
        int dim = date_days_in_month(e->year, e->month);
        int d = (int)e->day + delta;
        if(d < 1) d = dim;
        if(d > dim) d = 1;
        e->day = (uint8_t)d;
        break;
    }
    default:
        break;
    }
    clamp_edit(state);
}

static void enter_edit(AppState* state) {
    state->edit = state->has_date ? state->start : today_date();
    state->field = FieldYear;
    state->mode = ModeEdit;
}

/* Returns false when the app should exit. */
static bool handle_input(AppState* state, const InputEvent* event) {
    bool is_press = event->type == InputTypeShort;
    bool is_repeat = event->type == InputTypeRepeat;

    if(state->mode == ModeChip) {
        if(!is_press) return true;
        if(event->key == InputKeyBack) return false;
        if(event->key == InputKeyOk) enter_edit(state);
        return true;
    }

    /* ModeEdit */
    if(!is_press && !is_repeat) return true;
    switch(event->key) {
    case InputKeyUp:
        adjust_field(state, 1);
        break;
    case InputKeyDown:
        adjust_field(state, -1);
        break;
    case InputKeyLeft:
        if(is_press) state->field = (Field)((state->field + FieldCount - 1) % FieldCount);
        break;
    case InputKeyRight:
        if(is_press) state->field = (Field)((state->field + 1) % FieldCount);
        break;
    case InputKeyOk:
        if(is_press) {
            clamp_edit(state);
            state->start = state->edit;
            state->has_date = true;
            settings_save(&state->start);
            state->mode = ModeChip;
        }
        break;
    case InputKeyBack:
        if(is_press) {
            if(!state->has_date) return false;
            state->mode = ModeChip;
        }
        break;
    default:
        break;
    }
    return true;
}

/* ---------- entry point ---------- */

int32_t sobriety_chip_app(void* p) {
    UNUSED(p);

    AppState* state = malloc(sizeof(AppState));
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    state->has_date = settings_load(&state->start);
    state->mode = ModeChip;
    if(!state->has_date) enter_edit(state);

    FuriMessageQueue* queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, state);
    view_port_input_callback_set(view_port, input_callback, queue);

    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    bool running = true;
    InputEvent event;
    while(running) {
        /* Wake at least once a second so the count rolls over at midnight. */
        if(furi_message_queue_get(queue, &event, 1000) == FuriStatusOk) {
            furi_check(furi_mutex_acquire(state->mutex, FuriWaitForever) == FuriStatusOk);
            running = handle_input(state, &event);
            furi_mutex_release(state->mutex);
        }
        view_port_update(view_port);
    }

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(queue);
    furi_mutex_free(state->mutex);
    free(state);

    return 0;
}
