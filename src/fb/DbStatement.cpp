/*
 * DbStatement.h - execute queries with results or update queries
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

#include "DbStatement.h"

#include "DbBlob.h"
#include "DbRowProxy.h"
#include "DbTransaction.h"
#include "FbException.h"
#include "FbInternals.h"

#include <ibase.h>

#include <cassert>
#include <memory>
#include <string.h>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace fb
{

namespace {

/**
 * how much padding should we add to n so that it is a multiple of
 * blockSize (it aligns on an blockSize byte boundary)
 */
inline size_t pad_to_align(size_t n, size_t blockSize)
{
    size_t r = n % blockSize;
    return r ? (blockSize - r) : 0;
}

/**
 * fields will look like this in the memory:
 * 0-2 short, null field indicator
 * 2-4 short, field length for VARYING fields
 * 4-x padding bytes up to a multiple of 8 bytes
 * x-(x + sqllen) the field data, exception VARYING records start at offset 2
 * @remark the caller must delete [] the returned array
 */
unsigned char *allocateAndSetXsqldaFields(XSQLDA *sqlda)
{
    size_t fsize = 0;
    for (int i = 0; i != sqlda->sqld; ++i) {
        XSQLVAR &v1 = sqlda->sqlvar[i];
        //printf("Type: %d Len: %d, data: %p ind: %p, name: %s\n",
        //        v1.sqltype, v1.sqllen, v1.sqldata, v1.sqlind, v1.sqlname);
        fsize += pad_to_align(fsize, sizeof(ISC_SHORT));
        fsize += (2 * sizeof(ISC_SHORT));
        fsize += pad_to_align(fsize, 8);
        fsize += v1.sqllen;
    }

    unsigned char *fields = new unsigned char[fsize];
    memset(fields, 0, fsize);

    unsigned char *p = fields;

    for (int i = 0; i != sqlda->sqld; ++i) {
        XSQLVAR &v1 = sqlda->sqlvar[i];
        p += pad_to_align((size_t) (p - fields), sizeof(ISC_SHORT));
        v1.sqlind = (ISC_SHORT*) p;
        p += sizeof(ISC_SHORT);
        if ((v1.sqltype & ~1) == SQL_VARYING) {
            v1.sqldata = (ISC_SCHAR*) p;
            // tell the engine we have more space
            v1.sqllen += sizeof(ISC_SHORT);
        } else {
            p += sizeof(ISC_SHORT);
            p += pad_to_align((size_t) (p - fields), 8);
            v1.sqldata = (ISC_SCHAR*) p;
        }
        p += v1.sqllen;
    }
    assert(p <= (fields + fsize));
    return fields;
}

} /* anonymous namespace */

