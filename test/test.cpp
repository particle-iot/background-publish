#include "BackgroundPublish.h"
#include <thread>

#define CATCH_CONFIG_MAIN
#include "catch.h"

std::string str = "Publish This";
int high_cb_counter;
int low_cb_counter;
publishStatus status_returned;

bool keep_running() {
    return false;
}

void priority_high_cb(publishStatus status,
    const char *event_name,
    const char *event_data,
    const void *event_context) {
    status_returned = status;
    high_cb_counter++;
}

void priority_low_cb(publishStatus status,
    const char *event_name,
    const char *event_data,
    const void *event_context) {
    status_returned = status;
    low_cb_counter++;
}

TEST_CASE("Test Background Publish") {
    BackgroundPublish::instance().init();

    //FAIL, not enough entries
    for(int i = 0; i < NUM_ENTRIES; i++) {
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);
    }
    REQUIRE(BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        2, 
                        priority_high_cb ) == false);

    //PUBLISH_CLEANUP, run cleanup()
    high_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_BUSY;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = true;

    BackgroundPublish::instance().cleanup();
    REQUIRE(high_cb_counter == 8);
    REQUIRE(status_returned == publishStatus::PUBLISH_CLEANUP); 

    //FAIL, Not enough levels/queues
    REQUIRE(BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        2, 
                        priority_high_cb ) == false);
    
    //PASS, enough levels/queues
    REQUIRE(BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb ) == true);
    BackgroundPublish::instance().thread_f(); //run to clear off of the queues

    //PUBLISH_BUSY, run thread_f and fail on is.Succeeded()
    high_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_COMPLETE;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = false;
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);

    BackgroundPublish::instance().thread_f();
    REQUIRE(high_cb_counter == 1);
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);

    //PUBLISH_COMPLETE, run thread_f and pass on is.Succeeded()
    high_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_BUSY;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = true;
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);

    BackgroundPublish::instance().thread_f();
    BackgroundPublish::instance().thread_f();
    BackgroundPublish::instance().thread_f();
    REQUIRE(high_cb_counter == 3);
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);

    //PUBLISH_COMPLETE, publish from high and low priority queues
    high_cb_counter = 0;
    low_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_BUSY;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = true;
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb);

    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_low_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_low_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_low_cb);

    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    Particle.state_output.isSucceededReturn = false;
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    REQUIRE(high_cb_counter == 3);
    REQUIRE(low_cb_counter == 3);
}