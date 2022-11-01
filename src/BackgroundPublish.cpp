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

constexpr int NUM_OF_QUEUES {2};
constexpr system_tick_t PROCESS_QUEUE_INTERVAL_MS {1000};
constexpr int NUM_ENTRIES {8};

BackgroundPublish::BackgroundPublish() {
    for(int i = 0; i < NUM_OF_QUEUES; i++) {
        std::queue<publish_event_t>* queue_ptr = 
                        new (std::nothrow) std::queue<publish_event_t>;
        if(queue_ptr != nullptr) {
            if(!_queues.append(queue_ptr)) {
                Log.error("Failed to append queue to vector");
            }
        }
    }
}

void BackgroundPublish::init() {
    if(_thread == nullptr) {
        _thread = new (std::nothrow) Thread("background_publish", 
                                        [this]() {thread_f();}, 
                                        OS_THREAD_PRIORITY_DEFAULT);
    }
}

void BackgroundPublish::thread_f() {
    static system_tick_t process_time_ms = millis();

    do {
        //Set to always start with the highest priority queue, and after each
        //publish to break out of the loop. This gaurantees that the highest
        //priority queue with items is processed first. If the highest priority
        //queue is empty it just iterates to the next priority 
        //queue and so forth
        if(millis() - process_time_ms >= PROCESS_QUEUE_INTERVAL_MS) {
            for(auto queue : _queues) {
                _mutex.lock();
                if(!queue->empty()) {
                    process_time_ms = millis();
                    publish_event_t event = queue->front(); //return an element
                    queue->pop(); //remove the element
                    _mutex.unlock();
                    process_publish(event);
                    break;
                }
                _mutex.unlock();
            }
        }

        // Minimal force yield to processor
        delay(1); 

    } while(keep_running());

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
    std::lock_guard<RecursiveMutex> lock(_mutex);
    
    event_details.event_flags = flags;
    event_details.event_name = name;
    event_details.event_data = data;
    event_details.completed_cb = cb;
    event_details.event_context = context;

    //make sure the level not greater than number of queues you can index
    if (level >= NUM_OF_QUEUES) {
        Log.error("Level:%d exceeds number of queues:%d", level, NUM_OF_QUEUES);
        return false;
    }
    if(_queues.at(level)->size() >= NUM_ENTRIES) {
        Log.error("Exceeds number of entries allowed");
        return false;
    }
    _queues.at(level)->push(event_details);
    Log.info("Publish request accepted");

    return true;
}

void BackgroundPublish::cleanup() {
    std::lock_guard<RecursiveMutex> lock(_mutex);

    for(auto queue : _queues) {
        while(!queue->empty()) {
            publish_event_t &event {queue->front()};
            if(event.completed_cb != nullptr) {
                event.completed_cb(publishStatus::PUBLISH_CLEANUP, 
                            event.event_name, 
                            event.event_data, 
                            event.event_context);
            }
            queue->pop();
        }
    }
}

publishStatus BackgroundPublish::process_publish(const publish_event_t& event) {
    publishStatus status;
    auto ok = Particle.publish(event.event_name, 
                            event.event_data, 
                            event.event_flags);
    while(!ok.isDone()) { //yield to other threads if not done publishing
        delay(1);
    }

    if(ok.isSucceeded()) {
        status = publishStatus::PUBLISH_COMPLETE;
        Log.info("Publish succeeded");
    }
    else {
        status = publishStatus::PUBLISH_BUSY;
        Log.warn("Publish busy/failed");
    }

    if(event.completed_cb != nullptr) {       
        event.completed_cb(status, 
                        event.event_name, 
                        event.event_data, 
                        event.event_context);                        
    }

    return status;
}

bool __attribute__((weak)) keep_running() {
    return true;
}
