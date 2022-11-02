#include "BackgroundPublish.h"
#include <thread>

#define CATCH_CONFIG_MAIN
#include "catch.h"

std::string str = "Publish This";
int high_cb_counter;
int low_cb_counter;
particle::Error status_returned;

void priority_high_cb(particle::Error status,
    const char *event_name,
    const char *event_data,
    const void *event_context) {
    status_returned = status;
    high_cb_counter++;
}

void priority_low_cb(particle::Error status,
    const char *event_name,
    const char *event_data,
    const void *event_context) {
    status_returned = status;
    low_cb_counter++;
}

class TestBackgroundPublish : public BackgroundPublish {
public:
    void processOnce();
};

void TestBackgroundPublish::processOnce()
{
    constexpr system_tick_t process_interval {1000u};
    static system_tick_t process_time_ms = millis();

    auto now {millis()};
    if(now - process_time_ms >= process_interval) {
        for(auto &queue : _queues) {
            if(!queue.empty()) {
                process_time_ms = now;
                // Copy the event and pop so the publish is done without holding the mutex
                publish_event_t event {queue.front()};
                queue.pop();
                process_publish(event);
                break;
            }
        }
    }
}

TEST_CASE("Test Background Publish") {
    TestBackgroundPublish publisher;

    publisher.init();

    publisher.processOnce(); //run once at the top to establish
    //a start time for the processOnce

    for(int i = 0; i < NUM_ENTRIES; i++) {
        publisher.publish("TEST_PUB_HIGH",
                            str.c_str(),
                            PRIVATE,
                            1,
                            priority_high_cb);
    }
    REQUIRE(publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        2, 
                        priority_high_cb ) == false);

    //CANCELLED, run cleanup()
    high_cb_counter = 0;
    Particle.state_output.err = particle::Error::UNKNOWN;
    Particle.state_output.isDoneReturn = true;

    publisher.cleanup();
    REQUIRE(high_cb_counter == 8);
    REQUIRE(status_returned == particle::Error::CANCELLED);
    status_returned = particle::Error::UNKNOWN;

    //FAIL, Not enough levels/queues
    REQUIRE(publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        2, 
                        priority_high_cb ) == false);
    
    //PASS, enough levels/queues
    REQUIRE(publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_high_cb ) == true);
    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce(); //run to clear off the queues

    //Fail, not enough time passed between processing publishes
    //Then PASS once once time has passed.
    high_cb_counter = 0;
    low_cb_counter = 0;
    Particle.state_output.err = particle::Error::NONE;
    REQUIRE(publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb ) == true);
    System.inc(500); // not enough delay to process
    publisher.processOnce(); //run to clear off the queues
    REQUIRE(high_cb_counter == 0);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(status_returned == particle::Error::UNKNOWN);

    System.inc(500); //increase the tick by one second to allow processOnce to process
    publisher.processOnce(); //run to clear off the queues
    REQUIRE(high_cb_counter == 1);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(status_returned == particle::Error::NONE);
    status_returned = particle::Error::UNKNOWN;

    //LIMIT_EXCEEDED, run processOnce and fail on is.Succeeded()
    high_cb_counter = 0;
    low_cb_counter = 0;
    status_returned = particle::Error::UNKNOWN;
    Particle.state_output.err = particle::Error::LIMIT_EXCEEDED;
    publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(high_cb_counter == 1);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(status_returned == particle::Error::LIMIT_EXCEEDED);
    status_returned = particle::Error::UNKNOWN;

    //NONE, run processOnce and pass on is.Succeeded()
    high_cb_counter = 0;
    low_cb_counter = 0;
    Particle.state_output.isDoneReturn = true;
    Particle.state_output.err = particle::Error::NONE;
    publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(high_cb_counter == 1);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(status_returned.type() == particle::Error::NONE);
    status_returned = particle::Error::UNKNOWN; //clearout to something

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(high_cb_counter == 2);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(status_returned == particle::Error::NONE);
    status_returned = particle::Error::UNKNOWN; //clearout to something

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(high_cb_counter == 3);
    REQUIRE(low_cb_counter == 0);
    REQUIRE(status_returned == particle::Error::NONE);
    status_returned = particle::Error::UNKNOWN; //clearout to something

    //NONE, publish from high and low priority queues
    high_cb_counter = 0;
    low_cb_counter = 0;
    Particle.state_output.err = particle::Error::NONE;
    publisher.publish("TEST_PUB_LOW",
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_low_cb);
    publisher.publish("TEST_PUB_LOW",
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_low_cb);
    publisher.publish("TEST_PUB_LOW",
                        str.c_str(), 
                        PRIVATE,
                        1, 
                        priority_low_cb);
    publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);
    publisher.publish("TEST_PUB_HIGH",
                        str.c_str(), 
                        PRIVATE,
                        0, 
                        priority_high_cb);

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(low_cb_counter == 0);
    REQUIRE(high_cb_counter == 1);
    REQUIRE(status_returned == particle::Error::NONE);
    status_returned = particle::Error::UNKNOWN; //clear out to something

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(low_cb_counter == 0);
    REQUIRE(high_cb_counter == 2);
    REQUIRE(status_returned == particle::Error::NONE);
    status_returned = particle::Error::UNKNOWN; //clear out to something

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(low_cb_counter == 0);
    REQUIRE(high_cb_counter == 3);
    REQUIRE(status_returned == particle::Error::NONE);
    status_returned = particle::Error::UNKNOWN; //clear out to something

    Particle.state_output.err = particle::Error::LIMIT_EXCEEDED;
    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(low_cb_counter == 1);
    REQUIRE(high_cb_counter == 3);
    REQUIRE(status_returned == particle::Error::LIMIT_EXCEEDED);
    status_returned = particle::Error::UNKNOWN; //clear out to something

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(low_cb_counter == 2);
    REQUIRE(high_cb_counter == 3);
    REQUIRE(status_returned == particle::Error::LIMIT_EXCEEDED);
    status_returned = particle::Error::UNKNOWN;

    publisher.processOnce();
    REQUIRE(status_returned == particle::Error::UNKNOWN); // not enough delay to process

    System.inc(1000); //increase the tick by one second to allow processOnce to process
    publisher.processOnce();
    REQUIRE(status_returned == particle::Error::LIMIT_EXCEEDED);
    REQUIRE(low_cb_counter == 3);
    REQUIRE(high_cb_counter == 3);
}
