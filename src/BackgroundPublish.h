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

#include <functional>
#include <queue>
#include "Particle.h"

extern const int NUM_ENTRIES;

enum class publishStatus{
        PUBLISH_COMPLETE = 0,
        PUBLISH_BUSY,
        PUBLISH_CLEANUP,
};

typedef std::function<void(publishStatus status,
    const char *event_name,
    const char *event_data,
    const void *event_context)> publish_completed_cb_t;

typedef struct {
    PublishFlags event_flags;
    publish_completed_cb_t completed_cb;
    const char* event_name;
    const char* event_data;
    const void* event_context;
} publish_event_t;

class BackgroundPublish {
public:

    static BackgroundPublish& instance() {
        static BackgroundPublish instance;
        return instance;
    }

    /**
     * @brief Initialize the BackgroundPublish
     *
     * @details Creates the background publish thread
     *
     */
    void init();

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
                const void* context = nullptr);
    
    template <typename T>
    bool publish(const char* name,
                const char* data = nullptr,
                PublishFlags flags = PRIVATE,
                int level = 0,
                void (T::*cb)(publishStatus status, const char *, const char *, const void *) = nullptr,
                T* instance = nullptr,
                const void* context = nullptr);
    
    /**
     * @brief Thread for the background publish
     *
     * @details Thread protected with lock_guard mutex to prevent the queues
     * being peeked or popped from in different contexts while attempting to 
     * publish
     */
    void thread_f();

    /**
     * @brief Iterate through the queues and make calls to the 
     * callback functions
     *
     * @details Will iterate through each queue taking an item from the queue
     * and calling it's callback function with a status of PUBLISH_CLEANUP.
     * Intended for a user provided callback to potentially key off of this 
     * PUBLISH_CLEANUP and back up a publish to flash, or take an other 
     * meaningful action
     */
    void cleanup();
    
    //remove copy and assignment operators
    BackgroundPublish(BackgroundPublish const&) = delete; 
    void operator=(BackgroundPublish const&)    = delete;

private:
    /**
     * @brief Creates the queues needed on construction, and stores them in the
     * _queues vector
     *
     * @details NUM_OF_QUEUES determines how many queues get created. Each queue
     * has a priority level determined by its index in the _queues vector. The
     * lower the index, the higher the priority
     */
    BackgroundPublish();

    /**
     * @brief Process the publish request
     *
     * @details This function is called from the thread_f() thread by the RTOS
     * 
     * @param[in] event event details to be used in Particle.publish() call
     *
     * @return publishStatus type for status of the Particle.publish() call
     */
    publishStatus process_publish(const publish_event_t& event);

    RecursiveMutex _mutex;
    Thread *_thread = nullptr;
    Vector<std::queue<publish_event_t>*> _queues;
};

/**
 * @brief To be checked by the thread_f to know if the thread should keep
 * running
 *
 * @details This function is weakly defined so it can be re-written to return
 * false for unit tests.
 *
 * @return TRUE keep thread running, FALSE stop thread from running
 */
bool keep_running();

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
bool BackgroundPublish::publish(const char* name,
                const char* data,
                PublishFlags flags,
                int level,
                void (T::*cb)(publishStatus status, const char *, const char *, const void *),
                T* instance,
                const void* context) {
    return publish(name, 
                data, 
                flags, 
                level, 
                std::bind(cb, instance, std::placeholders::_1, 
                    std::placeholders::_2, std::placeholders::_3, std::placeholders::_4), 
                context);
}
