/*
 * DbRowProxy.h - proxy to a row of a query statement result set
 *
 *
 * This is part of the "DbWrap++ for Firebird" (DbWrap++FB)
 * C++ library for accessing Firebird databases in your C++11
 * program.
 *
 * @created: Jan 4, 2015
 *
 * @copyright: Copyright (c) 2015 Robert Zavalczki, distributed
 * under the terms and conditions of the Lesser GNU General
 * Public License version 2.1
 */

#ifndef DBWRAP__FB_DBROWPROXY_H_
#define DBWRAP__FB_DBROWPROXY_H_

#include <cstdint>
#include <string>
#include "FbCommon.h"

#include <unordered_map>

namespace fb {

// forward declarations
class DbBlob;

class DbRowProxy
{
    friend class DbStatement;
public:
    class DbField {
        friend class DbRowProxy;
    private:
        DbRowProxy* rowProxy;
        unsigned int idx;
    public:
        DbField(DbRowProxy* rowProxy, unsigned int idx);

        explicit operator bool() const;
        
        int64_t asInteger();

        double asDouble();

        std::string asString();

        std::string formatDate(const std::string &format = "%Y-%m-%d");
    };

    ~DbRowProxy();

    /** test if this is a valid row */
    explicit operator bool() const;

    unsigned int columnCount() const;
    bool fieldIsNull(unsigned int idx) const;
    int getInt(unsigned int idx) const;
    int64_t getInt64(unsigned int idx) const;
    std::string getText(unsigned int idx) const;
    DbBlob getBlob(unsigned int idx) const;
    DbField fieldByName(const char* name);
    std::string formatDate(unsigned int idx, const std::string &format = "%Y-%m-%d") const;

private:
    std::unordered_map<std::string, unsigned int> fields;
    DbRowProxy(SqlDescriptorArea *sqlda, FbApiHandle db, FbApiHandle tr);

    /** row_ is not owned by this */
    SqlDescriptorArea *row_;
    FbApiHandle db_;
    FbApiHandle transaction_;
};

} /* namespace fb */

#endif /* DBWRAP__FB_DBROWPROXY_H_ */
