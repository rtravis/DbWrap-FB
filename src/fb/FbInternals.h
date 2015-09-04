/*
 * FbInternals.h - private header, not to be included in your code
 *
 * This is part of the "DbWrap++ for Firebird" (DbWrap++FB)
 * C++ library for accessing Firebird databases in your C++11
 * program.
 *
 * @created: Jan 5, 2015
 *
 * @copyright: Copyright (c) 2015 Robert Zavalczki, distributed
 * under the terms and conditions of the Lesser GNU General
 * Public License version 2.1
 */

#ifndef DBWRAP__FB_FBINTERNALS_H_
#define DBWRAP__FB_FBINTERNALS_H_
#include <ibase.h>

namespace fb {

struct FbVarchar
{
    ISC_SHORT length;
    char str[1];
};

/**
 * SqlDescriptorArea is a placeholder for XSQLDA. We use SqlDescriptorArea
 * because XSQLDA is an alias (typedef) for an anonymous struct and can't
 * be forward declared
 */
struct SqlDescriptorArea : public XSQLDA
{
};

struct XSqlVar : public XSQLVAR
{
};

} /* namespace fb */

#endif /* DBWRAP__FB_FBINTERNALS_H_ */
