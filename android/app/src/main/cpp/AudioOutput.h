// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <aaudio/AAudio.h>

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "xen/engine.h"

namespace upic {

class AudioOutput {
public:
    AudioOutput() = default;
    ~AudioOutput();

    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    bool start();
    void stop();
    bool running() const { return running_.load(); }
    std::string error() const;

    xen::Engine& engine() { return engine_; }

    void noteOn(int note, float velocity);
    void noteOff(int note);
    void allNotesOff();

private:
    struct NoteEvent {
        int note = 0;
        float velocity = 0.0f;
        bool on = false;
    };

    static aaudio_data_callback_result_t dataCallback(
        AAudioStream* stream, void* userData, void* audioData,
        int32_t numFrames);
    void render(float* output, int32_t numFrames);
    bool pushEvent(const NoteEvent& event);
    void drainEvents();

    static constexpr uint32_t kEventCapacity = 256;

    xen::Engine engine_;
    AAudioStream* stream_ = nullptr;
    std::atomic<bool> running_ { false };
    mutable std::mutex streamMutex_;
    std::string error_;

    std::vector<float> left_;
    std::vector<float> right_;

    std::array<NoteEvent, kEventCapacity> events_ {};
    std::atomic<uint32_t> writeIndex_ { 0 };
    std::atomic<uint32_t> readIndex_ { 0 };
};

} // namespace upic
