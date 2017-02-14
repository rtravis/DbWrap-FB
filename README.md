# DbWrap++ for Firebird - yet another C++ database wrapper

## Overview

DbWrap++ for Firebird (DbWrap++FB) is a class library wrapper over
the _InterBase Applications Programming Interface (API)_, that is
used to access Firebird databases in a program.

Its purpose is to make relational database programming for
Firebird as easy as possible, without sacrificing efficiency.


## Requirements

DbWrap++FB makes use of C++ features available only in the C++11
standard of the C++ programming language, thus a C++11 capable
compiler is required.

## Usage

```cpp
    #include "DbConnection.h"
    #include "DbStatement.h"
    #include "DbRowProxy.h"

    #include <iostream>
    // ....
    using namespace fb;
    using namespace std;

    DbConnection connection(DATABASE_NAME, "localhost", "sysdba", "masterkey");
    DbStatement statement = connection.createStatement(
        "SELECT first_name, last_name FROM employee");

    for (DbStatement::Iterator i = statement.iterate();
         i != statement.end(); ++i) {
        DbRowProxy row = *i;
        cout << "First name: " << row.getText(0) << "\n"
             << "Last name: " << row.getText(1) << "\n";
    }
    // ....
    connection.executeUpdate(
        "INSERT INTO employee (first_name, last_name, phone_ext) "
        "VALUES ('John', 'Doe', '5555')");
    // ....
    statement = dbc.createStatement(
            "INSERT INTO employee (first_name, last_name, phone_ext) "
            "VALUES (?, ?, ?)");

    statement.setText(1, "Jane");
    statement.setText(2, "Doe");
    statement.setText(3, "6666");
    statement.execute();

    statement.setText(1, "James");
    statement.setText(2, "Pipo");
    statement.setText(3, "7777");
    statement.execute();

    // statement transaction will be committed when the statement is
    // either closed or destroyed automatically
    // ....
```

## Tasks

Contributors are welcome to contact me.

Documentation is not yet available, but you can check the
"FbDbUnitTest.cpp" source file for sample usage.

The library was tested only in a Linux environment with the
embedded version of the Firebird 2.5 database.


## License

DbWrap++FB is an open source free software project.
Copyright (c) 2015 Robert Zavalczki, distributed under the terms
and conditions of the Lesser GNU General Public License version
2.1


## Acknowlegements

Firebird is a registered trademark of the Firebird Foundation Incorporated
