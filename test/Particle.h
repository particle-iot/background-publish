#pragma once

#include <functional>
#include "spark_wiring_vector.h"
#include <chrono>
#include <mutex>
#include "concurrent_hal.h"

// List of all defined system errors
#define SYSTEM_ERROR_NONE                   (0)
#define SYSTEM_ERROR_UNKNOWN                (-100)
#define SYSTEM_ERROR_BUSY                   (-110)
#define SYSTEM_ERROR_NOT_SUPPORTED          (-120)
#define SYSTEM_ERROR_NOT_ALLOWED            (-130)
#define SYSTEM_ERROR_CANCELLED              (-140)
#define SYSTEM_ERROR_ABORTED                (-150)
#define SYSTEM_ERROR_TIMEOUT                (-160)
#define SYSTEM_ERROR_NOT_FOUND              (-170)
#define SYSTEM_ERROR_ALREADY_EXISTS         (-180)
#define SYSTEM_ERROR_TOO_LARGE              (-190)
#define SYSTEM_ERROR_NOT_ENOUGH_DATA        (-191)
#define SYSTEM_ERROR_LIMIT_EXCEEDED         (-200)
#define SYSTEM_ERROR_END_OF_STREAM          (-201)
#define SYSTEM_ERROR_INVALID_STATE          (-210)
#define SYSTEM_ERROR_IO                     (-220)
#define SYSTEM_ERROR_WOULD_BLOCK            (-221)
#define SYSTEM_ERROR_FILE                   (-225)
#define SYSTEM_ERROR_NETWORK                (-230)
#define SYSTEM_ERROR_PROTOCOL               (-240)
#define SYSTEM_ERROR_INTERNAL               (-250)
#define SYSTEM_ERROR_NO_MEMORY              (-260)
#define SYSTEM_ERROR_INVALID_ARGUMENT       (-270)
#define SYSTEM_ERROR_BAD_DATA               (-280)
#define SYSTEM_ERROR_OUT_OF_RANGE           (-290)
#define SYSTEM_ERROR_DEPRECATED             (-300)
#define SYSTEM_ERROR_COAP                   (-1000)
#define SYSTEM_ERROR_COAP_4XX               (-1100)
#define SYSTEM_ERROR_COAP_5XX               (-1132)
#define SYSTEM_ERROR_AT_NOT_OK              (-1200)
#define SYSTEM_ERROR_AT_RESPONSE_UNEXPECTED (-1210)

#define HAL_I2C_DEFAULT_TIMEOUT_MS (100)
#define I2C_BUFFER_LENGTH          (uint8_t)32

typedef uint32_t system_tick_t;
typedef uint16_t pin_t;

typedef enum PinMode {
    INPUT = 0,
    OUTPUT = 1,
    INPUT_PULLUP = 2,
    INPUT_PULLDOWN = 3,
    AF_OUTPUT_PUSHPULL = 4, // Used internally for Alternate Function Output PushPull(TIM, UART, SPI etc)
    AF_OUTPUT_DRAIN = 5,    // Used internally for Alternate Function Output Drain(I2C etc). External pullup resistors required.
    AN_INPUT = 6,           // Used internally for ADC Input
    AN_OUTPUT = 7,          // Used internally for DAC Output,
    OUTPUT_OPEN_DRAIN = AF_OUTPUT_DRAIN,
    OUTPUT_OPEN_DRAIN_PULLUP = 8,
    PIN_MODE_NONE = 0xFF
} PinMode;

/*! I2c Config Structure Version */
typedef enum hal_i2c_config_version_t {
    HAL_I2C_CONFIG_VERSION_1 = 0,
    HAL_I2C_CONFIG_VERSION_LATEST = HAL_I2C_CONFIG_VERSION_1,
} hal_i2c_config_version_t;

