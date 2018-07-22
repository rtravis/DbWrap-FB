/*
 * FbCommon.cpp - avoids including ibase.h in public headers by
 *                providing some common types and constants
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

#include "FbCommon.h"
#include <ibase.h>

namespace fb
{

// check some static assertions
static_assert(sizeof(FbApiHandle) == sizeof(FB_API_HANDLE),
                "Invalid size for FB API handle!");
static_assert(sizeof(FbApiHandle) == sizeof(isc_db_handle),
                "Invalid size for DB handle!");
static_assert(sizeof(FbApiHandle) == sizeof(isc_tr_handle),
                "Invalid size for transaction handle!");
static_assert(sizeof(FbApiHandle) == sizeof(isc_stmt_handle),
                "Invalid size for statement handle!");
static_assert(sizeof(FbApiHandle) == sizeof(isc_blob_handle),
                "Invalid size for blob handle!");
static_assert(sizeof(FbQuad) == sizeof(ISC_QUAD),
                "size of(FbQuad) <> size of(ISC_QUAD)!");
static_assert(sizeof(intptr_t) == sizeof(ISC_STATUS),
                "size of(intptr_t) <> size of(ISC_STATUS)!");
} /* namespace fb */
