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
#include "DbConnection.h"
#include <stdexcept>
#include <iostream>
#include "DbTransaction.h"
#include <unistd.h>
#include "FbException.h"
#include "DbStatement.h"
#include "DbRowProxy.h"
#include <cassert>
#include "DbBlob.h"

static const char DB_NAME[] = "/home/toby/fbdb_rx7wxDR.fdb";

namespace fbunittest
{

using namespace fb;

void create_db()
{
    if (access(DB_NAME, F_OK) == 0) {
        unlink(DB_NAME);
    }
    DbConnection dbc(DB_NAME);
}

void attach_db()
{
    if (access(DB_NAME, F_OK) != 0) {
        create_db();
    }

    DbConnection dbc(DB_NAME);
}

void populate_db()
{
    DbConnection dbc(DB_NAME);
    dbc.executeUpdate("create table test1 "
            "(iid integer, "
            "i64_1 bigint, "
            "vc5 varchar(5), "
            "i64v_2 bigint, "
            "val4 varchar(29) default '', "
            "ts timestamp default 'NOW', "
            "CONSTRAINT pk_test1 PRIMARY KEY (iid))");
    dbc.executeUpdate("GRANT ALL ON TEST1 TO PUBLIC WITH GRANT OPTION");

    try {
        DbTransaction tr1(dbc.nativeHandle(), 1,
                          DefaultTransMode::Commit,
                          TransStartMode::StartReadOnly);


        dbc.executeUpdate("insert into test1 (iid, i64_1, vc5) values (1, 20, 'a')", &tr1);
        throw std::runtime_error("successfully modified read-only transaction");
    } catch (FbException &exc) {
        // this is expected, we try to modify table in a read-only transaction
        // std::cout << "OK: " << exc.what() << '\n';
    }

    DbTransaction tr2(dbc.nativeHandle(), 1);
    dbc.executeUpdate("insert into test1 (iid, i64_1, vc5) values (1, 10, 'one')", &tr2);
    dbc.executeUpdate("insert into test1 (iid, i64_1, vc5) values (2, 20, 'two')", &tr2);
    dbc.executeUpdate("insert into test1 (iid, i64_1, vc5) values (3, 30, 'three')", &tr2);
    tr2.commitRetain();
    dbc.executeUpdate("insert into test1 (iid, i64_1, vc5) values (4, 40, '')", &tr2);
    dbc.executeUpdate("insert into test1 (iid, i64_1, vc5) values (5, 50, NULL)", &tr2);
    try {
        dbc.executeUpdate(
                "insert into test1 (iid, i64_1, vc5) values (3, 20, 'three')", &tr2);
        throw std::runtime_error("constraint violation should have failed");
    } catch (FbException &exc) {
        // OK, unique constraint violation
    }

    DbStatement dbs0 = dbc.createStatement(
            "insert into test1 (iid, i64_1, vc5) values (?, ?, ?) returning (iid)",
            &tr2);

    dbs0.setInt(1, 6);
    dbs0.setInt(2, 60);
    dbs0.setText(3, "sixty");
    dbs0.execute();
    dbs0.setInt(1, 7);
    dbs0.setInt(2, 70);
    dbs0.setText(3, "seventy");

    dbs0.execute();
    int count = 0;

    // tr2.commitRetain();
    tr2.commit();

    DbTransaction tr3(dbc.nativeHandle(), 1,
                      DefaultTransMode::Commit,
                      TransStartMode::DeferStart);
    tr3.start(true);

    DbStatement dbs = dbc.createStatement("select r.* from test1 r",
                                        &tr3);

    count = 0;
    for (DbStatement::Iterator i = dbs.iterate(); i != dbs.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
    assert(count == 7);
}

void select_prepared_statements_tests()
{
    int count = 0;
    DbConnection dbc(DB_NAME);

    DbTransaction tr2(dbc.nativeHandle(), 1);

    DbStatement dbs2 = dbc.createStatement(
                        "select r.* from test1 r where r.iid=?", &tr2);

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


    DbStatement dbs3 = dbc.createStatement("select r.* from test1 r where r.vc5=?");
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
}

void blob_tests()
{
    // create or attach database
    DbConnection dbc(DB_NAME);
    DbTransaction trans(dbc.nativeHandle(), 1);

    DbStatement st = dbc.createStatement(
                            "select rdb$relation_id "
                            "from rdb$relations "
                            "where rdb$relation_name=?",
                            &trans);
    st.setText(1, "MEMO1");
    DbRowProxy row = st.uniqueResult();

    if (row) {
        printf("dropping table memo1\n");
        // table 'memo1' already exists, drop it
        dbc.executeUpdate("drop table memo1", &trans);
        trans.commitRetain();
    }

    st = dbc.createStatement(
            "CREATE TABLE memo1 "
            "("
            "    ID bigint not null, "
            "    NAME varchar(32), "
            "    MEMO blob sub_type 1, "
            "    DATA blob, "
            "    CONSTRAINT pk_memo1 PRIMARY KEY (ID)"
            ")",
            &trans);
    printf("creating table memo1\n");
    st.execute();
    dbc.executeUpdate("GRANT ALL ON memo1 TO PUBLIC WITH GRANT OPTION", &trans);
    trans.commitRetain();

    printf("inserting rows into table memo1\n");
    const char *sql = "insert into memo1 (id, name, memo, data) values "
            "(?,?,?,'abcdefghijklmnopqrstuvxyz')";

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
    st.execute();
    st.reset();
    st.setInt(1, 2);
    st.setText(2, "val2");
    st.execute();
    trans.commitRetain();

    printf("querying rows from table memo1\n");
    st = dbc.createStatement("select r.* from memo1 r", &trans);
    int count = 0;
    for (DbStatement::Iterator i = st.iterate(); i != st.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        printf("%s\n", row.getBlob(2).readAll().c_str());
        printf("%s\n", row.getBlob(3).readAll().c_str());
        //for (unsigned i = 0; i != row.columnCount(); ++i) {
        //	printf("%02u %s\n", i, row.getText(i).c_str());
        //}
    }
    printf("finish\n");
}

void print_all_datatypes()
{
    DbConnection dbc("/tmp/eav_test.fdb");
    DbTransaction trans(dbc.nativeHandle(), 1);
    DbStatement st = dbc.createStatement(
                        "select r.* from val r", &trans);
    int count = 0;
    for (DbStatement::Iterator i = st.iterate(); i != st.end(); ++i) {
        DbRowProxy row = *i;
        printf("%02d ------------------\n", count++);
        for (unsigned i = 0; i != row.columnCount(); ++i) {
            printf("%02u %s\n", i, row.getText(i).c_str());
        }
    }
}

} // namespace fbunittest


int main (int argc, char** argv) try
{
    using namespace fb;
    using namespace fbunittest;

    // create_db();
    // attach_db();
    // populate_db();
    // select_prepared_statements_tests();
    // blob_tests();
    print_all_datatypes();
    std::cout << "Firebird API Test completed successfully.\n";

    return 0;
} catch (std::exception &exc) {
    std::cerr << "test failed:\n" << exc.what() << '\n';
    return 1;
} catch (...) {
    std::cerr << "unexpected exception, test failed: \n";
    return 1;
}
