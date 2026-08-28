#pragma once

#include <Arduino.h>

// UART driver for the RehabMotion offline voice board.
// Main-controller wiring: RX=GPIO18, TX=GPIO17, 115200 8N1.
//
// queue*() + update() are the preferred real-time path. They preserve CRC,
// sequence de-duplication, acknowledgements and retries without blocking the
// BLE IMU/training loop while the voice board is busy playing another prompt.
class RehabVoiceLink {
public:
    static constexpr int kDefaultRxPin = 18;
    static constexpr int kDefaultTxPin = 17;
    static constexpr uint32_t kDefaultBaud = 115200;

    explicit RehabVoiceLink(HardwareSerial& serial);

    void begin(uint32_t baud = kDefaultBaud,
               int rx_pin = kDefaultRxPin,
               int tx_pin = kDefaultTxPin);

    bool queueStartFlow();
    bool queueCalibrationStart();
    bool queueTrainingStart();
    bool queuePause();
    bool queueResume();
    bool queueSetDone();
    bool queueNextSet();
    bool queueTrainingDone();
    bool queueSensorError();
    bool queueStop();
    bool queuePing();

    // Call once per main loop. This method never waits for an ACK.
    void update();
    bool busy() const;
    uint32_t deliveredCount() const { return delivered_count_; }
    uint32_t failedCount() const { return failed_count_; }

    // Synchronous compatibility API. Do not use it in the real-time IMU loop.
    bool startFlow(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool calibrationStart(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool trainingStart(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool pause(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool resume(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool setDone(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool nextSet(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool trainingDone(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool sensorError(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool stop(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);
    bool ping(uint32_t ack_timeout_ms = 250, uint8_t retries = 3);

private:
    static constexpr uint8_t kQueueCapacity = 8;
    static constexpr uint32_t kAsyncAckTimeoutMs = 250;
    static constexpr uint32_t kAsyncRetryIntervalMs = 250;
    // Covers the longest built-in prompt being busy, while remaining bounded
    // when the voice board is disconnected.
    static constexpr uint8_t kAsyncMaxAttempts = 32;

    HardwareSerial& serial_;
    uint32_t next_sequence_ = 1;

    const char* queue_[kQueueCapacity] = {};
    uint8_t queue_head_ = 0;
    uint8_t queue_tail_ = 0;
    uint8_t queue_count_ = 0;

    bool async_active_ = false;
    bool async_waiting_ack_ = false;
    uint32_t async_sequence_ = 0;
    uint32_t async_sent_at_ms_ = 0;
    uint32_t async_retry_not_before_ms_ = 0;
    uint8_t async_attempts_ = 0;
    char async_frame_[80] = {};
    size_t async_frame_length_ = 0;
    char async_rx_line_[96] = {};
    size_t async_rx_length_ = 0;
    uint32_t delivered_count_ = 0;
    uint32_t failed_count_ = 0;

    static uint8_t calculateCrc8(const char* data, size_t length);
    static bool timeReached(uint32_t now, uint32_t deadline);
    bool queueCommand(const char* command);
    bool startNextQueuedCommand(uint32_t now);
    void sendAsyncAttempt(uint32_t now);
    void consumeAsyncRx();
    void finishAsyncCommand(bool delivered);
    bool sendCommand(const char* command, uint32_t ack_timeout_ms, uint8_t retries);
    bool waitForAck(uint32_t sequence, uint32_t timeout_ms);
    bool parseAck(const char* line, uint32_t expected_sequence, bool& ok) const;
    void clearRx();
};
