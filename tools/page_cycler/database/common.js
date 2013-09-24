// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of common functions used by all database
 * performance tests.
 */

var CANNOT_OPEN_DB = -1;
var SETUP_FAILED = -2;
var TEST_FAILED = -3;

var TRANSACTIONS = 1000;  // number of transactions; number of rows in the DB
var RANDOM_STRING_LENGTH = 20;  // the length of the string on each row

/**
 * Generates a random string of upper-case letters of
 * RANDOM_STRING_LENGTH length.
 */
function getRandomString() {
  var s = '';
  for (var i = 0; i < RANDOM_STRING_LENGTH; i++)
    s += String.fromCharCode(Math.floor(Math.random() * 26) + 64);
  return s;
}

/**
 * Sets up and runs a performance test.
 * @param {!Object} params An object which must have the following fields:
 *   dbName: The database name.
 *   readOnly: If true, transactions will be run using the readTransaction()
 *       method. Otherwise, transaction() will be used.
 *   insertRowsAtSetup: Determines if setting up the database should include
 *       inserting TRANSACTIONS rows in it.
 *   transactionCallback: The transaction callback that should be timed. This
 *       function will be run TRANSACTIONS times and must take a transaction
 *       object as its only parameter.
 *   customRunTransactions: A custom function for running and timing
 *       transactions. If this parameter is not null, runPerformanceTest() will
 *       ignore the txFnct parameter and will call customRunTransactions() as
 *       soon as the setup is complete. In this case, the user is responsible
 *       for running and timing the transactions. If not null, this parameter
 *       must be a function that takes a database object as its only parameter.
 */
function runPerformanceTest(params) {
  var db = openTestDatabase(params.dbName);
  if (!db) {
    testComplete(CANNOT_OPEN_DB);
    return;
  }

  db.transaction(
      function(tx) {
        tx.executeSql('CREATE TABLE IF NOT EXISTS Test (ID INT, Foo TEXT)', [],
                      function(tx, data) {}, function(tx, error) {});
        tx.executeSql('DELETE FROM Test');
        if (params.insertRowsAtSetup) {
          var randomString = getRandomString();
          for (var i = 0; i < TRANSACTIONS; i++) {
            tx.executeSql('INSERT INTO Test VALUES (?, ?)',
                          [i, randomString]);
          }
        }
      }, function(error) {
        testComplete(SETUP_FAILED);
      }, function() {
        if (params.customRunTransactions)
          params.customRunTransactions(db);
        else
          runTransactions(db, params.readOnly, params.transactionCallback);
      });
}

/**
 * Opens a database with the given name.
 * @param {string} name The name of the database.
 */
function openTestDatabase(name) {
  if (window.openDatabase) {
    return window.openDatabase(name, '1.0', 'Test database.',
                               TRANSACTIONS * RANDOM_STRING_LENGTH);
  }

  return null;
}

/**
 * Runs the given transaction TRANSACTIONS times.
 * @param {!Object} db The database to run transactions on.
 * @param {boolean} readOnly If true, all transactions will be run using the
 *     db.readTransaction() call. Otherwise, the transactions will be run
 *     using the db.transaction() call.
 * @param {function(!Object)} The transaction callback.
 */
function runTransactions(db, readOnly, transactionCallback) {
  var transactionsCompleted = 0;
  var transactionFunction = readOnly ? db.readTransaction : db.transaction;
  var startTime = Date.now();
  for (var i = 0; i < TRANSACTIONS; i++) {
    transactionFunction.call(db, transactionCallback,
                             function(error) {
                               testComplete(TEST_FAILED);
                             }, function() {
                               if (++transactionsCompleted == TRANSACTIONS)
                                 testComplete(Date.now() - startTime);
                             });
  }
}
