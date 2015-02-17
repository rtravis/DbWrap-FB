/*
 * DbBlob.cpp - proxy for blob objects
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

#include "DbBlob.h"
#include <ibase.h>
#include "FbException.h"
#include <limits.h>

namespace fb
{

DbBlob::DbBlob(FbApiHandle db, FbApiHandle trans,
               const FbQuad *blobId) : blob_id_(*blobId),
                                       blob_handle_(0),
                                       write_access_(false)
{
    if (!blobId || !db || !trans) {
        // invalid, empty or null blob
        return;
    }

    ISC_STATUS_ARRAY status;
    if (isc_open_blob2(status, &db, &trans, &blob_handle_,
                      (ISC_QUAD*) blobId, 0, nullptr)) {
        throw FbException("Failed to open blob.", status);
    }
}

/** create a new, write only blob */
DbBlob::DbBlob(FbApiHandle db, FbApiHandle trans) :
                                            blob_id_{0, 0},
                                            blob_handle_(0),
                                            write_access_(true)
{
    ISC_STATUS_ARRAY status;
    if (isc_create_blob2(status, &db, &trans, &blob_handle_,
                         (ISC_QUAD*) &blob_id_, 0, nullptr)) {
        throw FbException("Failed to create blob.", status);
    }
}

DbBlob::DbBlob(DbBlob &&b) : blob_id_(b.blob_id_),
                             blob_handle_(b.blob_handle_),
                             write_access_(b.write_access_)
{
    b.blob_handle_ = 0;
}

DbBlob::~DbBlob()
{
    close();
}

void DbBlob::close()
{
    ISC_STATUS_ARRAY status;
    if (blob_handle_ != 0) {
        isc_close_blob(status, &blob_handle_);
        blob_handle_ = 0;
    }
}

void DbBlob::cancel()
{
    ISC_STATUS_ARRAY status;
    if (blob_handle_ != 0) {
        isc_cancel_blob(status, &blob_handle_);
        blob_handle_ = 0;
    }
}

/** test if this is a valid blob */
DbBlob::operator bool() const
{
    return (blob_handle_ != 0);
}

const FbQuad &DbBlob::getBlobId() const
{
    return blob_id_;
}

unsigned short DbBlob::read(char *buffer, unsigned short size)
{
    if (blob_handle_ == 0) {
        return 0;
    }

    if (write_access_) {
        throw std::logic_error("Can't read from blob opened for writing!");
    }

    ISC_STATUS_ARRAY status;
    unsigned short bytesRead = 0;

    ISC_STATUS res = isc_get_segment(
                            status,
                            &blob_handle_,
                            &bytesRead, size,
                            buffer);

    if (res == isc_segstr_eof) {
        return 0;
    } else if (res && res != isc_segment) {
        throw FbException("Failed to read blob!", status);
    }
    return bytesRead;
}


std::string DbBlob::readAll(unsigned int limit) const
{
    std::string data;

    if (blob_handle_ == 0) {
        return data;
    }

    if (write_access_) {
        throw std::logic_error("Can't read from blob opened for writing!");
    }

    while (true) {
        ISC_STATUS_ARRAY status;
        unsigned short bytesRead = 0;
        constexpr unsigned short BUF_SIZE = SHRT_MAX;
        ISC_SCHAR buffer[BUF_SIZE];

        ISC_STATUS res = isc_get_segment(
                                status,
                                const_cast<FbApiHandle*>(&blob_handle_),
                                &bytesRead, BUF_SIZE,
                                buffer);

        if (res == isc_segstr_eof) {
            break;
        } else if (res && res != isc_segment) {
            throw FbException("Failed to read blob!", status);
        }

        if ((data.size() + bytesRead) <= limit) {
            data.append(buffer, bytesRead);
        } else {
            bytesRead = (unsigned short) (limit - data.size());
            data.append(buffer, bytesRead);
            break;
        }
    }
    return data;
}

bool DbBlob::write(const char *buffer, unsigned short size)
{
    if (blob_handle_ == 0) {
        return false;
    }

    if (!write_access_) {
        throw std::logic_error("Can't write to blob opened for reading!");
    }

    ISC_STATUS_ARRAY status;
    if (isc_put_segment(status, &blob_handle_, size, buffer)) {
        throw FbException("Failed to write to blob!", status);
    }
    return true;
}

} /* namespace fb */
