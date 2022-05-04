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

    BackgroundPublish::instance().thread_f(); //run once at the top to establish
    //a start time for the thread_f

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
    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f(); //run to clear off the queues

    //Fail, not enough time passed between processing publishes
    //Then PASS once once time has passed.
    high_cb_counter = 0;
    low_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_COMPLETE;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = true;
    REQUIRE(BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb ) == true);
    System.inc(500); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f(); //run to clear off the queues
    REQUIRE(high_cb_counter == 0);
    REQUIRE(low_cb_counter == 0);
    System.inc(500); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f(); //run to clear off the queues
    REQUIRE(high_cb_counter == 1);
    REQUIRE(low_cb_counter == 0);

    //PUBLISH_BUSY, run thread_f and fail on is.Succeeded()
    high_cb_counter = 0;
    low_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_COMPLETE;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = false;
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(high_cb_counter == 1);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);

    //PUBLISH_COMPLETE, run thread_f and pass on is.Succeeded()
    high_cb_counter = 0;
    low_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_BUSY;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = true;
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    status_returned = publishStatus::PUBLISH_BUSY; //clearout to something

    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    status_returned = publishStatus::PUBLISH_BUSY; //clearout to something

    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    status_returned = publishStatus::PUBLISH_BUSY; //clearout to something
    REQUIRE(high_cb_counter == 3);
    REQUIRE(low_cb_counter == 0);

    //PUBLISH_COMPLETE, publish from high and low priority queues
    high_cb_counter = 0;
    low_cb_counter = 0;
    status_returned = publishStatus::PUBLISH_CLEANUP;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.isSucceededReturn = true;
    BackgroundPublish::instance().publish("TEST_PUB_LOW", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_low_cb);
    BackgroundPublish::instance().publish("TEST_PUB_LOW", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_low_cb);
    BackgroundPublish::instance().publish("TEST_PUB_LOW", 
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_low_cb);

    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    BackgroundPublish::instance().publish("TEST_PUB_HIGH", 
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(high_cb_counter == 1);
    status_returned = publishStatus::PUBLISH_CLEANUP; //clear out to something

    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(high_cb_counter == 2);
    status_returned = publishStatus::PUBLISH_CLEANUP; //clear out to something

    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_COMPLETE);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(high_cb_counter == 3);
    status_returned = publishStatus::PUBLISH_CLEANUP; //clear out to something

    Particle.state_output.isSucceededReturn = false;
    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    REQUIRE(low_cb_counter == 1);
    REQUIRE(high_cb_counter == 3);
    status_returned = publishStatus::PUBLISH_CLEANUP; //clear out to something

    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    REQUIRE(low_cb_counter == 2);
    REQUIRE(high_cb_counter == 3);
    status_returned = publishStatus::PUBLISH_CLEANUP; //clear out to something

    System.inc(1000); //increase the tick by one second to allow thread_f to process
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    BackgroundPublish::instance().thread_f();
    REQUIRE(status_returned == publishStatus::PUBLISH_BUSY);
    REQUIRE(low_cb_counter == 3);
    REQUIRE(high_cb_counter == 3);
    status_returned = publishStatus::PUBLISH_CLEANUP; //clear out to something
}