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

#ifndef DBWRAP__FB_SRC_DBSTATEMENT_H_
#define DBWRAP__FB_SRC_DBSTATEMENT_H_
#include "FbCommon.h"
#include <cstdint>

#include <memory>
#include <unordered_map>
#include <typeinfo>
#include <typeindex>

namespace fb
{

// forward declarations
class DbRowProxy;
class DbTransaction;
class DbBlob;
class StParameter;

using StParameterPtr = std::unique_ptr<StParameter>;

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

    /** idx is 1 based index */
    void setDouble(unsigned int idx, double value);
    
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

    StParameterPtr paramByName(const std::string& name);
private:
    std::unordered_map<std::string, int> namedParameters;
    
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

class StParameter {
    //friend DbStatement;
private:
    DbStatement* st_;
    unsigned int idx_;
public:
    StParameter(DbStatement* st, unsigned int idx): st_(st), idx_(idx){};

    void setValue(short int v) { st_->setInt(idx_, v); };
    void setValue(unsigned short int v) { st_->setInt(idx_, v); };
    void setValue(int v) { st_->setInt(idx_, v); };
    void setValue(unsigned int v) { st_->setInt(idx_, v); };
    void setValue(long int v) { st_->setInt(idx_, v); };
    void setValue(unsigned long int v) { st_->setInt(idx_, v); };
    void setValue(long long int v) { st_->setInt(idx_, v); };
    void setValue(unsigned long long int v) { st_->setInt(idx_, v); };


    void setValue(float v)   { st_->setDouble(idx_, v); };
    void setValue(double v)  { st_->setDouble(idx_, v); };
    void setValue(const char* v){ st_->setText(idx_, v); };
    void setValue(const DbBlob &v) { st_->setBlob(idx_, v); };
    
    void setInt(int64_t v){
        st_->setInt(idx_, v);
    }
    void setDouble(double v){
        st_->setDouble(idx_, v);
    }
    void setText(const char* v){
        st_->setText(idx_, v);
    }
    void setNull(){
        st_->setNull(idx_);
    }
    void setBlob(const DbBlob &v){
        st_->setBlob(idx_, v);
    } 
};

} /* namespace fb */

#endif /* DBWRAP__FB_SRC_DBSTATEMENT_H_ */
