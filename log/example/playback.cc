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

/// \brief Example of playing back all messages from a log
/// This will create publishers for all topics in a file and publish them
/// with the same timing that they were received.
/// Launch a the ignition-transport subscriber example if the log was created
/// by recording the publisher example.

#include <iostream>
#include <regex>
#include <ignition/transport/log/Playback.hh>

//////////////////////////////////////////////////
int main(int argc, char *argv[])
{
  if (argc != 2)
  {
    std::cerr << "Usage: " << argv[0] << " INPUT.tlog\n";
    return -1;
  }

  ignition::transport::log::Playback player(argv[1]);

  // Playback all topics
  player.AddTopic(std::regex(".*"));

  // Begin recording, saving received messages to the given file
  auto result = player.Start();
  if (ignition::transport::log::PlaybackError::SUCCESS != result)
  {
    std::cerr << "Failed to start playback: " << static_cast<int64_t>(result)
      << "\n";
    return -1;
  }

  // Wait until the player stops on its own
  std::cout << "Playing all messages in the log file\n";
  player.WaitUntilFinished();
  return 0;
}
