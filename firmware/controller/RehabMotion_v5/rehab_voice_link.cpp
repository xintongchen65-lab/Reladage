#include "rehab_voice_link.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

RehabVoiceLink::RehabVoiceLink(HardwareSerial& serial) : serial_(serial) {}

void RehabVoiceLink::begin(uint32_t baud, int rx_pin, int tx_pin) {
    serial_.begin(baud, SERIAL_8N1, rx_pin, tx_pin);
    clearRx();
}

bool RehabVoiceLink::queueStartFlow() { return queueCommand("START_FLOW"); }
bool RehabVoiceLink::queueCalibrationStart() { return queueCommand("CALIBRATION_START"); }
bool RehabVoiceLink::queueTrainingStart() { return queueCommand("TRAINING_START"); }
bool RehabVoiceLink::queuePause() { return queueCommand("PAUSE"); }
bool RehabVoiceLink::queueResume() { return queueCommand("RESUME"); }
bool RehabVoiceLink::queueSetDone() { return queueCommand("SET_DONE"); }
bool RehabVoiceLink::queueNextSet() { return queueCommand("NEXT_SET"); }
bool RehabVoiceLink::queueTrainingDone() { return queueCommand("TRAINING_DONE"); }
bool RehabVoiceLink::queueSensorError() { return queueCommand("SENSOR_ERROR"); }
bool RehabVoiceLink::queueStop() { return queueCommand("STOP"); }
bool RehabVoiceLink::queuePing() { return queueCommand("PING"); }

bool RehabVoiceLink::busy() const {
    return async_active_ || queue_count_ != 0;
}

bool RehabVoiceLink::timeReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}

bool RehabVoiceLink::queueCommand(const char* command) {
    if (command == nullptr || command[0] == '\0' || queue_count_ >= kQueueCapacity) {
        return false;
    }
    queue_[queue_tail_] = command;
    queue_tail_ = static_cast<uint8_t>((queue_tail_ + 1U) % kQueueCapacity);
    ++queue_count_;
    return true;
}

bool RehabVoiceLink::startNextQueuedCommand(uint32_t now) {
    if (async_active_ || queue_count_ == 0) {
        return false;
    }

    const char* command = queue_[queue_head_];
    queue_head_ = static_cast<uint8_t>((queue_head_ + 1U) % kQueueCapacity);
    --queue_count_;

    async_sequence_ = next_sequence_++;
    if (next_sequence_ == 0) {
        next_sequence_ = 1;
    }

    char payload[64];
    const int payload_length = std::snprintf(payload, sizeof(payload), "C,%lu,%s",
                                             static_cast<unsigned long>(async_sequence_), command);
    if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(payload))) {
        ++failed_count_;
        return false;
    }

    const int frame_length = std::snprintf(async_frame_, sizeof(async_frame_), "@%s*%02X\n", payload,
                                           calculateCrc8(payload, static_cast<size_t>(payload_length)));
    if (frame_length <= 0 || frame_length >= static_cast<int>(sizeof(async_frame_))) {
        ++failed_count_;
        return false;
    }

    async_frame_length_ = static_cast<size_t>(frame_length);
    async_attempts_ = 0;
    async_waiting_ack_ = false;
    async_retry_not_before_ms_ = now;
    async_rx_length_ = 0;
    async_active_ = true;
    return true;
}

void RehabVoiceLink::sendAsyncAttempt(uint32_t now) {
    if (!async_active_ || async_waiting_ack_) {
        return;
    }
    const size_t written = serial_.write(reinterpret_cast<const uint8_t*>(async_frame_),
                                         async_frame_length_);
    ++async_attempts_;
    if (written == async_frame_length_) {
        async_sent_at_ms_ = now;
        async_waiting_ack_ = true;
    } else {
        async_retry_not_before_ms_ = now + kAsyncRetryIntervalMs;
    }
}

void RehabVoiceLink::consumeAsyncRx() {
    while (serial_.available() > 0) {
        const int value = serial_.read();
        if (value < 0) {
            break;
        }
        const char byte = static_cast<char>(value);
        if (byte == '\r') {
            continue;
        }
        if (byte == '\n') {
            async_rx_line_[async_rx_length_] = '\0';
            bool ok = false;
            if (async_active_ && parseAck(async_rx_line_, async_sequence_, ok)) {
                if (ok) {
                    finishAsyncCommand(true);
                } else {
                    async_waiting_ack_ = false;
                    async_retry_not_before_ms_ = millis() + kAsyncRetryIntervalMs;
                }
            }
            async_rx_length_ = 0;
            continue;
        }
        if (async_rx_length_ + 1U < sizeof(async_rx_line_)) {
            async_rx_line_[async_rx_length_++] = byte;
        } else {
            async_rx_length_ = 0;
        }
    }
}

void RehabVoiceLink::finishAsyncCommand(bool delivered) {
    if (delivered) {
        ++delivered_count_;
    } else {
        ++failed_count_;
    }
    async_active_ = false;
    async_waiting_ack_ = false;
    async_frame_length_ = 0;
    async_rx_length_ = 0;
}

