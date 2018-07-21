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

#include <memory>
#include <unordered_map>

namespace fb {

// forward declarations
class DbBlob;
class DbField; 

using DbFieldPtr = std::shared_ptr<DbField>; // max68

class DbRowProxy
{
    friend class DbStatement;
public:
    ~DbRowProxy();

    /** test if this is a valid row */
    explicit operator bool() const;

    unsigned int columnCount() const;
    bool fieldIsNull(unsigned int idx) const;
    int getInt(unsigned int idx) const;
    int64_t getInt64(unsigned int idx) const;
    std::string getText(unsigned int idx) const;
    DbBlob getBlob(unsigned int idx) const;
    DbFieldPtr fieldByName(const std::string& name);
    std::string formatDate(unsigned int idx, const std::string &format = "%Y-%m-%d") const;

private:
    std::unordered_map<std::string, DbFieldPtr> fields;
    DbRowProxy(SqlDescriptorArea *sqlda, FbApiHandle db, FbApiHandle tr);

    /** row_ is not owned by this */
    SqlDescriptorArea *row_;
    FbApiHandle db_;
    FbApiHandle transaction_;
};

class DbField {
    //friend class DbRowProxy;
private:
    DbRowProxy* rowProxy;
    unsigned int idx;
public:
    DbField(DbRowProxy* rowProxy, unsigned int idx) 
    :rowProxy(rowProxy), idx(idx){};

    int64_t asInteger() {
        return rowProxy->getInt64(idx);
    }

    double asDouble() {
        return std::strtod(rowProxy->getText(idx).c_str(), nullptr);
    }

    std::string asString() {
        return rowProxy->getText(idx);
    }

    std::string formatDate(const std::string &format = "%Y-%m-%d") {
        return rowProxy->formatDate(idx, format);
    }
};

} /* namespace fb */

#endif /* DBWRAP__FB_DBROWPROXY_H_ */
