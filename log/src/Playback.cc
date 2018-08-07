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

#include <sqlite3.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

#include <ignition/transport/Node.hh>
#include <ignition/transport/log/Log.hh>
#include <ignition/transport/log/Playback.hh>
#include "build_config.hh"
#include "Console.hh"
#include "raii-sqlite3.hh"

using namespace ignition::transport;
using namespace ignition::transport::log;

// We check whether sqlite3 is potentially threadsafe. Note that this only
// knows whether sqlite3 was compiled with multi-threading capabilities. It
// might not catch changes to sqlite3's runtime settings.
// See: https://www.sqlite.org/threadsafe.html
static const bool kSqlite3Threadsafe = (sqlite3_threadsafe() != 0);

//////////////////////////////////////////////////
/// \brief Private implementation of Playback
class ignition::transport::log::Playback::Implementation
{
  /// \brief Constructor. Creates and initializes the log file
  /// \param[in] _file The full path of the file to open
  public: explicit Implementation(
    const std::string &_file, const NodeOptions &_nodeOptions)
    : logFile(std::make_shared<Log>()),
      addTopicWasUsed(false),
      nodeOptions(_nodeOptions)
  {
    if (!this->logFile->Open(_file, std::ios_base::in))
    {
      LERR("Could not open file [" << _file << "]\n");
    }
    else
    {
      LDBG("Playback opened file [" << _file << "]\n");
    }
  }

  /// \brief This gets used by RemoveTopic(~) to make sure we follow the correct
  /// behavior.
  void DefaultToAllTopics()
  {
    if (!this->addTopicWasUsed)
    {
      const Descriptor *desc = this->logFile->Descriptor();
      const Descriptor::NameToMap &allTopics = desc->TopicsToMsgTypesToId();
      for (const auto &entry : allTopics)
        this->topicNames.insert(entry.first);

      // Topics have been set, so we change this flag to true
      this->addTopicWasUsed = true;
    }
  }

  /// \brief log file to play from
  public: std::shared_ptr<Log> logFile;

  /// \brief topics that are being played back
  public: std::unordered_set<std::string> topicNames;

  /// \brief True if a call to either overload of AddTopic() has been made.
  public: bool addTopicWasUsed;

  /// \brief This is the last handle that was produced by the Playback object.
  /// This is only used to ensure safety in special cases where multi-threaded
  /// sqlite3 is known to be unavailable.
  public: std::weak_ptr<PlaybackHandle> lastHandle;

  /// \brief The node options.
  public: NodeOptions nodeOptions;
};

//////////////////////////////////////////////////
/// \brief Private implementation of PlaybackHandle
class PlaybackHandle::Implementation
{
  /// \brief Constructor
  /// \param[in] _logFile A reference to the Log instance
  /// \param[in] _topics A set of all topics to publish
  /// \param[in] _waitAfterAdvertising How long to wait after advertising the
  /// topics
  public: Implementation(
      const std::shared_ptr<Log> &_logFile,
      const std::unordered_set<std::string> &_topics,
      const std::chrono::nanoseconds &_waitAfterAdvertising,
      const NodeOptions &_nodeOptions);

  /// \brief Look through the types of data that _topic can publish and create
  /// a publisher for each type.
  /// \param[in] _topic Name of the topic to publish
  public: void AddTopic(const std::string &_topic);

  /// \brief Create a publisher of a given topic name and type
  /// \param[in] _topic Topic name to publish to
  /// \param[in] _type The message type name to publish
  public: void CreatePublisher(
      const std::string &_topic,
      const std::string &_type);

  /// \brief Begin playing messages in another thread
  /// \param[in] _batch The messages to be played back
  public: void StartPlayback(Batch _batch);

  /// \brief Stop the playback
  public: void Stop();

  /// \brief Wait until playback has finished playing
  public: void WaitUntilFinished();

  /// \brief node used to create publishers
  /// \note This member needs to come before the publishers member so that they
  /// get destructed in the correct order
  public: std::unique_ptr<ignition::transport::Node> node;

  /// \brief Map whose key is a topic name and value is another map whose
  /// key is a message type name and value is a publisher
  public: std::unordered_map<std::string,
          std::unordered_map<std::string,
            ignition::transport::Node::Publisher>> publishers;

