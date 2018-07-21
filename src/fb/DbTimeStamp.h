/*
 * DbTimeStamp.h - date, time and time-stamp wrapper classes
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

#ifndef DBWRAP_FB_SRC_DBTIMESTAMP_H_
#define DBWRAP_FB_SRC_DBTIMESTAMP_H_

#include <string>

namespace fb
{

class DbDate
{
public:
    DbDate(int iscDate);
    DbDate(const DbDate&);

    int iscDate() const;
    std::string iso8601Date() const;

private:
    int isc_date_;
};

class DbTime
{
public:
    explicit DbTime(unsigned int iscTime);

    unsigned int iscTime() const;
    std::string iso8601Time() const;

private:
    unsigned int isc_time_;
};

class DbTimeStamp
{
public:
    struct IscTimestamp
    {
        int isc_date_;
        unsigned int isc_time_;
    };

    DbTimeStamp(const IscTimestamp &ts);
    DbTimeStamp(const DbTimeStamp &ts);

    IscTimestamp &iscTimestamp();
    const IscTimestamp &iscTimestamp() const;

    std::string iso8601DateTime() const;

private:
    IscTimestamp isc_ts_;
};

} /* namespace fb */

#endif /* DBWRAP_FB_SRC_DBTIMESTAMP_H_ */
