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

DbConnection::DbConnection(const char *dbName, const char *server,
        const char *userName, const char *userPassword,
        const DbCreateOptions *opts) :
        connectMutex_(), db_(0), eventSettings_(nullptr)
{
    // check some static assertions
    static_assert(sizeof(db_) == sizeof(isc_db_handle),
                "Invalid size for DB handle!");

    connect(dbName, server, userName, userPassword, opts);
}

DbConnection::~DbConnection()
{
    disableEvents();
    dissconnect();
}

bool DbConnection::connect(const char *dbName, const char *server,
                           const char *userName, const char *userPassword,
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
    ISC_STATUS_ARRAY status; /* status vector */
    long sqlcode;
    ISC_STATUS rc;

    std::string spb_buffer;

    spb_buffer.append(1, static_cast<char>(isc_dpb_version1));


    int sqlDialect = FB_SQL_DIALECT;
    spb_buffer.append(1, static_cast<char>(isc_dpb_sql_dialect));
    spb_buffer.append(1, static_cast<char>(sizeof(int)));
    spb_buffer.append(reinterpret_cast<const char*>(&sqlDialect), sizeof(int));

    static_assert(sizeof(opts->forced_writes_) == sizeof(short),
                        "Invalid size for forced_writes_!");

    spb_buffer.append(1, static_cast<char>(isc_dpb_force_write));
    spb_buffer.append(1, static_cast<char>(sizeof(short)));
    spb_buffer.append(reinterpret_cast<const char*>(&opts->forced_writes_), sizeof(short));

    static_assert(sizeof(opts->page_size_) == sizeof(int),
                        "Invalid size for page size!");

    spb_buffer.append(1, static_cast<char>(isc_dpb_page_size));
    spb_buffer.append(1, static_cast<char>(sizeof(int)));
    spb_buffer.append(reinterpret_cast<const char*>(&opts->page_size_), sizeof(int));

    if (userName) {
        size_t n = strlen(userName);
        if (n < 128) {
            spb_buffer.append(1, static_cast<char>(isc_dpb_user_name));
            spb_buffer.append(1, static_cast<char>(n));
            spb_buffer.append(userName, n);
        }

        if (userPassword) {
            n = strlen(userPassword);
            if (n < 128) {
                spb_buffer.append(1, static_cast<char>(isc_dpb_password));
                spb_buffer.append(1, static_cast<char>(n));
                spb_buffer.append(userPassword, n);
            }
        }
    } else {
        // no user and password, trusted authorisation, maybe an embedded database
        spb_buffer.append(1, static_cast<char>(isc_dpb_trusted_auth));
        spb_buffer.append(1, static_cast<char>(1)); // size = 1
        spb_buffer.append(1, static_cast<char>(1)); // value = 1 (yes)
    }

    /*
     * Try to connect to an existing database
     */
    rc = isc_attach_database(status, 0, connectionString.c_str(), &db_,
                             static_cast<short>(spb_buffer.size()), spb_buffer.data());

    if (rc != 0) {
        sqlcode = isc_sqlcode(status);
        // -902 in this context means database does not exist
        if (sqlcode != -902 || !opts->try_create_db_) {
            throw FbException("attach database", status);
        }
    } else {
        // we successfully attached to the database
        return true;
    }

    assert(opts->try_create_db_ && rc != 0 && sqlcode == -902);

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
    createDbSql.append(";");

    isc_tr_handle dbTransaction = 0;
    rc = isc_dsql_execute_immediate(status, &db_, &dbTransaction,
                                    0, createDbSql.c_str(),
                                    FB_SQL_DIALECT, nullptr);

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
                                   0, updateSql, FB_SQL_DIALECT, nullptr)) {
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

// = = = = = = = = = BEGIN PETE SHEW event callback support  = = = = = = = = =
// December 2018 modifications by Pete Shew pete@shew.org

struct DbConnection::EventSettings
{
    EventCallback event_callback_;
    void *event_callback_data_;
    ISC_UCHAR *event_buffer_;
    ISC_UCHAR *result_buffer_;
    short event_buffer_length_;
    bool destroy_called_;
    ISC_LONG event_id_;
    FbApiHandle db_;
    std::vector<std::string> event_names_;
    std::mutex callbackMutex_;

    EventSettings(FbApiHandle db, EventCallback callback, void *callbackData,
            const std::vector<std::string> &names) :
            event_callback_(callback), event_callback_data_(callbackData),
            event_buffer_(nullptr), result_buffer_(nullptr),
            event_buffer_length_(0), destroy_called_(false), event_id_(0),
            db_(db), event_names_(names), callbackMutex_()
    {
        if (names.empty()) {
            // we've nothing to do
            return;
        } else if (names.size() > 15) {
            throw std::runtime_error(
                    "Invalid argument! At most 15 events can be watched.");
        }

        assert(event_names_.size() <= 15);
        std::array<const char*, 15> nl;
        size_t idx = 0;

        for (; idx < event_names_.size(); ++idx) {
            nl[idx] = event_names_[idx].c_str();
        }

        for (; idx < nl.size(); ++idx) {
            nl[idx] = nullptr;
        }

        // All attempts to pass a va_list failed so using a brute force approach
        event_buffer_length_ = static_cast<short>(isc_event_block(
                &event_buffer_, &result_buffer_,
                static_cast<ISC_USHORT>(event_names_.size()), nl[0], nl[1],
                nl[2], nl[3], nl[4], nl[5], nl[6], nl[7], nl[8], nl[9], nl[10],
                nl[11], nl[12], nl[13], nl[14]));

        if (event_buffer_length_ == 0) {
            throw std::bad_alloc();
        }

        ISC_STATUS_ARRAY status_vector;
        // Enable the trigger (passing our instantiation for use in the static callback)
        ISC_STATUS status = isc_que_events(status_vector, &db_, &event_id_,
                event_buffer_length_, event_buffer_, event_callback_function,
                this);

        if (status != 0) {
            isc_free(reinterpret_cast<ISC_SCHAR*>(event_buffer_));
            isc_free(reinterpret_cast<ISC_SCHAR*>(result_buffer_));
            throw FbException("isc_que_events failed", status_vector);
        }
    }

    ~EventSettings()
    {
        callbackMutex_.lock();
        destroy_called_ = true;
        callbackMutex_.unlock();

        if (event_id_) {
            ISC_STATUS_ARRAY status_vector;
            // notice that we're ignoring the return code
            isc_cancel_events(status_vector, &db_, &event_id_);
        }

        std::lock_guard<std::mutex> lg(callbackMutex_);
        if (event_buffer_) {
            isc_free(reinterpret_cast<ISC_SCHAR*>(event_buffer_));
        }

        if (result_buffer_) {
            isc_free(reinterpret_cast<ISC_SCHAR*>(result_buffer_));
        }
    }

    static void event_callback_function(void* me, ISC_USHORT length,
            const ISC_UCHAR *updated)
    {
        EventSettings &eventSettings = *static_cast<EventSettings*>(me);
        std::lock_guard<std::mutex> lg(eventSettings.callbackMutex_);
        if (eventSettings.destroy_called_) {
            return;
        }

        assert(!eventSettings.event_names_.empty());
        // If we have a callback defined
        if (!eventSettings.event_callback_) {
            return;
        }

        // Copy the new information
        memcpy(eventSettings.result_buffer_, updated, length);

        ISC_ULONG counts[15 + 1];
        memset(&counts, 0, sizeof(counts));
        isc_event_counts(counts, eventSettings.event_buffer_length_,
                eventSettings.event_buffer_, eventSettings.result_buffer_);

        // iterate the registered events
        for (size_t i = 0; i < eventSettings.event_names_.size(); ++i) {
            // if this event has new trigger(s), call the user's callback
            if (counts[i]) {
                eventSettings.event_callback_(
                        eventSettings.event_callback_data_,
                        eventSettings.event_names_[i].c_str(),
                        static_cast<int>(counts[i]));
            }
        }

        // After being called we need to reset the trigger
        ISC_STATUS_ARRAY status_vector;
        ISC_STATUS status = isc_que_events(status_vector, &eventSettings.db_,
                &eventSettings.event_id_, eventSettings.event_buffer_length_,
                eventSettings.event_buffer_, &event_callback_function, me);

        if (status != 0) {
            throw FbException("isc_que_events failed", status_vector);
        }
    }
};

void DbConnection::enableEvents(EventCallback callback, void *callbackData,
        const std::vector<std::string> &eventNames)
{
    std::lock_guard<std::mutex> const lg(connectMutex_);
    delete eventSettings_;
    eventSettings_ = new DbConnection::EventSettings(db_, callback,
            callbackData, eventNames);
}

void DbConnection::disableEvents()
{
    std::lock_guard<std::mutex> const lg(connectMutex_);
    if (eventSettings_) {
        delete eventSettings_;
        eventSettings_ = nullptr;
    }
}
// = = = = = = = = = END PETE SHEW event callback support  = = = = = = = = =

} /* namespace fb */