typedef enum hal_i2c_interface_t {
    HAL_I2C_INTERFACE1 = 0,
    HAL_I2C_INTERFACE2 = 1,
    HAL_I2C_INTERFACE3 = 2
} hal_i2c_interface_t;

typedef struct hal_i2c_transmission_config_t {
    uint16_t size;
    uint16_t version;
    uint8_t address;
    uint8_t reserved[3];
    uint32_t quantity;
    system_tick_t timeout_ms;
    uint32_t flags;
} hal_i2c_transmission_config_t;

typedef struct hal_i2c_config_t {
    uint16_t size;
    uint16_t version;
    uint8_t* rx_buffer;
    uint32_t rx_buffer_size;
    uint8_t* tx_buffer;
    uint32_t tx_buffer_size;
} hal_i2c_config_t;

typedef enum hal_i2c_transmission_flag_t {
    HAL_I2C_TRANSMISSION_FLAG_NONE = 0x00,
    HAL_I2C_TRANSMISSION_FLAG_STOP = 0x01
} hal_i2c_transmission_flag_t;

class WireTransmission {
public:
  WireTransmission(uint8_t address)
      : address_{address},
        size_{0},
        stop_{true},
        timeout_{HAL_I2C_DEFAULT_TIMEOUT_MS} {
  }

  WireTransmission() = delete;

  WireTransmission& quantity(size_t size) {
    size_ = size;
    return *this;
  }

  WireTransmission& timeout(system_tick_t ms) {
    timeout_ = ms;
    return *this;
  }

  WireTransmission& timeout(std::chrono::milliseconds ms) {
    return timeout((system_tick_t)ms.count());
  }

  WireTransmission& stop(bool stop) {
    stop_ = stop;
    return *this;
  }

hal_i2c_transmission_config_t halConfig() const {
    hal_i2c_transmission_config_t conf = {
      .size = sizeof(hal_i2c_transmission_config_t),
      .version = 0,
      .address = address_,
      .reserved = {0},
      .quantity = (uint32_t)size_,
      .timeout_ms = timeout_,
      .flags = (uint32_t)(stop_ ? HAL_I2C_TRANSMISSION_FLAG_STOP : 0)
    };
    return conf;
}

private:
  uint8_t address_;
  size_t size_;
  bool stop_;
  system_tick_t timeout_;
};

namespace particle {

template<bool S, size_t bits, typename T>
struct bits_fit_in_type {
    using type = typename std::conditional<S, typename std::make_signed<T>::type, typename std::make_unsigned<T>::type>::type;
    static const bool value = (bits <= std::numeric_limits<type>::digits);
};
} //particle

enum endTransmissionReturns: uint8_t {
    SUCCESS = 0,
    TIMEOUT = 1,
};

class TwoWire
{
private:
    hal_i2c_interface_t _i2c;
    size_t index;
public:
    TwoWire(hal_i2c_interface_t i2c, const hal_i2c_config_t& config) {};
    ~TwoWire() {};
    inline void setClock(uint32_t speed) {
        setSpeed(speed);
    }
    void setSpeed(uint32_t) {};
    void enableDMAMode(bool);
    void stretchClock(bool);
    void begin() {};
    void begin(uint8_t) {};
    void begin(int) {};
    void beginTransmission(uint8_t) {};
    void beginTransmission(int) {};
    void beginTransmission(const WireTransmission& transfer) {};
    void end();
    uint8_t endTransmission(void) {return end_transmission_return;}
    uint8_t endTransmission(uint8_t) {return end_transmission_return;}
    size_t requestFrom(uint8_t, size_t) {return num_bytes_to_read;}
    size_t requestFrom(uint8_t, size_t, uint8_t) {return num_bytes_to_read;}
    size_t requestFrom(const WireTransmission& transfer) {return num_bytes_to_read;}
    size_t write(uint8_t) {return num_bytes_to_write;}
    size_t write(const uint8_t *, size_t) {return num_bytes_to_write;}
    int available(void) {return num_bytes_to_read;}
    int read(void) {
        int c{-1};
        if(num_bytes_to_read) {
            c = data_read[index++];
            num_bytes_to_read--;
            if(!num_bytes_to_read) {index = 0;}
        }
        return c;
    }
    int peek(void);
    void flush(void);
    void onReceive(void (*)(int));
    void onRequest(void (*)(void));

