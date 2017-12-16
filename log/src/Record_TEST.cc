/*
 * Copyright (C) 2017 Open Source Robotics Foundation
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

#include "ignition/transport/log/Record.hh"
#include "gtest/gtest.h"

using namespace ignition;

//////////////////////////////////////////////////
TEST(Record, Start)
{
  transport::log::Record recorder;
  EXPECT_EQ(transport::log::NO_ERROR, recorder.Start(":memory:"));
}

//////////////////////////////////////////////////
TEST(Record, StartImpossibleFilename)
{
  transport::log::Record recorder;
  EXPECT_EQ(transport::log::FAILED_TO_OPEN, recorder.Start("//////////"));
}

//////////////////////////////////////////////////
TEST(Record, DoubleStart)
{
  transport::log::Record recorder;
  EXPECT_EQ(transport::log::NO_ERROR, recorder.Start(":memory:"));
  EXPECT_EQ(transport::log::ALREADY_RECORDING, recorder.Start(":memory:"));
}

//////////////////////////////////////////////////
TEST(Record, StartStopStart)
{
  transport::log::Record recorder;
  EXPECT_EQ(transport::log::NO_ERROR, recorder.Start(":memory:"));
  recorder.Stop();
  EXPECT_EQ(transport::log::NO_ERROR, recorder.Start(":memory:"));
}

//////////////////////////////////////////////////
TEST(Record, AddValidTopic)
{
  transport::log::Record recorder;
  EXPECT_EQ(transport::log::NO_ERROR, recorder.AddTopic(std::string("/foo")));
}

//////////////////////////////////////////////////
TEST(Record, AddInvalidTopic)
{
  transport::log::Record recorder;
  EXPECT_EQ(transport::log::FAILED_TO_SUBSCRIBE,
      recorder.AddTopic(std::string("/////")));
}

//////////////////////////////////////////////////
TEST(Record, AddTopicRegex)
{
  transport::log::Record recorder;
  EXPECT_EQ(0, recorder.AddTopic(std::regex("////")));
}

//////////////////////////////////////////////////
int main(int argc, char **argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