void RehabVoiceLink::update() {
    consumeAsyncRx();
    const uint32_t now = millis();

    if (!async_active_) {
        startNextQueuedCommand(now);
    }
    if (!async_active_) {
        return;
    }

    if (async_waiting_ack_ &&
        static_cast<uint32_t>(now - async_sent_at_ms_) >= kAsyncAckTimeoutMs) {
        async_waiting_ack_ = false;
        async_retry_not_before_ms_ = now;
    }

    if (!async_waiting_ack_ && timeReached(now, async_retry_not_before_ms_)) {
        if (async_attempts_ >= kAsyncMaxAttempts) {
            finishAsyncCommand(false);
            startNextQueuedCommand(now);
        } else {
            sendAsyncAttempt(now);
        }
    }
}

bool RehabVoiceLink::startFlow(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("START_FLOW", ack_timeout_ms, retries);
}

bool RehabVoiceLink::calibrationStart(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("CALIBRATION_START", ack_timeout_ms, retries);
}

bool RehabVoiceLink::trainingStart(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("TRAINING_START", ack_timeout_ms, retries);
}

bool RehabVoiceLink::pause(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("PAUSE", ack_timeout_ms, retries);
}

bool RehabVoiceLink::resume(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("RESUME", ack_timeout_ms, retries);
}

bool RehabVoiceLink::setDone(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("SET_DONE", ack_timeout_ms, retries);
}

bool RehabVoiceLink::nextSet(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("NEXT_SET", ack_timeout_ms, retries);
}

bool RehabVoiceLink::trainingDone(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("TRAINING_DONE", ack_timeout_ms, retries);
}

bool RehabVoiceLink::sensorError(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("SENSOR_ERROR", ack_timeout_ms, retries);
}

bool RehabVoiceLink::stop(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("STOP", ack_timeout_ms, retries);
}

bool RehabVoiceLink::ping(uint32_t ack_timeout_ms, uint8_t retries) {
    return sendCommand("PING", ack_timeout_ms, retries);
}

uint8_t RehabVoiceLink::calculateCrc8(const char* data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint8_t>(data[i]);
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x80) ? static_cast<uint8_t>((crc << 1) ^ 0x07)
                               : static_cast<uint8_t>(crc << 1);
        }
    }
    return crc;
}

bool RehabVoiceLink::sendCommand(const char* command, uint32_t ack_timeout_ms, uint8_t retries) {
    if (command == nullptr || retries == 0 || busy()) {
        return false;
    }

    const uint32_t sequence = next_sequence_++;
    if (next_sequence_ == 0) {
        next_sequence_ = 1;
    }

    char payload[64];
    const int payload_length = std::snprintf(payload, sizeof(payload), "C,%lu,%s",
                                             static_cast<unsigned long>(sequence), command);
    if (payload_length <= 0 || payload_length >= static_cast<int>(sizeof(payload))) {
        return false;
    }

    char frame[80];
    const int frame_length = std::snprintf(frame, sizeof(frame), "@%s*%02X\n", payload,
                                           calculateCrc8(payload, static_cast<size_t>(payload_length)));
    if (frame_length <= 0 || frame_length >= static_cast<int>(sizeof(frame))) {
        return false;
    }

    for (uint8_t attempt = 0; attempt < retries; ++attempt) {
        clearRx();
        serial_.write(reinterpret_cast<const uint8_t*>(frame), static_cast<size_t>(frame_length));
        serial_.flush();
        if (waitForAck(sequence, ack_timeout_ms)) {
            return true;
        }
    }
    return false;
}

bool RehabVoiceLink::waitForAck(uint32_t sequence, uint32_t timeout_ms) {
    char line[96];
    size_t length = 0;
    const uint32_t started_at = millis();

    while (static_cast<uint32_t>(millis() - started_at) < timeout_ms) {
        while (serial_.available() > 0) {
            const int value = serial_.read();
            if (value < 0) {
                break;
            }

            const char byte = static_cast<char>(value);
            if (byte == '\n') {
                line[length] = '\0';
                bool ok = false;
                if (parseAck(line, sequence, ok)) {
                    return ok;
                }
                length = 0;
            } else if (byte != '\r') {
                if (length + 1 < sizeof(line)) {
                    line[length++] = byte;
                } else {
                    length = 0;
                }
            }
        }
        delay(1);
    }
    return false;
}

bool RehabVoiceLink::parseAck(const char* line, uint32_t expected_sequence, bool& ok) const {
    if (line == nullptr || line[0] != '@') {
        return false;
    }

    const char* separator = std::strrchr(line, '*');
    if (separator == nullptr || std::strlen(separator + 1) != 2) {
        return false;
    }

    char* crc_end = nullptr;
    const unsigned long received_crc = std::strtoul(separator + 1, &crc_end, 16);
    if (crc_end == nullptr || *crc_end != '\0' || received_crc > 0xFF) {
        return false;
    }

    const size_t payload_length = static_cast<size_t>(separator - (line + 1));
    if (payload_length == 0 || payload_length >= 64 ||
        calculateCrc8(line + 1, payload_length) != static_cast<uint8_t>(received_crc)) {
        return false;
    }

    char payload[64];
    std::memcpy(payload, line + 1, payload_length);
    payload[payload_length] = '\0';

    unsigned long ack_sequence = 0;
    int ack_ok = 0;
    if (std::sscanf(payload, "A,%lu,%d", &ack_sequence, &ack_ok) != 2 ||
        static_cast<uint32_t>(ack_sequence) != expected_sequence) {
        return false;
    }

    ok = ack_ok == 1;
    return true;
}

void RehabVoiceLink::clearRx() {
    while (serial_.available() > 0) {
        serial_.read();
    }
}
