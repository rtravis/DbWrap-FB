/*
 * FbCommon.h - avoids including ibase.h in public headers by
 *              providing some common types and constants
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

#ifndef DBWRAP__FB_FBCOMMON_H_
#define DBWRAP__FB_FBCOMMON_H_

namespace fb
{

/**
 * FbApiHandle is an alias for FB_API_HANDLE (must be the same type as
 * FB_API_HANDLE). We use FbApiHandle so we don't have to include ibase.h
 */
#if defined(_LP64) || defined(__LP64__) || defined(__arch64__) || defined(_WIN64)
typedef unsigned int	FbApiHandle;
#else
typedef void*		FbApiHandle;
#endif

/**
 * SqlDescriptorArea is a placeholder for XSQLDA. We use SqlDescriptorArea
 * because XSQLDA is an alias (typedef) for an anonymous struct and
 * can't be forward declared. It is declared in FbInternals.h.
 */
struct SqlDescriptorArea;

/**
 * XSqlVar is a placeholder for XSQLVAR
 */
struct XSqlVar;

/** SQL dialect used throughout the program */
constexpr int FB_SQL_DIALECT = 3;

/**
 * FbQuad is equivalent to ISC_QUAD (or GDS_QUAD), it exists so that you
 * don't have to include ibase.h in your code.
 */
struct FbQuad
{
    int quad_high;
    unsigned int quad_low;
};

} /* namespace fb */

#endif /* DBWRAP__FB_FBCOMMON_H_ */
