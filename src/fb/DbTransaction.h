/*
 * DbTransaction.h - database operations are made in transactions,
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

#ifndef DBWRAP__FB_SRC_FB_DBTRANSACTION_H_
#define DBWRAP__FB_SRC_FB_DBTRANSACTION_H_
#include "FbCommon.h"
#include <vector>

namespace fb
{

enum class DefaultTransMode : char
{
    Commit = 0,
    Rollback
};

enum class TransStartMode : char
{
    DeferStart = 0,
    StartReadOnly,
    StartReadWrite,
};

class DbTransaction
{
public:
    DbTransaction(const FbApiHandle *databases,
                  unsigned int dbCount,
                  DefaultTransMode defaultMode = DefaultTransMode::Commit,
                  TransStartMode startMode = TransStartMode::StartReadWrite);
    ~DbTransaction();

    void start(bool readOnly = false);
    void commit();
    void commitRetain();
    void rollback();
    void rollbackRetain();

    FbApiHandle *nativeHandle();

private:
    typedef std::vector<FbApiHandle> DbSet;
    DbSet dbs_;
    FbApiHandle transaction_;
    DefaultTransMode transMode_;
};

} /* namespace fb */

#endif /* DBWRAP__FB_SRC_FB_DBTRANSACTION_H_ */
