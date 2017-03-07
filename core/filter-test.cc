/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#include <gtest/gtest.h>
#include "core/client.h"
#include "core/configuration.h"
#include "core/get.h"
#include "core/result.h"
#include "core/table.h"
#include "if/Comparator.pb.h"
#include "if/HBase.pb.h"
#include "serde/table-name.h"
#include "test-util/test-util.h"

using hbase::Configuration;
using hbase::Get;
using hbase::FilterFactory;
using hbase::Table;
using hbase::TestUtil;
using hbase::pb::CompareType;
using hbase::ComparatorFactory;
using hbase::Comparator;

class FilterTest : public ::testing::Test {
 protected:
  static void SetUpTestCase() {
    test_util_ = std::make_unique<TestUtil>();
  }

  static void TearDownTestCase() { test_util_.release(); }

  virtual void SetUp() {}
  virtual void TearDown() {}

  static std::unique_ptr<TestUtil> test_util_;
};


std::unique_ptr<TestUtil> FilterTest::test_util_ = nullptr;

TEST_F(FilterTest, GetWithColumnPrefixFilter) {
  // write row1 with 3 columns (column_1, column_2, and foo_column)
  FilterTest::test_util_->CreateTable("t", "d");
  FilterTest::test_util_->TablePut("t", "row1", "d", "column_1", "value1");
  FilterTest::test_util_->TablePut("t", "row1", "d", "column_2", "value2");
  FilterTest::test_util_->TablePut("t", "row1", "d", "foo_column", "value3");

  // Create TableName and Row to be fetched from HBase
  auto tn = folly::to<hbase::pb::TableName>("t");
  auto row = "row1";

  // Gets to be performed on above HBase Table
  Get get_all(row);  // expected to return all 3 columns
  Get get_one(row);  // expected to return 1 column
  Get get_two(row);  // expected to return 2 column

  get_one.SetFilter(FilterFactory::ColumnPrefixFilter("foo_"));
  get_two.SetFilter(FilterFactory::ColumnPrefixFilter("column_"));

  // Create a client
  hbase::Client client(*(FilterTest::test_util_->conf()));

  // Get connection to HBase Table
  auto table = client.Table(tn);
  ASSERT_TRUE(table) << "Unable to get connection to Table.";

  // Perform the Get
  auto result_all = table->Get(get_all);
  auto result_one = table->Get(get_one);
  auto result_two = table->Get(get_two);

  table->Close();
  client.Close();

  // Test the values
  ASSERT_TRUE(!result_one->IsEmpty()) << "Result shouldn't be empty.";
  ASSERT_TRUE(!result_two->IsEmpty()) << "Result shouldn't be empty.";
  ASSERT_TRUE(!result_all->IsEmpty()) << "Result shouldn't be empty.";
  EXPECT_EQ(row, result_one->Row());
  EXPECT_EQ(row, result_two->Row());
  EXPECT_EQ(row, result_all->Row());
  EXPECT_EQ(1, result_one->Size());
  EXPECT_EQ(2, result_two->Size());
  EXPECT_EQ(3, result_all->Size());
  EXPECT_EQ("value3", *(result_one->Value("d", "foo_column")));
  EXPECT_EQ("value1", *(result_two->Value("d", "column_1")));
  EXPECT_EQ("value2", *(result_two->Value("d", "column_2")));
}

TEST_F(FilterTest, GetWithQualifierFilter) {
  // write row1 with 3 columns (a,b,c)
  FilterTest::test_util_->CreateTable("t1", "d");
  FilterTest::test_util_->TablePut("t1", "row1", "d", "a", "value1");
  FilterTest::test_util_->TablePut("t1", "row1", "d", "b", "value2");
  FilterTest::test_util_->TablePut("t1", "row1", "d", "c", "value3");

  // Create TableName and Row to be fetched from HBase
  auto tn = folly::to<hbase::pb::TableName>("t1");
  auto row = "row1";

  // Gets to be performed on above HBase Table
  Get get(row);
  get.SetFilter(FilterFactory::QualifierFilter(CompareType::GREATER_OR_EQUAL,
                                               *ComparatorFactory::BinaryComparator("b")));

  // Create a client
  hbase::Client client(*(FilterTest::test_util_->conf()));

  // Get connection to HBase Table
  auto table = client.Table(tn);
  ASSERT_TRUE(table) << "Unable to get connection to Table.";

  // Perform the Get
  auto result = table->Get(get);

  table->Close();
  client.Close();

  // Test the values
  ASSERT_TRUE(!result->IsEmpty()) << "Result shouldn't be empty.";
  EXPECT_EQ(row, result->Row());
  EXPECT_EQ(2, result->Size());
  EXPECT_EQ("value2", *(result->Value("d", "b")));
  EXPECT_EQ("value3", *(result->Value("d", "c")));
}