#include "modbustcp.h"

ModbusTCPSensor::ModbusTCPSensor(const std::string &host, uint16_t port, uint16_t register_address, const std::string &byte_order, uint32_t update_interval)
    : PollingComponent(update_interval), host_(host), port_(port), register_address_(register_address), byte_order_(byte_order) {}

void ModbusTCPSensor::setup() {
    ESP_LOGD("modbus_tcp", "Setting up Modbus TCP sensor at address %d...", register_address_);
    this->set_accuracy_decimals(2);  // Display sensor values with 2 decimal places
}

void ModbusTCPSensor::update() {
    WiFiClient client;
    if (!client.connect(host_.c_str(), port_)) {
        ESP_LOGE("modbus_tcp", "Failed to connect to Modbus server %s:%d", host_.c_str(), port_);
        return;
    }

    uint8_t request[] = {
        0x00, 0x01, 0x00, 0x00, 0x00, 0x06, 0x01, 0x03, 
        (uint8_t)((register_address_ >> 8) & 0xFF), 
        (uint8_t)(register_address_ & 0xFF), 
        0x00, 0x02
    };

    client.write(request, sizeof(request));
    delay(100);

    uint8_t response[256];
    size_t response_len = client.read(response, sizeof(response));
    if (response_len < 9) {
        ESP_LOGE("modbus_tcp", "Invalid response length: %d", response_len);
        return;
    }

    if (response[7] != 0x03) {
        ESP_LOGE("modbus_tcp", "Unexpected function code: 0x%02X", response[7]);
        return;
    }

    float value = decode_float(&response[9], byte_order_);
    ESP_LOGD("modbus_tcp", "Register %d: %.2f", register_address_, value);
    publish_state(value);
}

float ModbusTCPSensor::decode_float(uint8_t *data, const std::string &byte_order) {
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