DbStatement::DbStatement(FbApiHandle *db,
                         DbTransaction *tr,
                         const char *sql) :
                            results_(nullptr),
                            fields_(nullptr),
                            inParams_(nullptr),
                            inFields_(nullptr),
                            statement_(0),
                            db_(*db),
                            trans_(tr),
                            ownsTransaction_(tr == nullptr),
                            cursorOpened_(false),
                            statementType_(0)
{
    assert(db);

    /* make sure we delete a transaction we create if the
     * constructor fails with an exception
     */
    std::unique_ptr<DbTransaction> transPtr;

    if (ownsTransaction_) {
        // create and start read-write transaction
        trans_ = new DbTransaction(&db_, 1,
                                   DefaultTransMode::Commit,
                                   TransStartMode::StartReadWrite);
        transPtr.reset(trans_);
    }

    ISC_STATUS_ARRAY status;
    if (isc_dsql_allocate_statement(status, db, &statement_)) {
        throw FbException("Failed to allocate statement.", status);
    }

    results_ = (SqlDescriptorArea*) new char[XSQLDA_LENGTH(1)];
    results_->sqln = 1;
    results_->sqld = 1;
    results_->version = SQLDA_VERSION1;

    // preprocess sql for named parameters
    int i = 0;
    int j = 0;
    int k = 0;
    int cntPar = 0;
    int l = strlen(sql);
    char pSql[l];
    char paramName[32];
    while (sql[i] != '\0') {
        switch (sql[i]) {
            case '?':
                pSql[j++] = sql[i++];
                ++cntPar;
                break;
            case '\'':
                pSql[j++] = sql[i++];
                while (sql[i] != '\0' && sql[i] != '\'')
                    pSql[j++] = sql[i++];
                if (sql[i] == '\0')
                    throw FbException("Preprocessing SQL error.", nullptr);
                else
                    pSql[j++] = sql[i++];
                break;
            case ':':
                k = 0;
                pSql[j++] = '?';
                ++i;
                while (std::isalpha(sql[i]) || std::isdigit(sql[i]) || sql[i] == '_') {
                    paramName[k++] = std::toupper(sql[i++]);
                }
                if (k > 0 && k < 32) {
                    paramName[k] = '\0';
                    namedParameters[paramName] = ++cntPar;
                } else
                    throw FbException("Preprocessing SQL error.", nullptr);
                break;
            default:
                pSql[j++] = sql[i++];
                break;
        }
    }
    pSql[j] = '\0';
    
    if (isc_dsql_prepare(status, trans_->nativeHandle(), &statement_, 0,
                        pSql, (short) FB_SQL_DIALECT, results_)) {
        throw FbException("Failed to prepare statement.", status);
    }

    ISC_SHORT columns = results_->sqld;

    if (columns > results_->sqln) {
        delete [] results_;
        results_ = (SqlDescriptorArea*) new char[XSQLDA_LENGTH(columns)];
        results_->sqln = columns;
        results_->version = SQLDA_VERSION1;
    }

    const char sqlInfoRequest[] = { isc_info_sql_stmt_type };
    char sqlInfoReply[8] = "";
    if (isc_dsql_sql_info(status, &statement_, 1, sqlInfoRequest,
                          sizeof(sqlInfoReply), sqlInfoReply)) {
        throw FbException("Failed to get statement type.", status);
    }

    if (sqlInfoReply[0] != isc_info_sql_stmt_type) {
        throw FbException("Unexpected SQL info reply.", nullptr);
    } else {
        statementType_ = sqlInfoReply[3];
    }

    if (columns != 0 &&
        isc_dsql_describe(status, &statement_, SQLDA_VERSION1, results_)) {
        throw FbException("Failed to describe statement results.", status);
    } else if (columns == 0) {
        // we don't have any results returned by this query
        delete [] results_;
        results_ = nullptr;
    }

    if (results_) {
        // allocate memory to hold field data, and set the data
        // pointers in the output XSQLDA structure
        fields_ = allocateAndSetXsqldaFields(results_);
    }

    // constructor succeeded (until here), release the transaction deleter
    transPtr.release();
}

/** move constructor */
DbStatement::DbStatement(DbStatement &&st)
{
    results_ = st.results_;
    fields_ = st.fields_;
    inParams_ = st.inParams_;
    inFields_ = st.inFields_;
    statement_ = st.statement_;
    db_ = st.db_;
    trans_ = st.trans_;
    ownsTransaction_ = st.ownsTransaction_;
    cursorOpened_ = st.cursorOpened_;
    statementType_ = st.statementType_;
    namedParameters = st.namedParameters;

    st.results_ = nullptr;
    st.fields_ = nullptr;
    st.inParams_ = nullptr;
    st.inFields_ = nullptr;
    st.statement_ = 0;
    st.trans_ = nullptr;
}

/** move assignment */
DbStatement &DbStatement::operator=(DbStatement &&st)
{
    close();
    results_ = st.results_;
    fields_ = st.fields_;
    inParams_ = st.inParams_;
    inFields_ = st.inFields_;
    statement_ = st.statement_;
    db_ = st.db_;
    trans_ = st.trans_;
    ownsTransaction_ = st.ownsTransaction_;
    cursorOpened_ = st.cursorOpened_;
    statementType_ = st.statementType_;
    namedParameters = st.namedParameters;

    st.results_ = nullptr;
    st.fields_ = nullptr;
    st.inParams_ = nullptr;
    st.inFields_ = nullptr;
    st.statement_ = 0;
    st.trans_ = nullptr;

    return *this;
}

DbStatement::~DbStatement()
{
    close();
}

/**
 * deallocate resources
 */
void DbStatement::close()
{
    delete [] results_;
    results_ = nullptr;
    delete [] fields_;
    fields_ = nullptr;
    delete [] inParams_;
    inParams_ = nullptr;
    delete [] inFields_;
    inFields_ = nullptr;

    ISC_STATUS_ARRAY status;
    if (statement_ != 0 &&
        isc_dsql_free_statement(status, &statement_, DSQL_drop)) {
        throw FbException("Failed to allocate statement.", status);
    }
    assert(statement_ == 0);

    if (trans_ && ownsTransaction_) {
        delete trans_;
        trans_ = nullptr;
    }
}

DbStatement::operator bool() const
{
    return (statement_ != 0);
}

int DbStatement::columCount() const
{
    assert(results_);
    if (!results_) {
        return 0;
    }
    return results_->sqld;
}

