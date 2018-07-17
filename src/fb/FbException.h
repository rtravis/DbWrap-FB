/*
 * FbException.h - exceptions classes thrown by DbWrap++FB
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

#ifndef DBWRAP__FB_FBEXCEPTION_H_
#define DBWRAP__FB_FBEXCEPTION_H_

#include <stdexcept>
#include <ibase.h>

namespace fb {

class FbException : public std::runtime_error
{
public:
    /** status should be an ISC_STATUS_ARRAY from ibase.h */
    FbException(const char *operation, ISC_STATUS_ARRAY status); //max68
    virtual ~FbException() noexcept;
    virtual const char *what() const noexcept;

private:
    std::string what_;
};

} /* namespace fb */

#endif /* DBWRAP__FB_FBEXCEPTION_H_ */
