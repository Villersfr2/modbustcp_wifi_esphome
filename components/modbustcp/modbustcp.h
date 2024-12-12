#include "esphome.h"
#include <WiFiClient.h>

class ModbusTCPSensor : public PollingComponent, public Sensor {
 public:
  ModbusTCPSensor(const std::string &host, uint16_t port, uint16_t register_address, const std::string &byte_order, uint32_t update_interval)
      : PollingComponent(update_interval), host_(host), port_(port), register_address_(register_address), byte_order_(byte_order) {}

  void setup() override {
    ESP_LOGD("modbus_tcp", "Setting up Modbus TCP sensor at address %d...", register_address_);
    this->set_accuracy_decimals(2);  // Display sensor values with 2 decimal places
  }

  void update() override {
    WiFiClient client;
    if (!client.connect(host_.c_str(), port_)) {
      ESP_LOGE("modbus_tcp", "Failed to connect to Modbus server %s:%d", host_.c_str(), port_);
      return;
    }

    // Build Modbus request (Read Holding Registers, function code 0x03)
    uint8_t request[] = {
        0x00, 0x01,  // Transaction ID
        0x00, 0x00,  // Protocol ID
        0x00, 0x06,  // Length
        0x01,        // Unit ID
        0x03,        // Function Code (Read Holding Registers)
        (uint8_t)((register_address_ >> 8) & 0xFF),  // Start Address (High Byte)
        (uint8_t)(register_address_ & 0xFF),        // Start Address (Low Byte)
        0x00, 0x02   // Quantity (Read 2 Registers = 32 bits for FP32)
    };

    client.write(request, sizeof(request));
    delay(100);

    uint8_t response[256];
    size_t response_len = client.read(response, sizeof(response));
    if (response_len < 9) {
      ESP_LOGE("modbus_tcp", "Invalid response length: %d", response_len);
      return;
    }

    // Debugging: Print raw response data
    ESP_LOGD("modbus_tcp", "Raw Response Data:");
    for (size_t i = 0; i < response_len; i++) {
      ESP_LOGD("modbus_tcp", "Byte %d: 0x%02X", i, response[i]);
    }

    if (response[7] != 0x03) {
      ESP_LOGE("modbus_tcp", "Unexpected function code: 0x%02X", response[7]);
      return;
    }

    // Decode value based on byte order
    float value = decode_float(&response[9], byte_order_);
    ESP_LOGD("modbus_tcp", "Register %d: %.2f", register_address_, value);
    publish_state(value);
  }

 private:
  std::string host_;
  uint16_t port_;
  uint16_t register_address_;
  std::string byte_order_;

  float decode_float(uint8_t *data, const std::string &byte_order) {
    uint32_t raw_value = 0;

    if (byte_order == "AB_CD") {
      raw_value = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | data[3];
    } else if (byte_order == "CD_AB") {
      raw_value = (data[2] << 24) | (data[3] << 16) | (data[0] << 8) | data[1];
    } else {
      ESP_LOGE("modbus_tcp", "Invalid byte order: %s", byte_order.c_str());
      return 0.0f;
    }

    float value;
    memcpy(&value, &raw_value, sizeof(float));
    return value;
  }
};
