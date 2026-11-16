#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Pure calendar helpers (no Flipper dependencies, unit-testable on host). */

typedef struct {
    uint16_t year;
    uint8_t month; /* 1..12 */
    uint8_t day; /* 1..31 */
} SobrietyDate;

typedef struct {
    uint32_t years;
    uint32_t months;
    uint32_t days;
} SobrietySpan;

typedef enum {
    MilestoneUnitDays,
    MilestoneUnitMonths,
} MilestoneUnit;

typedef struct {
    MilestoneUnit unit;
    uint32_t value;
} Milestone;

bool date_is_leap_year(uint16_t year);
uint8_t date_days_in_month(uint16_t year, uint8_t month);
bool date_is_valid(const SobrietyDate* d);

/* Days since 1970-01-01 (proleptic Gregorian). */
int32_t date_to_day_number(const SobrietyDate* d);

/* Signed difference in whole days: to - from. */
int32_t date_diff_days(const SobrietyDate* from, const SobrietyDate* to);

/* Add calendar months, clamping the day to the end of the target month. */
SobrietyDate date_add_months(const SobrietyDate* d, uint32_t months);

/* Calendar breakdown from start to today (zero if today < start). */
SobrietySpan date_span(const SobrietyDate* start, const SobrietyDate* today);

/* Date on which a milestone is reached. */
SobrietyDate milestone_date(const SobrietyDate* start, const Milestone* m);

/* Milestone #index in the (infinite) chip sequence:
 * 24 hours, 30, 60, 90 days, 6, 9 months, 1 year, 18 months, 2, 3, 4... years. */
Milestone milestone_at(uint32_t index);

/* Index of the highest milestone reached, or -1 if none yet. */
int32_t milestone_last_reached(const SobrietyDate* start, const SobrietyDate* today);

/* Human readable chip name, e.g. "24 Hours", "90 Days", "6 Months", "1 Year". */
void milestone_label(const Milestone* m, char* out, uint32_t out_size);
