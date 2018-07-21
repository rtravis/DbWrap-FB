/*
 * DbTimeStamp.cpp - date, time and time-stamp wrapper classes
 *
 *
 * This is part of the "DbWrap++ for Firebird" (DbWrap++FB)
 * C++ library for accessing Firebird databases in your C++11
 * program.
 *
 * @created: Jan 28, 2015
 *
 * @copyright: Copyright (c) 2015 Robert Zavalczki, distributed
 * under the terms and conditions of the Lesser GNU General
 * Public License version 2.1
 */

#include "DbTimeStamp.h"
#include <ibase.h>

namespace fb
{

static_assert(sizeof(ISC_TIMESTAMP) == sizeof(DbTimeStamp::IscTimestamp),
                "Invalid size for DbTimeStamp::IscTimestamp");

DbDate::DbDate(int iscDate) : isc_date_(iscDate)
{
}

DbDate::DbDate(const DbDate &d) : isc_date_(d.isc_date_)
{
}

int DbDate::iscDate() const
{
    return isc_date_;
}

std::string DbDate::iso8601Date() const
{
    struct tm tm1;
    isc_decode_sql_date(&isc_date_, &tm1);
    char buf[16];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
            tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday);
    return buf;
}

DbTime::DbTime(unsigned int iscTime) : isc_time_(iscTime)
{
}

unsigned int DbTime::iscTime() const
{
    return isc_time_;
}

std::string DbTime::iso8601Time() const
{
    struct tm tm1;
    isc_decode_sql_time(&isc_time_, &tm1);
    char buf[16];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
             tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
    return buf;
}

DbTimeStamp::DbTimeStamp(const IscTimestamp &ts) : isc_ts_(ts)
{
}

DbTimeStamp::DbTimeStamp(const DbTimeStamp &ts) : isc_ts_(ts.isc_ts_)
{
}

DbTimeStamp::IscTimestamp &DbTimeStamp::iscTimestamp()
{
    return isc_ts_;
}

const DbTimeStamp::IscTimestamp &DbTimeStamp::iscTimestamp() const
{
    return isc_ts_;
}

std::string DbTimeStamp::iso8601DateTime() const
{
    struct tm tm1;
    isc_decode_timestamp(reinterpret_cast<const ISC_TIMESTAMP*>(&isc_ts_), &tm1);
    char buf[24];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02dT%02d:%02d:%02d",
            tm1.tm_year + 1900, tm1.tm_mon + 1, tm1.tm_mday,
            tm1.tm_hour, tm1.tm_min, tm1.tm_sec);
    return buf;
}

} /* namespace fb */
