#include "core/world_time/internal.h"

namespace world_time_internal {

auto days_from_civil(Sint32 year, unsigned month, unsigned day) -> Sint64
{
    year -= month <= 2;
    const Sint32 era = (year >= 0 ? year : year - 399) / 400;
    const unsigned yoe = static_cast<unsigned>(year - era * 400);
    const unsigned doy = (153 * (month + (month > 2 ? -3u : 9u)) + 2) / 5 + day - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return static_cast<Sint64>(era) * 146097 + static_cast<Sint64>(doe) - 719468;
}

auto civil_from_days(Sint64 z) -> world_time_date_t
{
    z += 719468;
    const Sint64 era = (z >= 0 ? z : z - 146096) / 146097;
    const unsigned doe = static_cast<unsigned>(z - era * 146097);
    const unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const Sint32 year = static_cast<Sint32>(yoe) + static_cast<Sint32>(era) * 400;
    const unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const unsigned mp = (5 * doy + 2) / 153;
    const unsigned day = doy - (153 * mp + 2) / 5 + 1;
    const unsigned month = mp < 10 ? mp + 3 : mp - 9;
    return {
        .year = year + (month <= 2),
        .month = static_cast<Sint32>(month),
        .day = static_cast<Sint32>(day),
    };
}

} // namespace world_time_internal

bool world_time_is_leap_year(Sint32 year)
{
    if ((year % 4) != 0)
    {
        return false;
    }
    if ((year % 100) != 0)
    {
        return true;
    }
    return (year % 400) == 0;
}

int world_time_days_in_month(Sint32 year, Sint32 month)
{
    static const int kDaysPerMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month < 1 || month > 12)
    {
        return 31;
    }
    if (month == 2 && world_time_is_leap_year(year))
    {
        return 29;
    }
    return kDaysPerMonth[month - 1];
}
