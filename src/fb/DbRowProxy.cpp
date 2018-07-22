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

#include <time.h>
#include <iomanip>

#include <algorithm>
#include <cstring>

namespace fb {

DbRowProxy::DbRowProxy(SqlDescriptorArea *sqlda,
                       FbApiHandle db,
                       FbApiHandle tr) : row_(sqlda),
                                         db_(db),
                                         transaction_(tr)
{
}

DbRowProxy::~DbRowProxy()
{
}

unsigned int DbRowProxy::columnCount() const
{
    return row_->sqld;
}

bool DbRowProxy::fieldIsNull(unsigned int idx) const
{
    if (!row_) {
        return false;
    }

    if (idx >= (unsigned int) row_->sqld) {
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
    if ((int) n != n) {
        throw std::overflow_error("Field can't fit to a 32 bit signed integer!");
    }
    return (int) n;
}

int64_t DbRowProxy::getInt64(unsigned int idx) const
{
    if (!row_) {
        return 0;
    }

    if (idx >= (unsigned int) row_->sqld) {
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
        buf.assign(v1.sqldata, v1.sqllen);
        n = strtoll(buf.c_str(), nullptr, 0);
        break;
    case SQL_VARYING:
        u1.ivc = (const FbVarchar*) v1.sqldata;
        buf.assign(u1.ivc->str, (size_t) u1.ivc->length);
        n = strtoll(buf.c_str(), nullptr, 0);
        break;
    case SQL_SHORT:
        n = *((const short*) v1.sqldata);
        break;
    case SQL_LONG:
        n = *((const ISC_LONG*) v1.sqldata);
        break;
    case SQL_FLOAT:
        n = *((const float*) v1.sqldata);
        break;
    case SQL_DOUBLE:
        n = *((const double*) v1.sqldata);
        break;
    case SQL_D_FLOAT:
        // VAX double?
        n = *((const double*) v1.sqldata);
        break;
    case SQL_TIMESTAMP:
        u1.its = ((const ISC_TIMESTAMP*) v1.sqldata);
        n = ((uint64_t) u1.its->timestamp_date << 32) + u1.its->timestamp_time;
        break;
    case SQL_BLOB:
        u1.iquad = ((const ISC_QUAD*) v1.sqldata);
        n = ((uint64_t) u1.iquad->gds_quad_high << 32) + u1.iquad->gds_quad_low;
        break;
    case SQL_ARRAY:
        u1.iquad = ((const ISC_QUAD*) v1.sqldata);
        n = ((uint64_t) u1.iquad->gds_quad_high << 32) + u1.iquad->gds_quad_low;
        break;
    case SQL_QUAD:
        u1.iquad = ((const ISC_QUAD*) v1.sqldata);
        n = ((uint64_t) u1.iquad->gds_quad_high << 32) + u1.iquad->gds_quad_low;
        break;
    case SQL_TYPE_TIME:
        u1.itime = ((const ISC_TIME*) v1.sqldata);
        n = *(u1.itime);
        break;
    case SQL_TYPE_DATE:
        u1.idate = ((const ISC_DATE*) v1.sqldata);
        n = *(u1.idate);
        break;
    case SQL_INT64:
        n = *((const ISC_INT64*) v1.sqldata);
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

    if (idx >= (unsigned int) row_->sqld) {
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
        buf.assign(v1.sqldata, v1.sqllen);
        break;
    case SQL_VARYING:
        u1.ivc = (const FbVarchar*) v1.sqldata;
        buf.assign(u1.ivc->str, (size_t) u1.ivc->length);
        break;
    case SQL_SHORT:
        u1.n = *((const short*) v1.sqldata);
        snprintf(convBuf, sizeof(convBuf), "%lld", u1.n);
        buf = convBuf;
        break;
    case SQL_LONG:
        u1.n = *((const ISC_LONG*) v1.sqldata);
        snprintf(convBuf, sizeof(convBuf), "%lld", u1.n);
        buf = convBuf;
        break;
    case SQL_FLOAT:
        u1.d = *((const float*) v1.sqldata);
        snprintf(convBuf, sizeof(convBuf), "%g", u1.d);
        buf = convBuf;
        break;
    case SQL_DOUBLE:
        u1.d = *((const double*) v1.sqldata);
        snprintf(convBuf, sizeof(convBuf), "%g", u1.d);
        buf = convBuf;
        break;
    case SQL_D_FLOAT:
        // VAX double?
        u1.d = *((const double*) v1.sqldata);
        snprintf(convBuf, sizeof(convBuf), "%g", u1.d);
        buf = convBuf;
        break;
    case SQL_TIMESTAMP:
        u1.its = ((const ISC_TIMESTAMP*) v1.sqldata);
        buf = DbTimeStamp(
                (const DbTimeStamp::IscTimestamp&) *u1.its).iso8601DateTime();
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
        u1.iquad = ((const ISC_QUAD*) v1.sqldata);
        snprintf(convBuf, sizeof(convBuf), "array %x:%x",
                static_cast<unsigned>(u1.iquad->gds_quad_high),
                static_cast<unsigned>(u1.iquad->gds_quad_low));
        buf = convBuf;
        break;
    case SQL_QUAD:
        u1.iquad = ((const ISC_QUAD*) v1.sqldata);
        snprintf(convBuf, sizeof(convBuf), "%08x:%08x",
                static_cast<unsigned>(u1.iquad->gds_quad_high),
                static_cast<unsigned>(u1.iquad->gds_quad_low));
        buf = convBuf;
        break;
    case SQL_TYPE_TIME:
        u1.itime = ((const ISC_TIME*) v1.sqldata);
        buf = DbTime(*u1.itime).iso8601Time();
        break;
    case SQL_TYPE_DATE:
        u1.idate = ((const ISC_DATE*) v1.sqldata);
        buf = DbDate(*u1.idate).iso8601Date();
        break;
    case SQL_INT64:
        u1.n = *((const ISC_INT64*) v1.sqldata);
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

    if (idx >= (unsigned int) row_->sqld) {
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
    return DbBlob(db_, transaction_, (const FbQuad*) v1.sqldata);
}

DbRowProxy::operator bool() const
{
    return (row_ != nullptr);
}

std::string DbRowProxy::formatDate(unsigned int idx, const std::string &format) const {

    struct tm times;
    const XSQLVAR &v1 = row_->sqlvar[idx];

    switch (v1.sqltype & ~1){
        case SQL_TYPE_DATE:
            isc_decode_sql_date((ISC_DATE *) v1.sqldata, &times);
            break;
        case SQL_TIMESTAMP:
            isc_decode_timestamp((ISC_TIMESTAMP *) v1.sqldata, &times);
            break;
        default:
            throw std::logic_error("Field type is not blob!");
    }

    char buff[30];
    strftime(buff, 30, format.c_str(), &times);

    return buff;
}

DbRowProxy::DbField DbRowProxy::fieldByName(const char* name) {
    if (row_) {
        auto it = fields.find(name);
        if (it == fields.end()) {
            int len = std::strlen(name);
            /*
            std::string Uname;
            int len = std::strlen(name);
            for(int i = 0; i < len; ++i) 
                Uname.append(1, std::toupper(name[i]));*/

            XSQLVAR *v1;
            for (int i = 0; i < row_->sqld; ++i) {
                v1 = &(row_->sqlvar[i]);
                if (v1->sqlname_length == len || v1->aliasname_length == len) {
                    if (std::strcmp(name, v1->sqlname) == 0 || std::strcmp(name, v1->aliasname) == 0) {
                        fields[name] = i;
                        return {this, static_cast<unsigned int>(i)};
                    }
                }
            }
            return {nullptr, 0};
        } else
            return {this, it->second};
    } else
        return {nullptr, 0};
}

DbRowProxy::DbField::DbField(DbRowProxy* rowProxy, unsigned int idx) 
        :rowProxy(rowProxy), idx(idx){}

DbRowProxy::DbField::operator bool() const{
    return rowProxy != nullptr;
}
        
int64_t DbRowProxy::DbField::asInteger() {
    return rowProxy->getInt64(idx);
}

double DbRowProxy::DbField::asDouble() {
    return std::strtod(rowProxy->getText(idx).c_str(), nullptr);
}

std::string DbRowProxy::DbField::asString() {
    return rowProxy->getText(idx);
}

std::string DbRowProxy::DbField::formatDate(const std::string &format) {
    return rowProxy->formatDate(idx, format);
}

} /* namespace fb */
