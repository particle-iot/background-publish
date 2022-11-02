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
#include <functional>
#include <queue>
#include "Particle.h"

using publish_completed_cb_t = std::function<void(particle::Error status,
    const char *event_name,
    const char *event_data,
    const void *event_context)>;

template <std::size_t NumQueues = 2u, std::size_t MaxEntries = 8u>
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
    BackgroundPublish() : running {false}, _thread {} {}

    /**
     * @brief Initialize the publisher
     *
     * @details Creates the background publish thread
     *
     */
    void init();

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
     * @param[in] level priority level of message. Lowest is highest priority.
     *  Zero indexed
     * @param[in] cb callback on publish success or failure
     * @param[in] context could be a pointer to class (*this)
     *
     * @return TRUE if request accepted, FALSE if not
     */
    bool publish(const char* name,
                const char* data = nullptr,
                PublishFlags flags = PRIVATE,
                int level = 0,
                publish_completed_cb_t cb = nullptr,
                const void*
                context = nullptr);

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
     * @param[in] level priority level of message. Lowest is highest priority.
     *  Zero indexed
     * @param[in] cb callback on publish success or failure
     * @param[in] this invisible this pointer to the class the cb belongs to
     * @param[in] context could be a pointer to class (*this)
     *
     * @return TRUE if request accepted, FALSE if not
     */
    template <typename T>
    bool publish(const char* name,
                 const char* data = nullptr,
                 PublishFlags flags = PRIVATE,
                 int level = 0,
                 void (T::*cb)(particle::Error status, const char *, const char *, const void *) = nullptr,
                 T* instance = nullptr,
                 const void* context = nullptr)
    {
        return publish(name,
                       data,
                       flags,
                       level,
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
    struct publish_event_t {
        PublishFlags event_flags;
        publish_completed_cb_t completed_cb;
        const char* event_name;
        const char* event_data;
        const void* event_context;
    };

    std::array<std::queue<publish_event_t>, NumQueues> _queues;
    static particle::Error process_publish(const publish_event_t& event);

private:
    void thread_f();

    RecursiveMutex _mutex;
    bool running;
    Thread _thread;

    static Logger logger;
};

template <std::size_t NumQueues, std::size_t NumEntries>
Logger BackgroundPublish<NumQueues, NumEntries>::logger("background-publish");

template <std::size_t NumQueues, std::size_t NumEntries>
void BackgroundPublish<NumQueues, NumEntries>::init() {
    if (running) {
        logger.warn("init() called on running publisher");
        return;
    }
    running = true;
    _thread = Thread("background_publish",
                     std::bind(&BackgroundPublish::thread_f, this),
                     OS_THREAD_PRIORITY_DEFAULT);
}

template <std::size_t NumQueues, std::size_t NumEntries>
void BackgroundPublish<NumQueues, NumEntries>::stop() {
    if (!running) {
        logger.warn("stop() called on non-running publisher");
        return;
    }
    running = false;
    _thread.join();
    cleanup();
}

template <std::size_t NumQueues, std::size_t NumEntries>
particle::Error BackgroundPublish<NumQueues, NumEntries>::process_publish(const publish_event_t& event) {
    auto promise {Particle.publish(event.event_name,
                                   event.event_data,
                                   event.event_flags)};

    // Can't use promise.wait() outside of the application thread
    while(!promise.isDone()) {
        delay(1); // yield to other threads
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
            logger.error("Publish failed: %s", error.message());
        }
    }

    return error;
}

template <std::size_t NumQueues, std::size_t NumEntries>
void BackgroundPublish<NumQueues, NumEntries>::thread_f() {
    constexpr system_tick_t process_interval {1000u};
    system_tick_t process_time_ms = millis();

    while(running) {
        //Set to always start with the highest priority queue, and after each
        //publish to break out of the loop. This gaurantees that the highest
        //priority queue with items is processed first. If the highest priority
        //queue is empty it just iterates to the next priority
        //queue and so forth
        auto now {millis()};
        if(now - process_time_ms >= process_interval) {
            for(auto &queue : _queues) {
                _mutex.lock();
                if(!queue.empty()) {
                    process_time_ms = now;
                    // Copy the event and pop so the publish is done without holding the mutex
                    publish_event_t event {queue.front()};
                    queue.pop();
                    _mutex.unlock();
                    process_publish(event);
                    break;
                }
                _mutex.unlock();
            }
        }

        delay(1); // force yield to processor
    }

    // Exit thread
    os_thread_exit(nullptr);
}

template <std::size_t NumQueues, std::size_t NumEntries>
bool BackgroundPublish<NumQueues, NumEntries>::publish(const char *name,
                                const char *data,
                                PublishFlags flags,
                                int level,
                                publish_completed_cb_t cb,
                                const void *context) {
    publish_event_t event_details{};

    event_details.event_flags = flags;
    event_details.event_name = name;
    event_details.event_data = data;
    event_details.completed_cb = cb;
    event_details.event_context = context;

    if (!running) {
        logger.error("publisher not initialized");
        return false;
    }

    //make sure the level not greater than number of queues you can index
    if (level >= NumQueues) {
        logger.error("Level:%d exceeds number of queues:%d", level, NumQueues);
        return false;
    }

    std::lock_guard<RecursiveMutex> lock(_mutex);

    if(_queues[level].size() >= NumEntries) {
        logger.error("Exceeds number of entries allowed");
        return false;
    }
    _queues[level].push(event_details);

    return true;
}

template <std::size_t NumQueues, std::size_t NumEntries>
void BackgroundPublish<NumQueues, NumEntries>::cleanup() {
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
