#include "date_math.h"

#include <stdio.h>

bool date_is_leap_year(uint16_t year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

uint8_t date_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if(month < 1 || month > 12) return 0;
    if(month == 2 && date_is_leap_year(year)) return 29;
    return days[month - 1];
}

bool date_is_valid(const SobrietyDate* d) {
    return d->year >= 1900 && d->year <= 2999 && d->month >= 1 && d->month <= 12 &&
           d->day >= 1 && d->day <= date_days_in_month(d->year, d->month);
}

/* Howard Hinnant's days_from_civil. */
int32_t date_to_day_number(const SobrietyDate* d) {
    int32_t y = (int32_t)d->year - (d->month <= 2 ? 1 : 0);
    int32_t m = d->month;
    int32_t era = (y >= 0 ? y : y - 399) / 400;
    int32_t yoe = y - era * 400;
    int32_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d->day - 1;
    int32_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

int32_t date_diff_days(const SobrietyDate* from, const SobrietyDate* to) {
    return date_to_day_number(to) - date_to_day_number(from);
}

SobrietyDate date_add_months(const SobrietyDate* d, uint32_t months) {
    uint32_t total = (uint32_t)d->year * 12 + (d->month - 1) + months;
    SobrietyDate r;
    r.year = (uint16_t)(total / 12);
    r.month = (uint8_t)(total % 12 + 1);
    uint8_t dim = date_days_in_month(r.year, r.month);
    r.day = d->day > dim ? dim : d->day;
    return r;
}

SobrietySpan date_span(const SobrietyDate* start, const SobrietyDate* today) {
    SobrietySpan span = {0, 0, 0};
    if(date_diff_days(start, today) <= 0) return span;

    int32_t months = ((int32_t)today->year * 12 + today->month) -
                     ((int32_t)start->year * 12 + start->month);
    if(months < 0) months = 0;
    /* Step back until start + months does not overshoot today. */
    while(months > 0) {
        SobrietyDate anchor = date_add_months(start, (uint32_t)months);
        if(date_diff_days(&anchor, today) >= 0) break;
        months--;
    }
    SobrietyDate anchor = date_add_months(start, (uint32_t)months);
    span.years = (uint32_t)months / 12;
    span.months = (uint32_t)months % 12;
    span.days = (uint32_t)date_diff_days(&anchor, today);
    return span;
}

SobrietyDate milestone_date(const SobrietyDate* start, const Milestone* m) {
    if(m->unit == MilestoneUnitMonths) return date_add_months(start, m->value);

    int32_t target = date_to_day_number(start) + (int32_t)m->value;
    /* civil_from_days */
    int32_t z = target + 719468;
    int32_t era = (z >= 0 ? z : z - 146096) / 146097;
    int32_t doe = z - era * 146097;
    int32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int32_t y = yoe + era * 400;
    int32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    int32_t mp = (5 * doy + 2) / 153;
    int32_t dd = doy - (153 * mp + 2) / 5 + 1;
    int32_t mm = mp < 10 ? mp + 3 : mp - 9;
    SobrietyDate r = {(uint16_t)(y + (mm <= 2 ? 1 : 0)), (uint8_t)mm, (uint8_t)dd};
    return r;
}

Milestone milestone_at(uint32_t index) {
    static const Milestone fixed[] = {
        {MilestoneUnitDays, 1},
        {MilestoneUnitDays, 30},
        {MilestoneUnitDays, 60},
        {MilestoneUnitDays, 90},
        {MilestoneUnitMonths, 6},
        {MilestoneUnitMonths, 9},
        {MilestoneUnitMonths, 12},
        {MilestoneUnitMonths, 18},
    };
    const uint32_t fixed_count = sizeof(fixed) / sizeof(fixed[0]);
    if(index < fixed_count) return fixed[index];
    /* After 18 months: every year starting with 2 years. */
    Milestone m = {MilestoneUnitMonths, (index - fixed_count + 2) * 12};
    return m;
}

int32_t milestone_last_reached(const SobrietyDate* start, const SobrietyDate* today) {
    int32_t last = -1;
    for(uint32_t i = 0; i < 1000; i++) {
        Milestone m = milestone_at(i);
        SobrietyDate when = milestone_date(start, &m);
        if(date_diff_days(&when, today) < 0) break;
        last = (int32_t)i;
    }
    return last;
}

void milestone_label(const Milestone* m, char* out, uint32_t out_size) {
    if(m->unit == MilestoneUnitDays) {
        if(m->value == 1) {
            snprintf(out, out_size, "24 Hours");
        } else {
            snprintf(out, out_size, "%lu Days", (unsigned long)m->value);
        }
    } else if(m->value % 12 == 0) {
        unsigned long years = m->value / 12;
        snprintf(out, out_size, "%lu Year%s", years, years == 1 ? "" : "s");
    } else {
        snprintf(out, out_size, "%lu Months", (unsigned long)m->value);
    }
}