void DbStatement::createBoundParametersBlock()
{
    assert(!inParams_);
    assert(!inFields_);
    assert(statement_ != 0);

    inParams_ = (SqlDescriptorArea*) new char[XSQLDA_LENGTH(1)];
    inParams_->sqln = 1;
    inParams_->sqld = 1;
    inParams_->version = SQLDA_VERSION1;

    ISC_STATUS_ARRAY status;
    if (isc_dsql_describe_bind(status, &statement_,
                               SQLDA_VERSION1, inParams_)) {
        throw FbException("Failed to describe statement results.", status);
    }

    ISC_SHORT parameters = inParams_->sqld;
    if (parameters > inParams_->sqln) {
        delete [] inParams_;
        inParams_ = (SqlDescriptorArea*) new char[XSQLDA_LENGTH(parameters)];
        inParams_->sqln = parameters;
        inParams_->version = SQLDA_VERSION1;
        if (isc_dsql_describe_bind(status, &statement_,
                                   SQLDA_VERSION1, inParams_)) {
            throw FbException("Failed to describe statement results.", status);
        }
    }

    if (parameters > 0) {
        inFields_ = allocateAndSetXsqldaFields(inParams_);
    }
}

/** idx should be 1 based index */
XSqlVar &DbStatement::getSqlVarCheckIndex(unsigned int idx,
                                          bool resetNullIndicator)
{
    assert(statement_ != 0);
    if(!inParams_) {
        createBoundParametersBlock();
    }

    if (idx == 0 || idx > (unsigned) inParams_->sqld) {
        throw std::out_of_range("statement parameter index is out of range!");
    }

    XSQLVAR &v1 = inParams_->sqlvar[idx - 1];
    if (resetNullIndicator) {
        if ((v1.sqltype & 1) && *v1.sqlind < 0) {
            *v1.sqlind = 0;
        }
    }
    return (XSqlVar&) v1;
}

void DbStatement::setNull(unsigned int idx)
{
    XSQLVAR &v1 = getSqlVarCheckIndex(idx, false);
    assert(v1.sqlind);
    *v1.sqlind = (ISC_SHORT) -1;
}

void DbStatement::setInt(unsigned int idx, int64_t v)
{
    XSQLVAR &v1 = getSqlVarCheckIndex(idx, true);
    switch (v1.sqltype & ~1) {
    case SQL_SHORT:
        *((ISC_SHORT*) v1.sqldata) = v;
        break;
    case SQL_LONG:
        *((ISC_LONG*) v1.sqldata) = v;
        break;
    case SQL_INT64:
        *((ISC_INT64*) v1.sqldata) = v;
        break;
    default:
        throw std::invalid_argument("invalid data type for bound parameter!");
        break;
    }
}

void DbStatement::setDouble(unsigned int idx, double v) {
    XSQLVAR &v1 = getSqlVarCheckIndex(idx, true);
    switch (v1.sqltype & ~1) {
        case SQL_FLOAT:
            *((float*) v1.sqldata) = v;
            break;
        case SQL_DOUBLE:
            *((double*) v1.sqldata) = v;
            break;
        case SQL_SHORT:
            *((int16_t*) v1.sqldata) = (int16_t)floor( v + 0.5);
            break;
        case SQL_LONG:
            *((ISC_LONG*) v1.sqldata) = (ISC_LONG)floor( v + 0.5);
            break;
        case SQL_INT64:
            *((int64_t*) v1.sqldata) = (int64_t)floor( v + 0.5);
            break;
        default:
            throw std::invalid_argument("invalid data type for bound parameter!");
            break;
    }
}

void DbStatement::setText(unsigned int idx,
                          const char *value,
                          int length /* = -1 */)
{
    if (!value) {
        setNull(idx);
        return;
    }

    if (length < 0) {
        length = (int) strlen(value);
    }

    XSQLVAR &v1 = getSqlVarCheckIndex(idx, true);
    FbVarchar *vc;

    switch (v1.sqltype & ~1) {
    case SQL_TEXT:
        if (v1.sqllen < length) {
            length = v1.sqllen;
            memcpy(v1.sqldata, value, length);
        } else {
            memcpy(v1.sqldata, value, length);
            memset(v1.sqldata + length, ' ', v1.sqllen - length);
        }
        break;
    case SQL_VARYING:
        vc = (FbVarchar*) v1.sqldata;
        if ((v1.sqllen - 2) < length) {
            length = v1.sqllen - 2;
        }
        vc->length = length;
        memcpy(vc->str, value, length);
        break;
    default:
        throw std::invalid_argument("invalid data type for bound parameter!");
        break;
    }
}
/**
 * idx is 1 based index
 */
