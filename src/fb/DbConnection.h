/*
 * DbConnection.h - connection to a single Firebird database
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

#ifndef DBWRAP_FB_SRC_DBCONNECTION_H_
#define DBWRAP_FB_SRC_DBCONNECTION_H_

#include "FbCommon.h"

#include <mutex>
#include <string>
#include <vector>


namespace fb
{

// forward declarations
class DbTransaction;
class DbStatement;

/**
 * used defined database events callback function set by `enableEvents`
 *
 * \param callbackData user defined callback data, it is the same value as the
 *  second argument passed to `enableEvents`
 * \param eventName the name of the event that was triggered, represented as a
 *  null terminated string
 * \param eventCount how many times the event was triggered
 * \remark event callback support is experimental
 */
using EventCallback = void (*)(void *callbackData, const char *eventName, int eventCount);

struct DbObjectInfo
{
    const char *name;
    const char *type;
    const char *sql;
};

struct DbCreateOptions
{
    /** page size should be 1024, 2048, 4096, 8192 or 16384 */
    int const page_size_;

    /**
     * synchronous or asynchronous writes (1 or 0)
     *
     * synchronous writes are safer but slower. forced_writes_
     * should be set to 1 for synchronous writes or to 0 otherwise.
     */
    short const forced_writes_;

    /*
     * try to create database if it does not exist
     */
    bool try_create_db_;

    DbObjectInfo const * const db_schema_;

    /** Initialise the create options with sensible defaults. */
    explicit DbCreateOptions(int page_size = 8192,
                             bool forced_writes = false,
                             const DbObjectInfo *initial_schema = nullptr)
              : page_size_(page_size),
                forced_writes_(forced_writes ? 1 : 0),
                try_create_db_(true),
                db_schema_(initial_schema)
    {
    }
};


class DbConnection final
{
public:
    // all these methods may throw FbException in case of an error
    DbConnection(const char *dbName,
                 const char *server = nullptr,
                 const char *userName = nullptr,
                 const char *userPassword = nullptr,
                 const DbCreateOptions *opts = nullptr);

    ~DbConnection();

    void executeUpdate(const char *updateSql,
                       DbTransaction *transaction = nullptr);

    DbStatement createStatement(const char *query,
                                DbTransaction *transaction = nullptr);

    const FbApiHandle *nativeHandle() const;

    /** event handling is experimental, use at own risk */
    void enableEvents(EventCallback callback, void *callbackData,
            const std::vector<std::string> &eventNames);
    void disableEvents();

private:
    DbConnection(const DbConnection&) = delete;
    DbConnection& operator=(const DbConnection&) = delete;

    bool connect(const char *dbName, const char *server,
                 const char *userName, const char *userPassword,
                 const DbCreateOptions *opts);
    bool dissconnect();

    std::mutex connectMutex_;
    FbApiHandle db_; /** database handle isc_db_handle a.k.a unsigned int */

    struct EventSettings;
    EventSettings *eventSettings_; /** event settings if enabled, otherwise null */
};

} /* namespace fb */

#endif /* DBWRAP_FB_SRC_DBCONNECTION_H_ */
