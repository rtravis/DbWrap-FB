/*
 * DbConnection.cpp - connection to a single Firebird database
 *
 *
 * This is part of the "DbWrap++ for Firebird" (DbWrap++FB)
 * C++ library for accessing Firebird databases in your C++11
 * program.
 *
 * @created: Dec 27, 2014
 *
 * @copyright: Copyright (c) 2015 Robert Zavalczki, distributed
 * under the terms and conditions of the Lesser GNU General
 * Public License version 2.1
 */

#include "DbConnection.h"

#include "DbStatement.h"
#include "DbTransaction.h"
#include "FbException.h"

#include <ibase.h>

#include <cassert>
#include <cstring>
#include <memory>
#include <string>



namespace fb {
    
static DBConnectionParameters connectionParameters_;

bool DbConnection::resolveParametersByName = false;

void DbConnection::initConnectionParameters(DBConnectionParameters &connectionParameters) {
            connectionParameters_ = connectionParameters;
}


DbConnection::DbConnection(){
    db_ = 0;

    // check some static assertions
    static_assert(sizeof(db_) == sizeof(isc_db_handle),
                "Invalid size for DB handle!");

    connect(
        connectionParameters_.database.c_str(), 
        connectionParameters_.hostname.c_str(),
        connectionParameters_.username.c_str(), 
        connectionParameters_.password.c_str(), 
        connectionParameters_.charset.c_str(),
        nullptr
            );
}

DbConnection::DbConnection(const char *dbName,
                           const char *server,
                           const char *userName,
                           const char *userPassword,
                           const char *charset,
                           const DbCreateOptions *opts) : db_(0)
{
    // check some static assertions
    static_assert(sizeof(db_) == sizeof(isc_db_handle),
                "Invalid size for DB handle!");

    connect(dbName, server, userName, userPassword, charset,  opts);
}

DbConnection::~DbConnection()
{
    dissconnect();
}

bool DbConnection::connect(const char *dbName, const char *server,
                           const char *userName, const char *userPassword, 
                           const char *charset,
                           const DbCreateOptions *opts)
{
    if (db_ != 0) {
        dissconnect();
    }

    static const DbCreateOptions defaultOptions;
    if (!opts) {
        opts = &defaultOptions;
    }

    std::string connectionString(server ? server : dbName);
    if (server) {
        connectionString += ":";
        connectionString += dbName;
    }

    std::lock_guard<std::mutex> const lg(connectMutex_);

    /*
     *    Create a new database.
     *    The database handle is zero.
     */
    ISC_STATUS_ARRAY status;    /* status vector */
    long sqlcode;
    ISC_STATUS rc;

    std::string spb_buffer;

    spb_buffer.append(1, (char) isc_dpb_version1);


    int sqlDialect = FB_SQL_DIALECT;
    spb_buffer.append(1, (char) isc_dpb_sql_dialect);
    spb_buffer.append(1, (char) sizeof(int));
    spb_buffer.append((const char*) &sqlDialect, sizeof(int));

    static_assert(sizeof(opts->forced_writes_) == sizeof(short),
                        "Invalid size for forced_writes_!");

    spb_buffer.append(1, (char) isc_dpb_force_write);
    spb_buffer.append(1, (char) sizeof(short));
    spb_buffer.append((const char*) &opts->forced_writes_, sizeof(short));

    static_assert(sizeof(opts->page_size_) == sizeof(int),
                        "Invalid size for page size!");

    spb_buffer.append(1, (char) isc_dpb_page_size);
    spb_buffer.append(1, (char) sizeof(int));
    spb_buffer.append((const char*) &opts->page_size_, sizeof(int));

    if (userName) {
        size_t n = strlen(userName);
        if (n < 128) {
            spb_buffer.append(1, (char) isc_dpb_user_name);
            spb_buffer.append(1, (char) n);
            spb_buffer.append(userName, n);
        }

        if (userPassword) {
            n = strlen(userPassword);
            if (n < 128) {
                spb_buffer.append(1, (char) isc_dpb_password);
                spb_buffer.append(1, (char) n);
                spb_buffer.append(userPassword, n);
            }
        }
    } else {
        // no user and password, trusted authorisation, maybe an embedded database
        spb_buffer.append(1, (char) isc_dpb_trusted_auth);
        spb_buffer.append(1, (char) 1); // size = 1
        spb_buffer.append(1, (char) 1); // value = 1 (yes)
    }
    
    if(charset) {
        size_t n = strlen(charset);
        if (n < 128) {
            spb_buffer.append(1, (char) isc_dpb_lc_ctype);
            spb_buffer.append(1, (char) n);
            spb_buffer.append(charset, n);
        }
    }
    
    /*
     * Try to connect to an existing database
     */
    rc = isc_attach_database(status, 0, connectionString.c_str(), &db_,
                             (short) spb_buffer.size(), spb_buffer.data());

    if (rc != 0) {
        sqlcode = isc_sqlcode(status);
        // -902 in this context means database does not exist
        if (sqlcode != -902 || !opts->tryToCreateDb_) {
            throw FbException("attach database", status);
        }
    } else {
        // we successfully attached to the database
        return true;
    }

    assert(opts->tryToCreateDb_ && rc != 0 && sqlcode == -902);

    // try to create database if it does not exist
    std::string createDbSql = "CREATE DATABASE '";
    createDbSql.append(connectionString).append("' ");

    if (userName) {
        createDbSql.append("USER '").append(userName).append("' ");
        if (userPassword) {
            createDbSql.append("PASSWORD '").append(userPassword).append("' ");
        }
    }

    createDbSql.append("PAGE_SIZE=").append(std::to_string(opts->page_size_));
    if (charset)
        createDbSql.append(" DEFAULT CHARACTER SET ").append(charset);
    
    createDbSql.append(";");

    isc_tr_handle dbTransaction = 0;
    rc = isc_dsql_execute_immediate(status, &db_, &dbTransaction,
                                    0, createDbSql.c_str(),
                                    FB_SQL_DIALECT, NULL);

    if (rc != 0) {
        throw FbException("create database", status);
    }

    if (opts->db_schema_) {
        // creating initial schema
        DbTransaction tr1(&db_, 1,
                          DefaultTransMode::Rollback,
                          TransStartMode::StartReadWrite);
        try {
            for (const DbObjectInfo *i = opts->db_schema_; i->name; ++i) {
                executeUpdate(i->sql, &tr1);
            }
            tr1.commit();
        } catch (...) {
            throw;
        }
    }

    return true;
}

bool DbConnection::dissconnect()
{
    ISC_STATUS_ARRAY status;
    std::lock_guard<std::mutex> const lg(connectMutex_);
    /* db_handle will be set to null on success */
    if (isc_detach_database(status, &db_)) {
        throw FbException("detach database", status);
    }
    assert(db_ == 0);
    return true;
}

const FbApiHandle *DbConnection::nativeHandle() const
{
    return db_ ? &db_ : nullptr;
}

/**
 * If transaction is null then a new one is created and committed.
 * else the caller is responsible for committing or rolling
 * back the transaction.
 */
void DbConnection::executeUpdate(const char *updateSql,
                                 DbTransaction *transaction /* = nullptr */)
{
    if (db_ == 0) {
        throw FbException("No database connection!", nullptr);
    }

    std::unique_ptr<DbTransaction> trPtr;

    assert(updateSql);
    if (transaction == nullptr) {
        transaction = new DbTransaction(&db_, 1,
                                        DefaultTransMode::Rollback,
                                        TransStartMode::StartReadWrite);
        trPtr.reset(transaction);
    }

    ISC_STATUS_ARRAY status;
    if (isc_dsql_execute_immediate(status, &db_, transaction->nativeHandle(),
                                   0, updateSql, FB_SQL_DIALECT, NULL)) {
        throw FbException("update/create/insert statement failed!", status);
    }

    if (trPtr) {
        trPtr->commit();
    }
}

DbStatement DbConnection::createStatement(const char *query,
                                    DbTransaction *transaction /* = nullptr */)
{
    return DbStatement(&db_, transaction, query);
}

} /* namespace fb */