void DbStatement::setBlob(unsigned int idx, const DbBlob &blob)
{
    XSQLVAR &v1 = getSqlVarCheckIndex(idx, true);
    ISC_QUAD *blobId;

    switch (v1.sqltype & ~1) {
    case SQL_BLOB:
        blobId = (ISC_QUAD*) v1.sqldata;
        blobId->gds_quad_high = blob.getBlobId().quad_high;
        blobId->gds_quad_low = blob.getBlobId().quad_low;
        break;
    default:
        throw std::invalid_argument("invalid data type for bound parameter!");
        break;
    }
}


void DbStatement::execute()
{
    assert(statement_ != 0);
    ISC_STATUS_ARRAY status;
    ISC_STATUS rc;

    if (statementType_ == isc_info_sql_stmt_select) {
        rc = isc_dsql_execute(status, trans_->nativeHandle(), &statement_,
                              1, inParams_);
    } else {
        rc = isc_dsql_execute2(status, trans_->nativeHandle(), &statement_,
                              1, inParams_, results_);
    }

    if (rc != 0) {
        throw FbException("Failed to execute statement.", status);
    }
}

void DbStatement::reset()
{
    ISC_STATUS_ARRAY status;
    if (cursorOpened_ &&
        statement_ != 0 &&
        isc_dsql_free_statement(status, &statement_, DSQL_close)) {
        throw FbException("Failed to free statement cursor.", status);
    }

    if (cursorOpened_) {
        cursorOpened_ = false;
    }
}

DbRowProxy DbStatement::uniqueResult()
{
    Iterator i = iterate();
    if (i != end()) {
        return *i;
    }
    return DbRowProxy(nullptr, 0, 0);
}

DbStatement::Iterator DbStatement::iterate()
{
    // this function must be called at most once per statement
    if (statement_ == 0) {
        return Iterator(nullptr);
    }
    return Iterator(this);
}

DbStatement::Iterator DbStatement::end() const
{
    return Iterator(nullptr);
}

DbStatement::Iterator::Iterator(DbStatement *s) : st_(s)
{
    if (!s) {
        return;
    }

    st_->execute();

    if (st_->statementType_ != isc_info_sql_stmt_select) {
        // no fetch required, isc_dsql_execute2 got the results into results_
        return;
    }

    ISC_STATUS_ARRAY status;
    ISC_STATUS rc = isc_dsql_fetch(status, &st_->statement_,
                                    1, st_->results_);
    if (rc != 0) {
        // we reached the end or an error occurred
        // rc == 100 means we reached the end of the cursor
        if (rc != 100l) {
            st_ = nullptr;
            throw FbException("Failed to fetch from statement.", status);
        } else {
            st_->cursorOpened_ = true;
            st_ = nullptr;
        }
    }

    if (st_) {
        st_->cursorOpened_ = true;
    }
}

DbStatement::Iterator::Iterator(Iterator &&it) : st_(it.st_)
{
    it.st_ = nullptr;
}

DbStatement::Iterator &DbStatement::Iterator::operator++()
{
    assert(st_ != 0);

    if (st_->statementType_ != isc_info_sql_stmt_select) {
        // we reached the end
        st_ = nullptr;
        return *this;
    }

    ISC_STATUS_ARRAY status;
    ISC_STATUS rc = isc_dsql_fetch(status, &st_->statement_,
                                   1, st_->results_);
    if (rc != 0) {
        // we reached the end or an error occurred
        // rc == 100 means we reached the end of the cursor
        st_ = nullptr;
        if (rc != 100l) {
            throw FbException("Failed to fetch from statement.", status);
        }
    }
    return *this;
}

bool DbStatement::Iterator::operator!=(const DbStatement::Iterator &other) const
{
    return st_ != other.st_;
}

DbRowProxy DbStatement::Iterator::operator*()
{
    assert(st_ != 0);
    return DbRowProxy(st_->results_,
                      st_->db_,
                      *st_->trans_->nativeHandle());
}

StParameterPtr DbStatement::paramByName(const std::string& name) {
    std::string Uname = name;
    std::transform(Uname.begin(), Uname.end(), Uname.begin(),
            [](unsigned char c) {
                return std::toupper(c); }
    );
    auto it = namedParameters.find(Uname);
    if (it == namedParameters.end())
        throw FbException("Could not find matching parameter.", nullptr);

    return std::make_unique<StParameter>(this, it->second);
}

} /* namespace fb */