  /// \brief a mutex to use when waiting for playback to finish
  public: std::mutex waitMutex;

  /// \brief Condition variable to wake up threads waiting on playback
  public: std::condition_variable waitConditionVariable;

  /// \brief Condition variable to wake up the playback thread if it's waiting
  /// to publish the next message.
  public: std::condition_variable stopConditionVariable;

  /// \brief True if the thread should be stopped
  public: std::atomic_bool stop;

  /// \brief This will be switched to true as the playback thread exits. This is
  /// different from `stop` because it cannot be changed by the user; it only
  /// changes exactly when the playback thread exits.
  public: std::atomic_bool finished;

  /// \brief thread running playback
  public: std::thread playbackThread;

  /// \brief log file to play from
  public: std::shared_ptr<Log> logFile;

  /// \brief mutex for thread safety with log file
  public: std::mutex logFileMutex;
};

//////////////////////////////////////////////////
Playback::Playback(const std::string &_file, const NodeOptions &_nodeOptions)
  : dataPtr(new Implementation(_file, _nodeOptions))
{
  // Do nothing
}

//////////////////////////////////////////////////
Playback::Playback(Playback &&_other)  // NOLINT
  : dataPtr(std::move(_other.dataPtr))
{
  // Do nothing
}

//////////////////////////////////////////////////
Playback::~Playback()
{
  // Do nothing
}

//////////////////////////////////////////////////
PlaybackHandlePtr Playback::Start(
    const std::chrono::nanoseconds &_waitAfterAdvertising) const
{
  if (!this->dataPtr->logFile->Valid())
  {
    LERR("Could not start: Failed to open log file\n");
    return nullptr;
  }

  if (!kSqlite3Threadsafe)
  {
    // If we know that threadsafety is not available, then we will insist on
    // not creating a new PlaybackHandle until the last one is finished.
    PlaybackHandlePtr lastHandle = this->dataPtr->lastHandle.lock();
    if (lastHandle && !lastHandle->Finished())
    {
      LWRN("You have linked to a single-threaded sqlite3. We can only spawn "
           "one PlaybackHandle at a time\n");
      return nullptr;
    }
  }

  std::unordered_set<std::string> topics;
  if (!this->dataPtr->addTopicWasUsed)
  {
    LDBG("No topics added, defaulting to all topics\n");
    const Descriptor *desc = this->dataPtr->logFile->Descriptor();
    const Descriptor::NameToMap &allTopics = desc->TopicsToMsgTypesToId();
    for (const auto &entry : allTopics)
      topics.insert(entry.first);
  }
  else
  {
    topics = this->dataPtr->topicNames;
  }

  PlaybackHandlePtr newHandle(
        new PlaybackHandle(
          std::make_unique<PlaybackHandle::Implementation>(
            this->dataPtr->logFile, topics, _waitAfterAdvertising,
            this->dataPtr->nodeOptions)));

  // We only need to store this if sqlite3 was not compiled in threadsafe mode.
  if (!kSqlite3Threadsafe)
    this->dataPtr->lastHandle = newHandle;

  return newHandle;
}

//////////////////////////////////////////////////
bool Playback::Valid() const
{
  return this->dataPtr->logFile->Valid();
}

//////////////////////////////////////////////////
bool Playback::AddTopic(const std::string &_topic)
{
  // We set this to true whether or not the function call succeeds, because by
  // calling this function, the user has expressed an intention to explicitly
  // specify which topics to publish.
  this->dataPtr->addTopicWasUsed = true;

  if (!this->dataPtr->logFile->Valid())
  {
    LERR("Failed to open log file\n");
    return false;
  }

  const Descriptor *desc = this->dataPtr->logFile->Descriptor();
  const Descriptor::NameToMap &allTopics = desc->TopicsToMsgTypesToId();

  const Descriptor::NameToMap::const_iterator it = allTopics.find(_topic);
  if (it == allTopics.end())
  {
    LWRN("Topic [" << _topic << "] is not in the log\n");
    return false;
  }

  this->dataPtr->topicNames.insert(_topic);

  return true;
}

