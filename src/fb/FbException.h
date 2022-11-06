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

#ifndef DBWRAP_FB_FBEXCEPTION_H_
#define DBWRAP_FB_FBEXCEPTION_H_

#include <cstdint>
#include <stdexcept>


namespace fb {

class FbException : public std::runtime_error
{
public:
    /** status should be an ISC_STATUS_ARRAY from ibase.h */
    FbException(const char *operation, const intptr_t *status);
    FbException(const FbException&) = default;

    virtual ~FbException() noexcept override;
    virtual const char *what() const noexcept override;

private:
    std::string what_;
};

} /* namespace fb */

#endif /* DBWRAP_FB_FBEXCEPTION_H_ */
