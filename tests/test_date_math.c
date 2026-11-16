/* Host-side tests for date_math.c. Run: make -C tests */
#include "../date_math.h"

#include <stdio.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond)                                                     \
    do {                                                                \
        if(!(cond)) {                                                   \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);      \
            failures++;                                                 \
        }                                                               \
    } while(0)

static SobrietyDate D(int y, int m, int d) {
    SobrietyDate r = {(uint16_t)y, (uint8_t)m, (uint8_t)d};
    return r;
}

static void check_label(int32_t idx, const char* expected) {
    char buf[24];
    Milestone m = milestone_at((uint32_t)idx);
    milestone_label(&m, buf, sizeof(buf));
    if(strcmp(buf, expected) != 0) {
        printf("FAIL label[%d]: got '%s' want '%s'\n", idx, buf, expected);
        failures++;
    }
}

int main(void) {
    SobrietyDate a, b;

    /* day numbers */
    a = D(1970, 1, 1);
    CHECK(date_to_day_number(&a) == 0);
    a = D(2000, 3, 1);
    CHECK(date_to_day_number(&a) == 11017);

    /* leap years */
    CHECK(date_is_leap_year(2000));
    CHECK(!date_is_leap_year(1900));
    CHECK(date_is_leap_year(2024));
    CHECK(date_days_in_month(2024, 2) == 29);
    CHECK(date_days_in_month(2023, 2) == 28);
    a = D(2023, 2, 29);
    CHECK(!date_is_valid(&a));

    /* diffs */
    a = D(2024, 1, 1);
    b = D(2025, 1, 1);
    CHECK(date_diff_days(&a, &b) == 366);
    a = D(2025, 1, 1);
    b = D(2026, 1, 1);
    CHECK(date_diff_days(&a, &b) == 365);
    CHECK(date_diff_days(&b, &a) == -365);

    /* add months with clamping */
    a = D(2024, 1, 31);
    b = date_add_months(&a, 1);
    CHECK(b.year == 2024 && b.month == 2 && b.day == 29);
    b = date_add_months(&a, 13);
    CHECK(b.year == 2025 && b.month == 2 && b.day == 28);
    a = D(2024, 11, 15);
    b = date_add_months(&a, 2);
    CHECK(b.year == 2025 && b.month == 1 && b.day == 15);

    /* spans */
    a = D(2023, 3, 10);
    b = D(2026, 11, 16);
    SobrietySpan s = date_span(&a, &b);
    CHECK(s.years == 3 && s.months == 8 && s.days == 6);
    a = D(2026, 10, 20);
    s = date_span(&a, &b);
    CHECK(s.years == 0 && s.months == 0 && s.days == 27);
    s = date_span(&b, &a); /* future start */
    CHECK(s.years == 0 && s.months == 0 && s.days == 0);
    a = D(2024, 1, 31);
    b = D(2024, 2, 29);
    s = date_span(&a, &b);
    CHECK(s.years == 0 && s.months == 1 && s.days == 0);

    /* milestone dates */
    a = D(2026, 1, 1);
    Milestone m = {MilestoneUnitDays, 90};
    b = milestone_date(&a, &m);
    CHECK(b.year == 2026 && b.month == 4 && b.day == 1);
    m.unit = MilestoneUnitMonths;
    m.value = 18;
    b = milestone_date(&a, &m);
    CHECK(b.year == 2027 && b.month == 7 && b.day == 1);

    /* milestone sequence + labels */
    check_label(0, "24 Hours");
    check_label(1, "30 Days");
    check_label(3, "90 Days");
    check_label(4, "6 Months");
    check_label(6, "1 Year");
    check_label(7, "18 Months");
    check_label(8, "2 Years");
    check_label(9, "3 Years");

    /* last reached */
    a = D(2026, 11, 16);
    CHECK(milestone_last_reached(&a, &a) == -1);
    b = D(2026, 11, 17);
    CHECK(milestone_last_reached(&a, &b) == 0);
    b = D(2027, 2, 14); /* 90 days later */
    CHECK(milestone_last_reached(&a, &b) == 3);
    b = D(2027, 2, 13);
    CHECK(milestone_last_reached(&a, &b) == 2);
    b = D(2036, 11, 16); /* 10 years */
    CHECK(milestone_last_reached(&a, &b) == 16);

    if(failures == 0) printf("All date_math tests passed\n");
    return failures == 0 ? 0 : 1;
}