//////////////////////////////////////////////////
int64_t Playback::AddTopic(const std::regex &_topic)
{
  // We set this to true whether or not the function call succeeds, because by
  // calling this function, the user has expressed an intention to explicitly
  // specify which topics to publish.
  this->dataPtr->addTopicWasUsed = true;

  if (!this->dataPtr->logFile->Valid())
  {
    LERR("Failed to open log file\n");
    return -1;
  }

  int64_t numMatches = 0;
  const Descriptor *desc = this->dataPtr->logFile->Descriptor();
  const Descriptor::NameToMap &allTopics = desc->TopicsToMsgTypesToId();

  for (const auto &topicEntry : allTopics)
  {
    const std::string &topic = topicEntry.first;
    if (!std::regex_match(topic, _topic))
      continue;

    this->dataPtr->topicNames.insert(topic);
    ++numMatches;
  }
  return numMatches;
}

//////////////////////////////////////////////////
bool Playback::RemoveTopic(const std::string &_topic)
{
  this->dataPtr->DefaultToAllTopics();

  return (this->dataPtr->topicNames.erase(_topic) > 0);
}

//////////////////////////////////////////////////
int64_t Playback::RemoveTopic(const std::regex &_topic)
{
  this->dataPtr->DefaultToAllTopics();

  uint64_t count = 0;
  std::unordered_set<std::string>::iterator it =
      this->dataPtr->topicNames.begin();

  while (it != this->dataPtr->topicNames.end())
  {
    if (std::regex_match(*it, _topic))
    {
      it = this->dataPtr->topicNames.erase(it);
      ++count;
    }
    else
    {
      ++it;
    }
  }

  return count;
}

//////////////////////////////////////////////////
PlaybackHandle::Implementation::Implementation(
    const std::shared_ptr<Log> &_logFile,
    const std::unordered_set<std::string> &_topics,
    const std::chrono::nanoseconds &_waitAfterAdvertising,
    const NodeOptions &_nodeOptions)
  : stop(true),
    finished(false),
    logFile(_logFile)
{
  this->node.reset(new transport::Node(_nodeOptions));

  for (const std::string &topic : _topics)
    this->AddTopic(topic);

  std::this_thread::sleep_for(_waitAfterAdvertising);

  Batch batch = this->logFile->QueryMessages(TopicList::Create(_topics));
  if (batch.begin() == batch.end())
  {
    LWRN("There are no messages to play\n");
  }

  this->StartPlayback(std::move(batch));
}

//////////////////////////////////////////////////
void PlaybackHandle::Implementation::AddTopic(
    const std::string &_topic)
{
  const Descriptor *desc = this->logFile->Descriptor();
  const Descriptor::NameToMap &allTopics = desc->TopicsToMsgTypesToId();

  const Descriptor::NameToMap::const_iterator it = allTopics.find(_topic);
  for (const auto &typeEntry : it->second)
  {
    const std::string &type = typeEntry.first;
    LDBG("Playing back [" << _topic << "] : [" << type << "]\n");
    this->CreatePublisher(_topic, type);
  }
}

//////////////////////////////////////////////////
void PlaybackHandle::Implementation::CreatePublisher(
    const std::string &_topic,
    const std::string &_type)
{
  auto firstMapIter = this->publishers.find(_topic);
  if (firstMapIter == this->publishers.end())
  {
    // Create a map for the message topic
    this->publishers[_topic] = std::unordered_map<std::string,
      ignition::transport::Node::Publisher>();
    firstMapIter = this->publishers.find(_topic);
  }

  auto secondMapIter = firstMapIter->second.find(_type);
  if (secondMapIter != firstMapIter->second.end())
  {
    // A publisher which matches this (topic, type) pair has already been
    // created, so we stop here.
    return;
  }

  // Create a publisher for the topic and type combo
  firstMapIter->second[_type] = this->node->Advertise(_topic, _type);
  LDBG("Creating publisher for " << _topic << " " << _type << "\n");
}

//////////////////////////////////////////////////
void PlaybackHandle::Implementation::WaitUntilFinished()
{
  if (this->logFile->Valid() && !this->stop)
  {
    std::unique_lock<std::mutex> lk(this->waitMutex);
    this->waitConditionVariable.wait(lk, [this]{return this->finished.load();});
  }
}

