/*
 * DbBlob.h - proxy for blob objects
 *
 *
 * This is part of the "DbWrap++ for Firebird" (DbWrap++FB)
 * C++ library for accessing Firebird databases in your C++11
 * program.
 *
 * @created: Jan 18, 2015
 *
 * @copyright: Copyright (c) 2015 Robert Zavalczki, distributed
 * under the terms and conditions of the Lesser GNU General
 * Public License version 2.1
 */

#ifndef DBWRAP__FB_DB_BLOB_H_
#define DBWRAP__FB_DB_BLOB_H_

#include "FbCommon.h"
#include <string>

namespace fb
{

class DbBlob
{
    friend class DbRowProxy;
public:
    /** create a new, write only blob */
    DbBlob(FbApiHandle db, FbApiHandle trans);
    DbBlob(DbBlob &&b);
    ~DbBlob();

    void close();
    void cancel();

    /** test if this is a valid blob */
    explicit operator bool() const;
    const FbQuad &getBlobId() const;

    unsigned short read(char *buffer, unsigned short size);
    bool write(const char *buffer, unsigned short size);
    std::string readAll(unsigned int limit = 4 * 1024 * 1024) const;

private:
    /** open a blob for reading only */
    DbBlob(FbApiHandle db, FbApiHandle trans, const FbQuad *blobId);

    // disable copying
    DbBlob(const DbBlob&) = delete;
    DbBlob &operator=(const DbBlob&) = delete;
private:
    FbQuad blob_id_;
    FbApiHandle blob_handle_;
    bool write_access_;
};

} /* namespace fb */

#endif /* DBWRAP__FB_DB_BLOB_H_ */
