/*
 * DbRowProxy.cpp - proxy to a row of a query statement result set
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

#include "DbRowProxy.h"
#include <ibase.h>
#include <cassert>
#include "FbInternals.h"
#include <stdexcept>
#include "DbBlob.h"
#include "DbTimeStamp.h"

namespace fb {

DbRowProxy::DbRowProxy(SqlDescriptorArea *sqlda,
                       FbApiHandle db,
                       FbApiHandle tr) : row_(sqlda),
                                         db_(db),
                                         transaction_(tr)
{
}

unsigned int DbRowProxy::columnCount() const
{
    return static_cast<unsigned>(row_->sqld);
}

bool DbRowProxy::fieldIsNull(unsigned int idx) const
{
    if (!row_) {
        return false;
    }

    if (idx >= static_cast<unsigned int>(row_->sqld)) {
        throw std::out_of_range("result field index is out of range!");
    }

    const XSQLVAR &v1 = row_->sqlvar[idx];
    if (v1.sqlind && *v1.sqlind == -1) {
        // the field is null
        return true;
    }

    return false;
}

int DbRowProxy::getInt(unsigned int idx) const
{
    int64_t n = getInt64(idx);
    if (static_cast<int>(n) != n) {
        throw std::overflow_error("Field can't fit to a 32 bit signed integer!");
    }
    return static_cast<int>(n);
}

int64_t DbRowProxy::getInt64(unsigned int idx) const
{
    if (!row_) {
        return 0;
    }

    if (idx >= static_cast<unsigned int>(row_->sqld)) {
        throw std::out_of_range("result field index is out of range!");
    }

    const XSQLVAR &v1 = row_->sqlvar[idx];
    if (v1.sqlind && *v1.sqlind == -1) {
        // the field is null
        return 0;
    }

    static_assert(sizeof(long long int) >= sizeof(int64_t),
                    "can't use strtoll to convert 64 bit integers!");

    union {
        const FbVarchar *ivc;
        const ISC_TIMESTAMP *its;
        const ISC_QUAD *iquad;
        const ISC_TIME *itime;
        const ISC_DATE *idate;
    } u1;

    int64_t n = 0;
    std::string buf;

    switch (v1.sqltype & ~1) {
    case SQL_TEXT:
        buf.assign(v1.sqldata, static_cast<size_t>(v1.sqllen));
        n = strtoll(buf.c_str(), nullptr, 0);
        break;
    case SQL_VARYING:
        u1.ivc = reinterpret_cast<const FbVarchar*>(v1.sqldata);
        buf.assign(u1.ivc->str, static_cast<size_t>(u1.ivc->length));
        n = strtoll(buf.c_str(), nullptr, 0);
        break;
    case SQL_SHORT:
        n = *(reinterpret_cast<const short*>(v1.sqldata));
        break;
    case SQL_LONG:
        n = *(reinterpret_cast<const ISC_LONG*>(v1.sqldata));
        break;
    case SQL_FLOAT:
        n = static_cast<int64_t>(*(reinterpret_cast<const float*>(v1.sqldata)));
        break;
    case SQL_DOUBLE:
        n = static_cast<int64_t>(*(reinterpret_cast<const double*>(v1.sqldata)));
        break;
    case SQL_D_FLOAT:
        // VAX double?
        n = static_cast<int64_t>(*(reinterpret_cast<const double*>(v1.sqldata)));
        break;
    case SQL_TIMESTAMP:
        u1.its = (reinterpret_cast<const ISC_TIMESTAMP*>(v1.sqldata));
        n = (static_cast<int64_t>(u1.its->timestamp_date) << 32) + u1.its->timestamp_time;
        break;
    case SQL_BLOB:
        u1.iquad = (reinterpret_cast<const ISC_QUAD*>(v1.sqldata));
        n = (static_cast<int64_t>(u1.iquad->gds_quad_high) << 32) + u1.iquad->gds_quad_low;
        break;
    case SQL_ARRAY:
        u1.iquad = (reinterpret_cast<const ISC_QUAD*>(v1.sqldata));
        n = (static_cast<int64_t>(u1.iquad->gds_quad_high) << 32) + u1.iquad->gds_quad_low;
        break;
    case SQL_QUAD:
        u1.iquad = (reinterpret_cast<const ISC_QUAD*>(v1.sqldata));
        n = (static_cast<int64_t>(u1.iquad->gds_quad_high) << 32) + u1.iquad->gds_quad_low;
        break;
    case SQL_TYPE_TIME:
        u1.itime = (reinterpret_cast<const ISC_TIME*>(v1.sqldata));
        n = *(u1.itime);
        break;
    case SQL_TYPE_DATE:
        u1.idate = (reinterpret_cast<const ISC_DATE*>(v1.sqldata));
        n = *(u1.idate);
        break;
    case SQL_INT64:
        n = *(reinterpret_cast<const ISC_INT64*>(v1.sqldata));
        break;
    case SQL_NULL:
    default:
        n = 0;
        break;
    }

    return n;
}

std::string DbRowProxy::getText(unsigned int idx) const
{
    if (!row_) {
        return std::string();
    }

    if (idx >= static_cast<unsigned int>(row_->sqld)) {
        throw std::out_of_range("result field index is out of range!");
    }

    std::string buf;
    const XSQLVAR &v1 = row_->sqlvar[idx];
    if (v1.sqlind && *v1.sqlind == -1) {
        // the field is null
        // buf = "[null]";
        return buf;
    }

    union {
        const FbVarchar *ivc;
        const ISC_TIMESTAMP *its;
        const ISC_QUAD *iquad;
        const ISC_TIME *itime;
        const ISC_DATE *idate;
        long long int n;
        double d;
    } u1;

    char convBuf[32];

    switch (v1.sqltype & ~1) {
    case SQL_TEXT:
        buf.assign(v1.sqldata, static_cast<size_t>(v1.sqllen));
        break;
    case SQL_VARYING:
        u1.ivc = reinterpret_cast<const FbVarchar*>(v1.sqldata);
        buf.assign(u1.ivc->str, static_cast<size_t>(u1.ivc->length));
        break;
    case SQL_SHORT:
        u1.n = *(reinterpret_cast<const short*>(v1.sqldata));
        snprintf(convBuf, sizeof(convBuf), "%lld", u1.n);
        buf = convBuf;
        break;
    case SQL_LONG:
        u1.n = *(reinterpret_cast<const ISC_LONG*>(v1.sqldata));
        snprintf(convBuf, sizeof(convBuf), "%lld", u1.n);
        buf = convBuf;
        break;
    case SQL_FLOAT:
        u1.d = static_cast<double>(*(reinterpret_cast<const float*>(v1.sqldata)));
        snprintf(convBuf, sizeof(convBuf), "%g", u1.d);
        buf = convBuf;
        break;
    case SQL_DOUBLE:
        u1.d = *(reinterpret_cast<const double*>(v1.sqldata));
        snprintf(convBuf, sizeof(convBuf), "%g", u1.d);
        buf = convBuf;
        break;
    case SQL_D_FLOAT:
        // VAX double?
        u1.d = *(reinterpret_cast<const double*>(v1.sqldata));
        snprintf(convBuf, sizeof(convBuf), "%g", u1.d);
        buf = convBuf;
        break;
    case SQL_TIMESTAMP:
        u1.its = (reinterpret_cast<const ISC_TIMESTAMP*>(v1.sqldata));
        buf = DbTimeStamp(
                reinterpret_cast<const DbTimeStamp::IscTimestamp&>(*u1.its)).iso8601DateTime();
        break;
    case SQL_BLOB:
        {
            DbBlob blob = getBlob(idx);
            if (blob) {
                buf = blob.readAll(65536);
            }
        }
        break;
    case SQL_ARRAY:
        u1.iquad = (reinterpret_cast<const ISC_QUAD*>(v1.sqldata));
        snprintf(convBuf, sizeof(convBuf), "array %x:%x",
                static_cast<unsigned>(u1.iquad->gds_quad_high),
                static_cast<unsigned>(u1.iquad->gds_quad_low));
        buf = convBuf;
        break;
    case SQL_QUAD:
        u1.iquad = (reinterpret_cast<const ISC_QUAD*>(v1.sqldata));
        snprintf(convBuf, sizeof(convBuf), "%08x:%08x",
                static_cast<unsigned>(u1.iquad->gds_quad_high),
                static_cast<unsigned>(u1.iquad->gds_quad_low));
        buf = convBuf;
        break;
    case SQL_TYPE_TIME:
        u1.itime = (reinterpret_cast<const ISC_TIME*>(v1.sqldata));
        buf = DbTime(*u1.itime).iso8601Time();
        break;
    case SQL_TYPE_DATE:
        u1.idate = (reinterpret_cast<const ISC_DATE*>(v1.sqldata));
        buf = DbDate(*u1.idate).iso8601Date();
        break;
    case SQL_INT64:
        u1.n = *(reinterpret_cast<const ISC_INT64*>(v1.sqldata));
        snprintf(convBuf, sizeof(convBuf), "%lld", u1.n);
        buf = convBuf;
        break;
    case SQL_NULL:
        buf = "[null]";
        break;
    default:
        buf.clear();
        break;
    }

    return buf;
}

DbBlob DbRowProxy::getBlob(unsigned int idx) const
{
    if (!row_) {
        return DbBlob(0, 0, nullptr);
    }

    if (idx >= static_cast<unsigned int>(row_->sqld)) {
        throw std::out_of_range("result field index is out of range!");
    }

    const XSQLVAR &v1 = row_->sqlvar[idx];
    if (v1.sqlind && *v1.sqlind == -1) {
        // the field is null
        return DbBlob(0, 0, nullptr);
    }

    if ((v1.sqltype & ~1) != SQL_BLOB) {
        throw std::logic_error("Field type is not blob!");
    }

    assert(v1.sqllen == sizeof(ISC_QUAD));
    return DbBlob(db_, transaction_, reinterpret_cast<const FbQuad*>(v1.sqldata));
}

DbRowProxy::operator bool() const
{
    return (row_ != nullptr);
}

} /* namespace fb */
