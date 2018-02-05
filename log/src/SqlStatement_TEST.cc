/*
 * Copyright (C) 2018 Open Source Robotics Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
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

#include "ignition/transport/log/SqlStatement.hh"
#include "gtest/gtest.h"

using namespace ignition;


//////////////////////////////////////////////////
TEST(SqlParameter, Construct)
{
  EXPECT_EQ(transport::log::SqlParameter::ParamType::NULL_TYPE,
      transport::log::SqlParameter().Type());
  EXPECT_EQ(transport::log::SqlParameter::ParamType::NULL_TYPE,
      transport::log::SqlParameter(nullptr).Type());
  EXPECT_EQ(transport::log::SqlParameter::ParamType::INTEGER,
      transport::log::SqlParameter(42L).Type());
  EXPECT_EQ(transport::log::SqlParameter::ParamType::REAL,
      transport::log::SqlParameter(3.14159).Type());
  EXPECT_EQ(transport::log::SqlParameter::ParamType::TEXT,
      transport::log::SqlParameter("Hello World!").Type());
}


//////////////////////////////////////////////////
TEST(SqlParameter, Set)
{
  transport::log::SqlParameter param;
  param.Set("Hello World!");
  EXPECT_EQ(transport::log::SqlParameter::ParamType::TEXT, param.Type());
  param.Set(3.14159);
  EXPECT_EQ(transport::log::SqlParameter::ParamType::REAL, param.Type());
  param.Set(42L);
  EXPECT_EQ(transport::log::SqlParameter::ParamType::INTEGER, param.Type());
  param.Set(nullptr);
  EXPECT_EQ(transport::log::SqlParameter::ParamType::NULL_TYPE, param.Type());
}


//////////////////////////////////////////////////
TEST(SqlParameter, QueryText)
{
  transport::log::SqlParameter param;
  param.Set("Hello World!");
  EXPECT_EQ(std::string("Hello World!"), *param.QueryText());
  param.Set(42L);
  EXPECT_EQ(nullptr, param.QueryText());
  param.Set(3.14159);
  EXPECT_EQ(nullptr, param.QueryText());
  param.Set(nullptr);
  EXPECT_EQ(nullptr, param.QueryText());
}


//////////////////////////////////////////////////
TEST(SqlParameter, QueryInteger)
{
  transport::log::SqlParameter param;
  param.Set("Hello World!");
  EXPECT_EQ(nullptr, param.QueryInteger());
  param.Set(42L);
  EXPECT_EQ(42L, *param.QueryInteger());
  param.Set(3.14159);
  EXPECT_EQ(nullptr, param.QueryInteger());
  param.Set(nullptr);
  EXPECT_EQ(nullptr, param.QueryInteger());
}


//////////////////////////////////////////////////
TEST(SqlParameter, QueryReal)
{
  transport::log::SqlParameter param;
  param.Set("Hello World!");
  EXPECT_EQ(nullptr, param.QueryReal());
  param.Set(42L);
  EXPECT_EQ(nullptr, param.QueryReal());
  param.Set(3.14159);
  EXPECT_DOUBLE_EQ(3.14159, *param.QueryReal());
  param.Set(nullptr);
  EXPECT_EQ(nullptr, param.QueryReal());
}

//////////////////////////////////////////////////
TEST(SqlParameter, MoveConstructor)
{
  transport::log::SqlParameter param_orig;
  param_orig.Set(3.14159);
  transport::log::SqlParameter param(std::move(param_orig));
  ASSERT_NE(nullptr, param.QueryReal());
  EXPECT_DOUBLE_EQ(3.14159, *param.QueryReal());
}


//////////////////////////////////////////////////
TEST(SqlParameter, CopyAssignment)
{
  transport::log::SqlParameter param_orig;
  param_orig.Set("Hello World!");
  transport::log::SqlParameter param = param_orig;
  ASSERT_NE(nullptr, param_orig.QueryText());
  ASSERT_NE(nullptr, param.QueryText());
  EXPECT_EQ(std::string("Hello World!"), *param_orig.QueryText());
  EXPECT_EQ(std::string("Hello World!"), *param.QueryText());
}


//////////////////////////////////////////////////
TEST(SqlParameter, MoveAssignment)
{
  transport::log::SqlParameter param_orig;
  param_orig.Set("Hello World!");
  transport::log::SqlParameter param = std::move(param_orig);
  ASSERT_NE(nullptr, param.QueryText());
  EXPECT_EQ(std::string("Hello World!"), *param.QueryText());
}


//////////////////////////////////////////////////
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

