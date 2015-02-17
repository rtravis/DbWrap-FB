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
#include <ibase.h>
#include "FbException.h"
#include <cassert>
#include "DbTransaction.h"
#include <memory>
#include "DbStatement.h"

namespace fb {

DbConnection::DbConnection(const char *dbFile,
                           const DbCreateOptions *opts) : db_(0)
{
    // check some static assertions
    static_assert(sizeof(db_) == sizeof(isc_db_handle),
                "Invalid size for DB handle!");

    connect(dbFile, opts);
}

DbConnection::~DbConnection()
{
    dissconnect();
}

bool DbConnection::connect(const char *dbFile, const DbCreateOptions *opts)
{
    if (db_ != 0) {
        dissconnect();
    }

    const DbCreateOptions defaultOptions;
    if (!opts) {
        opts = &defaultOptions;
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

    if (isc_create_database(status, 0, dbFile,
        &db_, spb_buffer.size(), spb_buffer.data(), 0)) {
        sqlcode = isc_sqlcode(status);
        // -902 means database already exists
        if (sqlcode != -902) {
            throw FbException("create database", status);
        }
    } else {
        // printf("Created database: '%s'.\n", dbFile);
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

    /*
     * Connect to the existing database
     */
    rc = isc_attach_database(status, 0, dbFile, &db_, 0, nullptr);
    if (rc != 0) {
        throw FbException("attach database", status);
    } else {
        printf("Attached to database: '%s'.\n", dbFile);
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
