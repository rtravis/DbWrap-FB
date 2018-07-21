/*
 * DbTransaction.cpp - database operations are made in transactions,
 * a transaction can span multiple databases
 *
 * This is part of the "DbWrap++ for Firebird" (DbWrap++FB)
 * C++ library for accessing Firebird databases in your C++11
 * program.
 *
 * @created: Jan 3, 2015
 *
 * @copyright: Copyright (c) 2015 Robert Zavalczki, distributed
 * under the terms and conditions of the Lesser GNU General
 * Public License version 2.1
 */

#include "DbTransaction.h"
#include <ibase.h>
#include <stdexcept>
#include "FbException.h"
#include <cassert>

namespace fb {

DbTransaction::DbTransaction(
                const FbApiHandle *databases,
                unsigned int dbCount,
                DefaultTransMode defaultMode /*= DefaultTransMode::Commit*/,
                TransStartMode startMode /*= TransStartMode::StartReadWrite*/) :
                dbs_(databases, databases + dbCount),
                transaction_(0),
                transMode_(defaultMode)
{
    switch (startMode) {
        case TransStartMode::StartReadOnly:
            start(true);
            break;
        case TransStartMode::StartReadWrite:
            start(false);
            break;
        case TransStartMode::DeferStart:
            break;
    }
}

DbTransaction::~DbTransaction()
{
    if (transaction_ == 0) {
        // not started
        return;
    }

    switch (transMode_) {
        case DefaultTransMode::Rollback:
            rollback();
            // [[fallthrough]];
            // fall through
        case DefaultTransMode::Commit:
            commit();
            break;
    }
}

// read: http://www.devrace.com/en/fibplus/articles/3292.php
void DbTransaction::start(bool readOnly /* = false */)
{
    if (transaction_ != 0) {
        throw std::logic_error("Can't start a transaction that is already started!");
    }

    char isc_tpb[] = {
            isc_tpb_version3,
            static_cast<char>((readOnly ? isc_tpb_read : isc_tpb_write)),
            isc_tpb_read_committed,
            isc_tpb_no_rec_version, // isc_tpb_rec_version
            isc_tpb_wait

            // isc_tpb_concurrency,
            // isc_tpb_shared,
            // isc_tpb_wait,
            // isc_tpb_lock_write
    };

    struct  ISC_TEB
    {
        const ISC_LONG *db_ptr;
        ISC_LONG tpb_len;
        const char *tpb_ptr;

        ISC_TEB(const FbApiHandle &db, size_t tpbLen, const char *tpb) :
                    db_ptr(const_cast<ISC_LONG*>(reinterpret_cast<const ISC_LONG*>(&db))),
                    tpb_len(static_cast<ISC_LONG>(tpbLen)),
                    tpb_ptr(tpb)
        {
        }
    };

    std::vector<ISC_TEB> dbInfo;
    for (DbSet::const_iterator i = dbs_.begin(); i != dbs_.end(); ++i) {
        const FbApiHandle &hdb = *i;
        if (hdb == 0) {
            throw std::logic_error("All databases of a transaction must be connected.");
        }
        dbInfo.emplace_back(hdb, sizeof(isc_tpb), isc_tpb);
    }

    ISC_STATUS_ARRAY status;
    if (isc_start_multiple(status, &transaction_,
                           static_cast<short>(dbInfo.size()), &dbInfo[0])) {
        throw FbException(
                "Failed to start transaction (isc_start_multiple)", status);
    }
}

void DbTransaction::commit()
{
    if (transaction_ == 0) {
        // a transaction was not started
        return;
    }

    ISC_STATUS_ARRAY status;
    if (isc_commit_transaction(status, &transaction_)) {
        throw FbException("failed to commit transaction!", status);
    }
    assert(transaction_ == 0);
}

void DbTransaction::commitRetain()
{
    if (transaction_ == 0) {
        // a transaction was not started
        return;
    }

    ISC_STATUS_ARRAY status;
    if (isc_commit_retaining(status, &transaction_)) {
        throw FbException("failed to commit transaction!", status);
    }
    assert(transaction_ != 0);
}

void DbTransaction::rollback()
{
    if (transaction_ == 0) {
        // a transaction was not started
        return;
    }

    ISC_STATUS_ARRAY status;
    if (isc_rollback_transaction(status, &transaction_)) {
        throw FbException("failed to rollback transaction!", status);
    }
    assert(transaction_ == 0);
}

void DbTransaction::rollbackRetain()
{
    if (transaction_ == 0) {
        // a transaction was not started
        return;
    }

    ISC_STATUS_ARRAY status;
    if (isc_rollback_retaining(status, &transaction_)) {
        throw FbException("failed to rollback transaction!", status);
    }
    assert(transaction_ != 0);
}

FbApiHandle *DbTransaction::nativeHandle()
{
    return transaction_ ? &transaction_ : nullptr;
}

} /* namespace fb */
