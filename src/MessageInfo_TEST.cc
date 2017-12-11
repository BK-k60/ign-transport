/*
 * Copyright (C) 2016 Open Source Robotics Foundation
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

#include <string>

#include "ignition/transport/MessageInfo.hh"
#include "gtest/gtest.h"

using namespace ignition;

//////////////////////////////////////////////////
/// \brief Check [Set]Topic().
TEST(MessageInfoTest, topic)
{
  transport::MessageInfo info;
  EXPECT_TRUE(info.Topic().empty());

  std::string aTopic = "/foo";
  info.SetTopic(aTopic);
  EXPECT_EQ(info.Topic(), aTopic);
}

//////////////////////////////////////////////////
/// \brief Check [Set]Type().
TEST(MessageInfoTest, type)
{
  transport::MessageInfo info;
  EXPECT_TRUE(info.Type().empty());

  std::string aType = ".msg.foo";
  info.SetType(aType);
  EXPECT_EQ(aType, info.Type());
}

//////////////////////////////////////////////////
/// \brief Check [Set]Partition().
TEST(MessageInfoTest, partition)
{
  transport::MessageInfo info;
  EXPECT_TRUE(info.Partition().empty());

  std::string aPartition = "some_partition";
  info.SetPartition(aPartition);
  EXPECT_EQ(aPartition, info.Partition());
}

//////////////////////////////////////////////////
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
