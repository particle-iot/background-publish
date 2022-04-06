/*
 * Particle.cpp
 *
 *  Created on: May 3, 2021
 *      Author: eric
 */

#include "Particle.h"
#include "BackgroundPublish.h"
#include <queue>

SystemClass System;
Logger Log;
CloudClass Particle;

std::queue<publish_event_t> my_queue;

hal_i2c_config_t defaultWireConfig() {
	hal_i2c_config_t config = {
		.size = sizeof(hal_i2c_config_t),
		.version = HAL_I2C_CONFIG_VERSION_1,
		.rx_buffer = new (std::nothrow) uint8_t[I2C_BUFFER_LENGTH],
		.rx_buffer_size = I2C_BUFFER_LENGTH,
		.tx_buffer = new (std::nothrow) uint8_t[I2C_BUFFER_LENGTH],
		.tx_buffer_size = I2C_BUFFER_LENGTH
	};

	return config;
}

hal_i2c_config_t acquireWireBuffer() {
    return defaultWireConfig();
}

TwoWire& __fetch_global_Wire()
{
	static TwoWire wire(HAL_I2C_INTERFACE1, acquireWireBuffer());
	return wire;
}

void pinMode(uint16_t pin, PinMode mode) {}
void delay(uint32_t ms) {}
void delayMicroseconds(uint32_t us) {}

