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

#ifndef DBWRAP_FB_SRC_DBSTATEMENT_H_
#define DBWRAP_FB_SRC_DBSTATEMENT_H_
#include "FbCommon.h"
#include <cstdint>


namespace fb
{

// forward declarations
class DbRowProxy;
class DbTransaction;
class DbBlob;

class DbStatement
{
public:
    friend class DbConnection;

    class Iterator
    {
        friend class DbStatement;
        public:
            Iterator(Iterator &&it);
            ~Iterator() = default;


            // C++ like forward iterator interface
            Iterator &operator++();
            bool operator!=(const Iterator &other) const;
            DbRowProxy operator*();

        private:
            Iterator(DbStatement *s);
            DbStatement *st_;
    };

    ~DbStatement();
    DbStatement(DbStatement &&st);
    DbStatement &operator=(DbStatement &&st);

    /** is this statement valid ? */
    explicit operator bool() const;

    void close();
    int columCount() const;

    /**
     * set field value as null
     * \param idx is the 1 based index of the field
     */
    void setNull(unsigned int idx);

    /** idx is 1 based index */
    void setInt(unsigned int idx, int64_t v);

    /**
     * idx is 1 based index
     * if length is negative strlen is used to determine the length
     */
    void setText(unsigned int idx, const char *value, int length = -1);

    /**
     * idx is 1 based index
     */
    void setBlob(unsigned int idx, const DbBlob &blob);

    void execute();
    void reset();
    Iterator iterate();
    Iterator end() const;
    DbRowProxy uniqueResult();

private:
    DbStatement(FbApiHandle *db, DbTransaction *tr, const char *sql);

    void createBoundParametersBlock();
    XSqlVar &getSqlVarCheckIndex(unsigned int idx, bool resetNullIndicator);


    // disable copying
    DbStatement(const DbStatement&) = delete;
    DbStatement &operator=(const DbStatement&) = delete;

    /** output column descriptions, pointer to XSQLDA */
    SqlDescriptorArea *results_;
    /** buffer to hold result field values in the XSQLDA */
    unsigned char *fields_;
    /** SQL ('?') parameters bound to the statement, pointer to XSQLDA */
    SqlDescriptorArea *inParams_;
    /** buffer to hold input (bound) parameters in the XSQLDA */
    unsigned char *inFields_;
    /** statement handle */
    FbApiHandle statement_;
    /** database handle */
    FbApiHandle db_;
    /** current transaction handle */
    DbTransaction *trans_;
    bool ownsTransaction_;
    bool cursorOpened_;
    /** one of the "isc_info_sql_stmt_*" values */
    char statementType_;
};

} /* namespace fb */

#endif /* DBWRAP_FB_SRC_DBSTATEMENT_H_ */
