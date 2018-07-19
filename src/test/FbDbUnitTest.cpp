/*
 * FbDbUnitTest.cpp - various tests to validate functionality
 *
 * This is part of the "DbWrap++ for Firebird" (DbWrap++FB)
 * C++ library for accessing Firebird databases in your C++11
 * program.
 *
 * @created: Dec 31, 2014
 *
 * @copyright: Copyright (c) 2015 Robert Zavalczki, distributed
 * under the terms and conditions of the Lesser GNU General
 * Public License version 2.1
 */
#include "DbBlob.h"
#include "DbConnection.h"
#include "DbRowProxy.h"
#include "DbStatement.h"
#include "DbTransaction.h"
#include "FbException.h"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <unistd.h>

static char g_dbName[200] = "/tmp/DbWrap++FB_LKzgBZOx.fdb";
static char g_dbServer[200] = "localhost";
static char g_dbUserName[100] = "sysdba";
static char DB_PASSWORD[32] = "masterkey";


namespace fbunittest
{

using namespace fb;

void create_database()
{
    if (access(g_dbName, F_OK) == 0) {
        unlink(g_dbName);
    }
    DbConnection dbc(g_dbName, g_dbServer, g_dbUserName, DB_PASSWORD);
}

void attach_database()
{
    if (access(g_dbName, F_OK) != 0) {
        create_database();
    }

    DbConnection dbc(g_dbName, g_dbServer, g_dbUserName, DB_PASSWORD);
}

void drop_table_if_exists(DbConnection &db,
                          DbTransaction &tr,
                          const char *tableName)
{
    assert(tableName);

    DbStatement st = db.createStatement(
                            "SELECT RDB$RELATION_ID "
                            "FROM RDB$RELATIONS "
                            "WHERE RDB$RELATION_NAME=?",
                            &tr);
    st.setText(1, tableName);

    if (st.uniqueResult()) {
        printf("dropping table %s\n", tableName);
        // table 'memo1' already exists, drop it
        std::string dropStatement = "DROP TABLE ";
        dropStatement += tableName;
        db.executeUpdate(dropStatement.c_str(), &tr);
        tr.commitRetain();
    }
}

void populate_database()
{
    // create or attach database
    DbConnection dbc(g_dbName, g_dbServer, g_dbUserName, DB_PASSWORD);
    DbTransaction trans(dbc.nativeHandle(), 1);

    drop_table_if_exists(dbc, trans, "TEST1");

    // create a test table
    dbc.executeUpdate(
            "CREATE TABLE TEST1 ("
                "IID    INTEGER, "
                "I64_1  BIGINT, "
                "VC5    VARCHAR(5), "
                "I64V_2 BIGINT, "
                "VAL4   VARCHAR(29) DEFAULT '', "
                "TS     TIMESTAMP DEFAULT 'NOW', "
                "CONSTRAINT PK_TEST1 PRIMARY KEY (IID))");

    dbc.executeUpdate("GRANT ALL ON TEST1 TO PUBLIC WITH GRANT OPTION");

    try {
        // start a read-only transaction and try to modify some data
        DbTransaction tr1(dbc.nativeHandle(), 1,
                          DefaultTransMode::Commit,
                          TransStartMode::StartReadOnly);


        dbc.executeUpdate(
                "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (1, 20, 'a')",
                &tr1);

        // we should not get here
        throw std::runtime_error(
                "successfully modified database in a read-only transaction!");

    } catch (FbException &exc) {
        // this is expected, we try to modify table in a read-only transaction
    }

    dbc.executeUpdate(
            "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (1, 10, 'one')",
            &trans);
    dbc.executeUpdate(
            "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (2, 20, 'two')",
            &trans);
    dbc.executeUpdate(
            "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (3, 30, 'three')",
            &trans);

    // commit transaction, but keep it for further work
    trans.commitRetain();

    dbc.executeUpdate(
            "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (4, 40, '')",
            &trans);
    dbc.executeUpdate(
            "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (5, 50, NULL)",
            &trans);


    try {
        // a unique constraint violation should throw an exception
        dbc.executeUpdate(
                "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (3, 20, 'three')",
                &trans);
        throw std::runtime_error("constraint violation should have failed");
    } catch (FbException &exc) {
        // OK, unique constraint violation
    }

    // test prepared statements
    DbStatement dbs0 = dbc.createStatement(
            "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (?, ?, ?) RETURNING (IID)",
            &trans);

    dbs0.setInt(1, 6);
    dbs0.setInt(2, 60);
    dbs0.setText(3, "sixty");
    dbs0.execute();

    dbs0.setInt(1, 7);
    dbs0.setInt(2, 70);
    dbs0.setText(3, "seventy");
    dbs0.execute();

    dbs0.setInt(1, 8);
    dbs0.setNull(2);
    dbs0.setText(3, nullptr);
    dbs0.execute();

    // test prepared statements with named parameters max68
    DbStatement dbs1 = dbc.createStatement(
            "INSERT INTO TEST1 (IID, I64_1, VAL4) VALUES (:IID, :I64_1, :VAL4) RETURNING (IID)",
            &trans);
    dbs1.paramByName("IID")->setValue(100);
    dbs1.paramByName("I64_1")->setValue(100);
    dbs1.paramByName("VAL4")->setValue("a hundred");
    dbs1.execute();

    // by committing the transaction we're not allowed to use it
    // further down in this function
    trans.commit();

    // create a transaction object, but don't actually start
    // a transaction until we call the start method
    DbTransaction tr3(dbc.nativeHandle(), 1,
                      DefaultTransMode::Commit,
                      TransStartMode::DeferStart);

    // start the transaction in read-only mode
    tr3.start(true);

    // test statements returning result sets
    DbStatement dbs = dbc.createStatement(
                            "SELECT r.* FROM TEST1 r", &tr3);

    int count = 0;
    for (DbStatement::Iterator i = dbs.iterate(); i != dbs.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
    assert(count == 9); // max68
}

void select_prepared_statements_tests()
{
    int count = 0;
    DbConnection dbc(g_dbName, g_dbServer, g_dbUserName, DB_PASSWORD);

    DbTransaction tr2(dbc.nativeHandle(), 1);

    DbStatement dbs2 = dbc.createStatement(
                        "SELECT r.* FROM TEST1 r WHERE r.IID=?", &tr2);

    dbs2.setInt(1, 2);
    count = 0;
    for (DbStatement::Iterator i = dbs2.iterate(); i != dbs2.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
    assert(count == 1);

    // re-run the query statement with a different parameter
    dbs2.reset();
    dbs2.setInt(1, 3);
    count = 0;
    for (DbStatement::Iterator i = dbs2.iterate(); i != dbs2.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
    assert(count == 1);


    DbStatement dbs3 = dbc.createStatement(
                            "SELECT r.* FROM TEST1 r WHERE r.VC5=?");
    dbs3.setText(1, "three");

    count = 0;
    for (DbStatement::Iterator i = dbs3.iterate(); i != dbs3.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
    assert(count == 1);
    
    // test statement with named parameters
    DbStatement dbs4 = dbc.createStatement(
                        "SELECT r.* FROM TEST1 r WHERE r.IID=:iid", &tr2);

    dbs4.paramByName("iid")->setValue(100);
    count = 0;
    for (DbStatement::Iterator i = dbs4.iterate(); i != dbs2.end(); ++i) {
        DbRowProxy row = *i;
        printf("Named parameters query %02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
    assert(count == 1);
    
}

void blob_tests()
{
    // create or attach database
    DbConnection dbc(g_dbName, g_dbServer, g_dbUserName, DB_PASSWORD);
    DbTransaction trans(dbc.nativeHandle(), 1);
    drop_table_if_exists(dbc, trans, "MEMO1");

    DbStatement st = dbc.createStatement(
                "CREATE TABLE MEMO1 "
                "("
                "    ID    BIGINT NOT NULL, "
                "    NAME  VARCHAR(32), "
                "    MEMO  BLOB SUB_TYPE 1, "
                "    MEMO_COPY  BLOB SUB_TYPE 1, "
                "    DATA  BLOB, "
                "    CONSTRAINT pk_memo1 PRIMARY KEY (ID)"
                ")",
                &trans);

    printf("creating table MEMO1\n");
    st.execute();
    dbc.executeUpdate("GRANT ALL ON MEMO1 TO PUBLIC WITH GRANT OPTION", &trans);
    trans.commitRetain();

    printf("inserting rows into table MEMO1\n");
    const char *sql = "INSERT INTO MEMO1 (ID, NAME, MEMO, MEMO_COPY, DATA) VALUES "
                      "(?,?,?, :MEMO_COPY, 'abcdefghijkl:mnopqrstuvxyz')";

    DbBlob blob(*dbc.nativeHandle(), *trans.nativeHandle());
    blob.write("Hello world!\n", 13);
    std::string str1(80, 'a');
    str1 += "\n";
    for (int i = 0; i != 4; ++i) {
        blob.write(str1.c_str(), str1.length());
    }
    blob.write("zzzzzz", 6);
    blob.close();

    st = dbc.createStatement(sql , &trans);
    st.setInt(1, 1);
    st.setText(2, "val1");
    st.setBlob(3, blob);
    st.paramByName("MEMO_COPY")->setValue(blob);
    st.execute();

    // repeat the insert statement with different parameters
    st.reset();
    st.setInt(1, 2);
    st.setText(2, "val2");
    st.execute();
    trans.commitRetain();

    printf("querying rows from table MEMO1\n");
    st = dbc.createStatement("SELECT r.* FROM MEMO1 r", &trans);
    int count = 0;
    for (DbStatement::Iterator i = st.iterate(); i != st.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        printf("%s\n", row.getBlob(2).readAll().c_str());
        printf("%s\n", row.getBlob(3).readAll().c_str());
        printf("%s\n", row.getBlob(4).readAll().c_str());
    }
}

void print_all_datatypes()
{
    DbConnection dbc(g_dbName, g_dbServer, g_dbUserName, DB_PASSWORD);
    DbTransaction trans(dbc.nativeHandle(), 1);

    drop_table_if_exists(dbc, trans, "ATTRIBUTE_VALUE");

    const char *sql = R"(
        CREATE TABLE ATTRIBUTE_VALUE
        (
          ID        BIGINT NOT NULL,
          ATTR_ID   INTEGER,
          OBJ_ID    BIGINT,
          INT_VAL   BIGINT,
          STR_VAL   VARCHAR(500),
          DATE_VAL  TIMESTAMP,
          BLOB_VAL  BLOB SUB_TYPE 1,
          FLOAT_VAL DOUBLE PRECISION,
          CONSTRAINT PK_ATTRIBUTE_VALUE PRIMARY KEY (ID)
        );)";

    dbc.executeUpdate(sql, &trans);

    sql = "GRANT ALL ON ATTRIBUTE_VALUE TO PUBLIC WITH GRANT OPTION";
    dbc.executeUpdate(sql, &trans);

    trans.commitRetain();

    sql = R"(INSERT INTO ATTRIBUTE_VALUE (
             ID, ATTR_ID, OBJ_ID,
             INT_VAL, STR_VAL, DATE_VAL,
             BLOB_VAL, FLOAT_VAL)
            VALUES ('1',
                    '1',
                    '1',
                    '555',
                    'abcdefg',
                    'NOW',
                    'blob 9999',
                    '3.1415926535');
           )";

    dbc.executeUpdate(sql, &trans);

    DbStatement st = dbc.createStatement(
                        "SELECT r.* FROM ATTRIBUTE_VALUE r", &trans);
    int count = 0;
    for (DbStatement::Iterator i = st.iterate(); i != st.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
    
    // re-print using named field
    DbStatement st1 = dbc.createStatement(
                        "SELECT r.* FROM ATTRIBUTE_VALUE r", &trans);
    count = 0;
    for (DbStatement::Iterator i = st1.iterate(); i != st1.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);

        printf("ID     : %lld\n", row.fieldByName("ID")->asInteger());  
        printf("ATTR_ID: %lld\n", row.fieldByName("ATTR_ID")->asInteger());  
        printf("OBJ_ID : %lld\n", row.fieldByName("OBJ_ID")->asInteger());  
        printf("INT_VAL: %lld\n", row.fieldByName("INT_VAL")->asInteger());  
        printf("STR_VAL: %s\n", row.fieldByName("STR_VAL")->asString().c_str());  

        printf("DATE_VAL: %s\n", row.fieldByName("DATE_VAL")->formatDate().c_str());  
        printf("BLOB_VAL: %s\n", row.fieldByName("BLOB_VAL")->asString().c_str());  
        printf("FLOAT_VAL: %f\n", row.fieldByName("FLOAT_VAL")->asDouble());  
    }
}

void execute_procedure_tests()
{
    // create or attach database
    DbConnection dbc(g_dbName, g_dbServer, g_dbUserName, DB_PASSWORD);
    DbTransaction trans(dbc.nativeHandle(), 1);

    drop_table_if_exists(dbc, trans, "TEST1");

    // create a test table
    dbc.executeUpdate(
            "CREATE TABLE TEST1 ("
                "IID    INTEGER, "
                "I64_1  BIGINT, "
                "VC5    VARCHAR(5), "
                "I64V_2 BIGINT, "
                "VAL4   VARCHAR(29) DEFAULT '', "
                "TS     TIMESTAMP DEFAULT 'NOW', "
                "CONSTRAINT PK_TEST1 PRIMARY KEY (IID))");

    dbc.executeUpdate("GRANT ALL ON TEST1 TO PUBLIC WITH GRANT OPTION");

    // test prepared statements
    DbStatement dbs0 = dbc.createStatement(
            "INSERT INTO TEST1 (IID, I64_1, VC5) VALUES (?, ?, ?) RETURNING (IID)",
            &trans);

    dbs0.setInt(1, 6);
    dbs0.setInt(2, 60);
    dbs0.setText(3, "sixty");
    dbs0.execute();

    dbs0.setInt(1, 7);
    dbs0.setInt(2, 70);
    dbs0.setText(3, "seventy");
    dbs0.execute();

    dbs0.setInt(1, 8);
    dbs0.setNull(2);
    dbs0.setText(3, nullptr);
    DbRowProxy executeResultRow = dbs0.uniqueResult();
    if (executeResultRow) {
        printf("unique result returned IID: %d\n", executeResultRow.getInt(0));
        if (executeResultRow.getInt(0) != 8) {
            throw std::runtime_error("INSERT ... RETURNING uniqueResult failure.");
        }
    } else {
        throw std::runtime_error("INSERT ... RETURNING uniqueResult failure.");
    }

    // by committing the transaction we're not allowed to use it
    // further down in this function
    trans.commit();
}

} // namespace fbunittest


int main (int argc, char *argv[]) try
{
    using namespace fb;
    using namespace fbunittest;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-server") == 0 && (i + 1) < argc) {
            snprintf(g_dbServer, sizeof(g_dbServer), argv[i + 1]);
            ++i;
        } else if (strcmp(argv[i], "-name") == 0 && (i + 1) < argc) {
            snprintf(g_dbName, sizeof(g_dbName), argv[i + 1]);
            ++i;
        } else if (strcmp(argv[i], "-user") == 0 && (i + 1) < argc) {
            snprintf(g_dbUserName, sizeof(g_dbUserName), argv[i + 1]);
            ++i;
        } else  if (strcmp(argv[i], "-password") == 0 && (i + 1) < argc) {
            snprintf(DB_PASSWORD, sizeof(DB_PASSWORD), argv[i + 1]);
            ++i;
        } else {
            printf("Unknown parameter: '%s'\n", argv[i]);
            return 1;
        }
    }

    create_database();
    attach_database();
    populate_database();
    select_prepared_statements_tests();
    blob_tests();
    print_all_datatypes();
    execute_procedure_tests();
    std::cout << "Firebird API Test completed successfully.\n";

    return 0;
} catch (std::exception &exc) {
    std::cerr << "test failed:\n" << exc.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "unexpected exception, test failed: \n";
    return 1;
}

