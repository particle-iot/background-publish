/*
 * Copyright (c) 2022 Particle Industries, Inc.
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
 */

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <queue>

#include "Particle.h"

using publish_completed_cb_t = std::function<void(particle::Error status,
    const char *event_name,
    const char *event_data,
    const void *event_context)>;

template<std::size_t NumQueues = 2u>
class BackgroundPublish {
public:
    /**
     * @brief Creates the queues needed on construction, and stores them in the
     * _queues vector
     *
     * @details NUM_OF_QUEUES determines how many queues get created. Each queue
     * has a priority level determined by its index in the _queues vector. The
     * lower the index, the higher the priority
     */
    BackgroundPublish(std::size_t max_entries = 8u) : running {false}, maxEntries {max_entries}, _thread()  {}

    /**
     * @brief Start the publisher
     *
     * @details Creates the background publish thread
     *
     */
    void start();

    /**
     * @brief Stop the publisher
     *
     * @details Clean up the queues and stop the background publish thread
     */
    void stop();

    /**
     * @brief Request a publish message to the cloud
     *
     * @details Puts the event details for the request in the corresponding
     * queue depending on what priority level the message is set to. Number 
     * of priority levels is determined by the NUM_OF_QUEUES macros. The lower
     * the priority level the higher the priority of the message. The level is
     * used to access the _queues vector as an index
     *
     * @param[in] name of the event requested
     * @param[in] data pointer to data to send
     * @param[in] flags PublishFlags type for the request
     * @param[in] priority priority of message. Lowest is highest priority, zero indexed
     * @param[in] cb callback on publish success or failure
     * @param[in] context could be a pointer to class (*this)
     *
     * @return TRUE if request accepted, FALSE if not
     */
    bool publish(const char* name,
                 const char* data = nullptr,
                 PublishFlags flags = PRIVATE,
                 std::size_t priority = 0u,
                 publish_completed_cb_t cb = nullptr,
                 const void *context = nullptr);

    /**
     * @brief Wrapper class for callbacks that are for non-static functions
     * Request a publish message to the cloud
     *
     * @details Puts the event details for the request in the corresponding
     * queue depending on what priority level the message is set to. Number
     * of priority levels is determined by the NUM_OF_QUEUES macros. The lower
     * the priority level the higher the priority of the message. The level is
     * used to access the _queues vector as an index
     *
     * @param[in] name of the event requested
     * @param[in] data pointer to data to send
     * @param[in] flags PublishFlags type for the request
     * @param[in] priority priority of message. Lowest is highest priority, zero indexed
     * @param[in] cb callback on publish success or failure
     * @param[in] this invisible this pointer to the class the cb belongs to
     * @param[in] context could be a pointer to class (*this)
     *
     * @return TRUE if request accepted, FALSE if not
     */
    template<typename T>
    bool publish(const char* name,
                 const char* data = nullptr,
                 PublishFlags flags = PRIVATE,
                 std::size_t priority = 0u,
                 void (T::*cb)(particle::Error status, const char *, const char *, const void *) = nullptr,
                 T* instance = nullptr,
                 const void* context = nullptr)
    {
        return publish(name,
                       data,
                       flags,
                       priority,
                       std::bind(cb, instance, std::placeholders::_1,
                                 std::placeholders::_2, std::placeholders::_3, std::placeholders::_4),
                       context);
    }

    /**
     * @brief Iterate through the queues and make calls to the 
     * callback functions
     *
     * @details Will iterate through each queue taking an item from the queue
     * and calling it's callback function with a status of CANCELLED.
     * Intended for a user provided callback to potentially key off of this 
     * CANCELLED and back up a publish to flash, or take an other
     * meaningful action
     */
    void cleanup();
    
    //remove copy and assignment operators
    BackgroundPublish(BackgroundPublish const&) = delete; 
    void operator=(BackgroundPublish const&)    = delete;

protected:
    // Define these as protected so the test case can access these to simulate the processing thread
    struct publish_event_t {
        PublishFlags event_flags;
        publish_completed_cb_t completed_cb;
        const void *event_context;
        char event_name[particle::protocol::MAX_EVENT_NAME_LENGTH + 1];
        char event_data[particle::protocol::MAX_EVENT_DATA_LENGTH + 1];
    };