    bool lock();
    bool unlock();

    inline size_t write(unsigned long n) { return write((uint8_t)n); }
    inline size_t write(long n) { return write((uint8_t)n); }
    inline size_t write(unsigned int n) { return write((uint8_t)n); }
    inline size_t write(int n) { return write((uint8_t)n); }

    bool isEnabled(void);

    /**
     * Attempts to reset this I2C bus.
     */
    void reset();

    hal_i2c_interface_t interface() const {
    return _i2c;
    }
    int num_bytes_to_write{};
    int num_bytes_to_read{};
    const int* data_read{};
    uint8_t end_transmission_return{endTransmissionReturns::SUCCESS};
};

typedef void* os_mutex_recursive_t;
class RecursiveMutex
{
    os_mutex_recursive_t handle_;
public:
    /**
     * Creates a shared mutex.
     */
    RecursiveMutex(os_mutex_recursive_t handle) : handle_(handle) {}

    RecursiveMutex() : handle_(nullptr)
    {
    }

    ~RecursiveMutex() {
        dispose();
    }

    void dispose()
    {
    }

    void lock() { }
    bool trylock() { return true; }
    bool try_lock() { return true; }
    void unlock() {}

};

void pinMode(uint16_t pin, PinMode mode);
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);


#define Wire __fetch_global_Wire()
TwoWire& __fetch_global_Wire();

using namespace spark;

class SystemClass {
public:
    SystemClass() : _tick(0) {}

    system_tick_t Uptime() const {
        return (system_tick_t)_tick;
    }

    unsigned uptime() const {
        return _tick / 1000;
    }

    uint64_t millis() const {
        return _tick;
    }

    void inc(int i = 1) {
        _tick += i;
    }

private:
    uint64_t _tick;
};

class Logger {
public:
    Logger() = default;
    Logger(const char *) {};
    void trace(const char* str, ...) {};
    void info(const char* str, ...) {};
    void error(const char* str, ...) {};
    void warn(const char* str, ...) {};
};

extern SystemClass System;
extern Logger Log;

inline system_tick_t millis(void) { return System.millis(); }

static inline bool HAL_IsISR() 
{
	return false;
}

static void vPortYield( void )
{}
#define portYIELD_FROM_ISR( x ) if( x ) vPortYield()

typedef short BaseType_t;

namespace particle {

template<typename TagT, typename ValueT>
class Flags;

// Class storing a typed flag value
template<typename TagT, typename ValueT = unsigned>
class Flag {
public:
    explicit Flag(ValueT val) {}

    explicit operator ValueT() const {}

    ValueT value() const {}

private:
    ValueT val_;
};

// Class storing or-combinations of typed flag values
template<typename TagT, typename ValueT = unsigned>
class Flags {
public:
    typedef TagT TagType;
    typedef ValueT ValueType;
    typedef Flag<TagT, ValueT> FlagType;

    Flags() {}
    Flags(Flag<TagT, ValueT> flag) {}

    explicit operator ValueT() const;
    explicit operator bool() const;

    ValueT value() const;

private:
    ValueT val_;

    explicit Flags(ValueT val);
};

class Error {
public:
    // Error type
    enum Type {
        NONE = 0,
        UNKNOWN,
        LIMIT_EXCEEDED,
        CANCELLED,
    };

    Error(Type type = UNKNOWN);
    Error(Type type, const char* msg);
    explicit Error(const char* msg);
    Error(const Error& error);

    Type type() const;
    const char* message() const;