//////////////////////////////////////////////////
void PlaybackHandle::Implementation::StartPlayback(Batch _batch)
{
  this->stop = false;

  // Dev Note: batch is being initialized via the move-constructor within the
  // capture statement, as if it were being declared and constructed as
  // `auto batch{std::move(_batch)}`.
  //
  // For now, we need to use the move constructor because Batch does not yet
  // support copy construction. Also, the object named `_batch` will go out of
  // scope while this lambda is still in use, so we cannot rely on capturing it
  // by reference.
  this->playbackThread = std::thread(
    [this, batch{std::move(_batch)}] () mutable
    {
      bool publishedFirstMessage = false;

      // Get current elapsed on monotonic clock
      const std::chrono::nanoseconds startTime(
          std::chrono::steady_clock::now().time_since_epoch());
      std::chrono::nanoseconds firstMsgTime;

      std::lock_guard<std::mutex> playbackLock(this->logFileMutex);
      for (const Message &msg : batch)
      {
        if (this->stop)
        {
          break;
        }

        // Publish the first message right away, all others delay
        if (publishedFirstMessage)
        {
          const std::chrono::nanoseconds target =
              msg.TimeReceived() - firstMsgTime;

          // This will get initialized at if(!FinishedWaiting()) because the
          // FinishedWaiting lambda is capturing this by reference.
          std::chrono::nanoseconds now;

          // We create a lambda to test whether this thread needs to keep
          // waiting. This is used as a predicate by the
          // condition_variable::wait_for(~) function to avoid spurious wakeups.
          auto FinishedWaiting = [this, &startTime, &target, &now]() -> bool
          {
            now =
                std::chrono::steady_clock::now().time_since_epoch() - startTime;

            return target <= now || this->stop;
          };

          if (!FinishedWaiting())
          {
            // Passing a lock to wait_for is just a formality (we don't actually
            // want to unlock any mutex while waiting), so we create a temporary
            // mutex and lock to satisfy the function.
            //
            // According to the C++11 standard, it is undefined behavior to pass
            // different mutexes into the condition_variable::wait_for()
            // function of the same condition_variable instance from different
            // threads. However, this current thread should be the only thread
            // that is ever waiting on stopConditionVariable because we are
            // keeping playbackLock locked this whole time. Therefore, it's okay
            // for it lock its own local, unique mutex.
            //
            // This is really a substitute for the sleep_for function. This
            // alternative allows us to interrupt the sleep in case the user
            // calls Playback::Stop() while we are waiting between messages.
            std::mutex tempMutex;
            std::unique_lock<std::mutex> tempLock(tempMutex);
            this->stopConditionVariable.wait_for(
                  tempLock, target - now, FinishedWaiting);
          }
        }
        else
        {
          publishedFirstMessage = true;
          firstMsgTime = msg.TimeReceived();
        }

        // Actually publish the message
        LDBG("publishing\n");
        this->publishers[msg.Topic()][msg.Type()].PublishRaw(
            msg.Data(), msg.Type());
      }
      {
        this->finished = true;
        std::lock_guard<std::mutex> lk(this->waitMutex);
        this->stop = true;
      }
      this->waitConditionVariable.notify_all();
    });
}

//////////////////////////////////////////////////
void PlaybackHandle::Implementation::Stop()
{
  if (!this->logFile->Valid())
  {
    return;
  }

  this->stop = true;
  this->stopConditionVariable.notify_all();

  if (this->playbackThread.joinable())
    this->playbackThread.join();
}

//////////////////////////////////////////////////
PlaybackHandle::~PlaybackHandle()
{
  if (this->dataPtr)
  {
    this->dataPtr->Stop();
  }
}

//////////////////////////////////////////////////
void PlaybackHandle::Stop()
{
  this->dataPtr->Stop();
}

//////////////////////////////////////////////////
void PlaybackHandle::WaitUntilFinished()
{
  this->dataPtr->WaitUntilFinished();
}

//////////////////////////////////////////////////
bool PlaybackHandle::Finished() const
{
  return this->dataPtr->finished;
}

//////////////////////////////////////////////////
PlaybackHandle::PlaybackHandle(
  std::unique_ptr<Implementation> &&_internal) // NOLINT
  : dataPtr(std::move(_internal))
{
  // Do nothing
}