    std::array<std::queue<publish_event_t>, NumQueues> _queues;
    static particle::Error process_publish(const publish_event_t& event);

private:
    void thread();

    RecursiveMutex _mutex;
    bool running;
    Thread _thread;
    std::size_t maxEntries;

    static Logger logger;
};

template<std::size_t NumQueues>
Logger BackgroundPublish<NumQueues>::logger("background-publish");

template<std::size_t NumQueues>
void BackgroundPublish<NumQueues>::start()
{
    if (running) {
        logger.warn("start() called on running publisher");
        return;
    }
    running = true;
    _thread = Thread("background_publish",
                     std::bind(&BackgroundPublish::thread, this),
                     OS_THREAD_PRIORITY_DEFAULT);
}

template<std::size_t NumQueues>
void BackgroundPublish<NumQueues>::stop()
{
    if (!running) {
        logger.warn("stop() called on non-running publisher");
        return;
    }
    running = false;
    _thread.join();
    cleanup();
}

template<std::size_t NumQueues>
particle::Error BackgroundPublish<NumQueues>::process_publish(const publish_event_t& event)
{
    auto promise {Particle.publish(event.event_name,
                                   event.event_data,
                                   event.event_flags)};

    // Can't use promise.wait() outside of the application thread
    while(!promise.isDone()) {
        delay(2); // yield to other threads
    }
    auto error {promise.error()};

    if(event.completed_cb != nullptr) {
        event.completed_cb(error,
                           event.event_name,
                           event.event_data,
                           event.event_context);
    } else {
        if (error != particle::Error::NONE) {
            // log error if no callback is used
            logger.error("publish failed: %s", error.message());
        }
    }

    return error;
}

template<std::size_t NumQueues>
void BackgroundPublish<NumQueues>::thread() {
    constexpr std::size_t burst_rate {2u}; // allowable burst rate (Hz), Device OS allows up to 4/s
    constexpr system_tick_t process_interval {1000u};

    system_tick_t publish_t[burst_rate] {}; // publish time of the last (burst_rate) sends in a circular buffer
    std::size_t i {}; // publish time of the previous (burst_rate)th send

    while(running) {
        auto now {millis()};
        if(now - publish_t[i] >= process_interval) {
            for(auto &queue : _queues) {
                _mutex.lock();
                if(!queue.empty()) {
                    publish_t[i] = now;
                    i = (i + 1) % burst_rate;
                    // Copy the event and pop so the publish and wait is done without holding the mutex
                    publish_event_t event {queue.front()};
                    queue.pop();
                    _mutex.unlock();
                    process_publish(event);
                    break;
                }
                _mutex.unlock();
            }
        }

        delay(2); // force yield to processor
    }
}

template<std::size_t NumQueues>
bool BackgroundPublish<NumQueues>::publish(const char *name,
                                           const char *data,
                                           PublishFlags flags,
                                           std::size_t priority,
                                           publish_completed_cb_t cb,
                                           const void *context)
{
    if (!running) {
        logger.error("publisher not initialized");
        return false;
    }

    if (priority >= NumQueues) {
        logger.error("priority %d exceeds number of queues %d", priority, NumQueues);
        return false;
    }

    std::lock_guard<RecursiveMutex> lock(_mutex);

    if(_queues[priority].size() >= maxEntries) {
        logger.error("queue at priority %d is full");
        return false;
    }
    _queues[priority].emplace();
    auto &event {_queues[priority].back()};
    event.event_flags = flags;
    event.completed_cb = cb;
    event.event_context = context;
    std::strncpy(event.event_name, name, sizeof(event.event_name));
    event.event_name[sizeof(event.event_name) - 1] = '\0';
    if (data != nullptr) {
        std::strncpy(event.event_data, data, sizeof(event.event_data));
        event.event_data[sizeof(event.event_data) - 1] = '\0';
    } else {
        event.event_data[0] = '\0';
    }

    return true;
}

template<std::size_t NumQueues>
void BackgroundPublish<NumQueues>::cleanup()
{
    std::lock_guard<RecursiveMutex> lock(_mutex);

    for(auto &queue : _queues) {
        while(!queue.empty()) {
            publish_event_t &event {queue.front()};
            if(event.completed_cb != nullptr) {
                event.completed_cb(particle::Error::CANCELLED,
                            event.event_name,
                            event.event_data,
                            event.event_context);
            }
            queue.pop();
        }
    }
}
