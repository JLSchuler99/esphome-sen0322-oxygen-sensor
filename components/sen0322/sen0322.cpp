#include "sen0322.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace sen0322 {

static const char *const TAG = "sen0322";

// Official register addresses
static const uint8_t REG_OXYGEN_DATA = 0x03;
static const uint8_t REG_KEY = 0x05;

void SEN0322Sensor::setup() {
  ESP_LOGCONFIG(TAG, "Setting up SEN0322...");
  // No need to send collect/judge phase based on DFRobot reference
  ESP_LOGCONFIG(TAG, "SEN0322 setup complete");
}

void SEN0322Sensor::dump_config() {
  ESP_LOGCONFIG(TAG, "SEN0322 Oxygen Sensor:");
  LOG_I2C_DEVICE(this);
  LOG_UPDATE_INTERVAL(this);
  LOG_SENSOR("  ", "Oxygen", this);
  
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Communication with SEN0322 failed!");
  }
}

void SEN0322Sensor::update() {
  ESP_LOGV(TAG, "Updating SEN0322 sensor...");

  // Step 1: Read calibration key from flash (register 0x05)
  if (!this->write_byte(REG_KEY, 0x00)) {
    ESP_LOGE(TAG, "Failed to request calibration key");
    this->status_set_warning();
    return;
  }

  esphome::delay(50);

  auto key_result = this->read_byte(REG_KEY);
  if (!key_result.has_value()) {
    ESP_LOGE(TAG, "Failed to read calibration key");
    this->status_set_warning();
    return;
  }
  uint8_t key_byte = key_result.value();
  float key = (key_byte == 0) ? (20.9f / 120.0f) : (key_byte / 1000.0f);
  key = 20.9f / 120.0f;
  key = 0.18463f;
  ESP_LOGD(TAG, "Calibration key: 0x%02X → %.5f", key_byte, key);
  
  float oxygen = 0.0f;;

  // Request data
  if (!this->write_byte(REG_OXYGEN_DATA, 0x00)) {
    ESP_LOGE(TAG, "Failed to send oxygen data request");
  }

  esphome::delay(100);  // Wait for sensor to prepare data

  uint8_t data[3];
  if (!this->read_bytes_raw(data, 3)) {
    ESP_LOGE(TAG, "Failed to read oxygen data");
    
  }

  oxygen = key * (data[0] + data[1] / 10.0f + data[2] / 100.0f);
  ESP_LOGV(TAG, "Raw bytes: [%d, %d, %d] → %.2f%%", data[0], data[1], data[2], oxygen);

  float alpha = 0.3f; // Tune between 0.1 (slow) and 0.5 (fast)
  
  if (oxygen >= 0.0f && oxygen <= 30.0f) {
    if (std::isnan(ema_oxygen_)) {
      ema_oxygen_ = oxygen;
    } else {
      ema_oxygen_ = alpha * oxygen + (1 - alpha) * ema_oxygen_;
    }
    this->publish_state(ema_oxygen_);
    this->status_clear_warning();
  } else {
    ESP_LOGW(TAG, "Discarded invalid reading: %.2f%%", oxygen);
    this->status_set_warning();
  }
}

float SEN0322Sensor::get_setup_priority() const {
  return setup_priority::DATA;
}

}  // namespace sen0322
}  // namespace esphome