    bool operator==(const Error& error) const;
    bool operator!=(const Error& error) const;
    bool operator==(Type type) const;
    bool operator!=(Type type) const;

    explicit operator bool() const;

private:
    const char* msg_;
    Type type_;

    friend void swap(Error& error1, Error& error2);
};

inline Error::Error(Type type) :
        msg_(nullptr),
        type_(type) {
}

inline Error::Error(Type type, const char* msg) :
        msg_(msg),
        type_(type) {
}

inline Error::Error(const char* msg) :
        Error(UNKNOWN, msg) {
}

inline Error::Error(const Error& error) :
        Error(error.type_, error.msg_) {
}

inline Error::Type Error::type() const {
    return type_;
}

inline const char* Error::message() const {
    return msg_ ? msg_ : "";
}

inline bool Error::operator==(const Error& error) const {
    return (type_ == error.type_);
}

inline bool Error::operator!=(const Error& error) const {
    return !operator==(error);
}

inline bool Error::operator==(Type type) const {
    return (type_ == type);
}

inline bool Error::operator!=(Type type) const {
    return !operator==(type);
}

inline Error::operator bool() const {
    return type_ != NONE;
}

template<typename ContextT>
class Future {
public:
    Future() {}
    bool isSucceeded() const {
        return isSucceededReturn;
    }

    bool isDone() const {
        return isDoneReturn;
    }

    Error error() const {
        return err;
    }

    bool isDoneReturn;
    bool isSucceededReturn;
    Error err;
};
} // namespace particle

struct PublishFlagType; // Tag type for Particle.publish() flags
typedef particle::Flags<PublishFlagType, uint8_t> PublishFlags;
typedef PublishFlags::FlagType PublishFlag;

class CloudClass {
public:

    CloudClass() {}
    inline particle::Future<bool> publish(const char *eventName, 
                                        const char *eventData, 
                                        PublishFlags flags1, 
                                        PublishFlags flags2 = PublishFlags()) {
        return state_output;
    }
    particle::Future<bool> state_output;
};
extern CloudClass Particle;

const uint32_t PUBLISH_EVENT_FLAG_PUBLIC = 0x0;
const uint32_t PUBLISH_EVENT_FLAG_PRIVATE = 0x1;
const uint32_t PUBLISH_EVENT_FLAG_NO_ACK = 0x2;
const uint32_t PUBLISH_EVENT_FLAG_WITH_ACK = 0x8;

const PublishFlag PUBLIC(PUBLISH_EVENT_FLAG_PUBLIC);
const PublishFlag PRIVATE(PUBLISH_EVENT_FLAG_PRIVATE);
const PublishFlag NO_ACK(PUBLISH_EVENT_FLAG_NO_ACK);
const PublishFlag WITH_ACK(PUBLISH_EVENT_FLAG_WITH_ACK);

#define pdFALSE			( ( BaseType_t ) 0 )
#define pdTRUE			( ( BaseType_t ) 1 )
#define pdPASS			( pdTRUE )
#define pdFAIL			( pdFALSE )

/* Default priority is the same as the application thread */
#define OS_THREAD_PRIORITY_DEFAULT       (2)
#define OS_THREAD_PRIORITY_CRITICAL      (9)
#define OS_THREAD_PRIORITY_NETWORK       (7)
#define OS_THREAD_PRIORITY_NETWORK_HIGH  (8)
#define OS_THREAD_STACK_SIZE_DEFAULT (3*1024)
#define OS_THREAD_STACK_SIZE_DEFAULT_HIGH (4*1024)
#define OS_THREAD_STACK_SIZE_DEFAULT_NETWORK (6*1024)

class Thread
{
public:
    Thread(const char *name, 
            wiring_thread_fn_t function,
            os_thread_prio_t priority=OS_THREAD_PRIORITY_DEFAULT, 
            size_t stack_size=OS_THREAD_STACK_SIZE_DEFAULT) {
    }
};

