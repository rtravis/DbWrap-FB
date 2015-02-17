/*
 * FbException.cpp - exceptions classes thrown by DbWrap++FB
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

#include "FbException.h"
#include <ibase.h>



namespace fb {

/** status should be an ISC_STATUS_ARRAY from ibase.h */
FbException::FbException(const char *operation, const long *status) :
                                std::runtime_error("Firebird exception!")
{
    if (!status) {
        what_ = "Firebird exception";
        if (operation) {
            what_ += ": ";
            what_ += operation;
        }
        return;
    }

    char buffer[1024];
    ISC_LONG sqlCode = isc_sqlcode(status);
    if (sqlCode != -999) {
        snprintf(buffer, sizeof(buffer), "SQL Code: %d\n", (int) sqlCode);
        what_ += buffer;
        isc_sql_interprete((short) sqlCode, buffer, (short) sizeof(buffer));
        what_ += buffer;
        what_ += "\n";
    }

    const ISC_STATUS *istatus = status;
    while(fb_interpret(buffer, (unsigned int) sizeof(buffer), &istatus)) {
        what_ += buffer;
        what_ += "\n";
    }
}

FbException::~FbException() noexcept
{
}

const char *FbException::what() const noexcept
{
    return what_.c_str();
}

} /* namespace fb */
