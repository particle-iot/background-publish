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

#include "BackgroundPublish.h"

static Logger logger("background-publish");

BackgroundPublish::BackgroundPublish() :
    running {false},
    _thread()
{
}

void BackgroundPublish::init() {
    if (running) {
        logger.warn("init() called on running publisher");
        return;
    }
    running = true;
    _thread = Thread("background_publish",
                     std::bind(&BackgroundPublish::thread_f, this),
                     OS_THREAD_PRIORITY_DEFAULT);
}

void BackgroundPublish::stop() {
    if (!running) {
        logger.warn("stop() called on non-running publisher");
        return;
    }
    running = false;
    _thread.join();
    cleanup();
}

particle::Error BackgroundPublish::process_publish(const publish_event_t& event) {
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

void BackgroundPublish::thread_f() {
    constexpr system_tick_t process_interval {1000u};
    static system_tick_t process_time_ms = millis();

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

bool BackgroundPublish::publish(const char *name,
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
    if (level >= NUM_OF_QUEUES) {
        logger.error("Level:%d exceeds number of queues:%d", level, NUM_OF_QUEUES);
        return false;
    }

    std::lock_guard<RecursiveMutex> lock(_mutex);

    if(_queues[level].size() >= NUM_ENTRIES) {
        logger.error("Exceeds number of entries allowed");
        return false;
    }
    _queues[level].push(event_details);

    return true;
}

void BackgroundPublish::cleanup() {
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
