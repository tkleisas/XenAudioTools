// SPDX-License-Identifier: GPL-3.0-or-later
#include "AudioOutput.h"

#include <algorithm>
#include <cmath>

#include <android/log.h>

namespace upic {

namespace {

constexpr const char* kLogTag = "UPICStudio";
constexpr int kMaxBlock = 4096;

float softClip(float sample) {
    const float magnitude = std::abs(sample);
    if (magnitude <= 0.9f) return sample;
    const float limited =
        0.9f + 0.1f * std::tanh((magnitude - 0.9f) / 0.1f);
    return std::copysign(limited, sample);
}

} // namespace

AudioOutput::~AudioOutput() {
    stop();
}

std::string AudioOutput::error() const {
    std::lock_guard<std::mutex> lock(streamMutex_);
    return error_;
}

bool AudioOutput::start() {
    std::lock_guard<std::mutex> lock(streamMutex_);
    if (stream_ != nullptr) return true;

    left_.assign(kMaxBlock, 0.0f);
    right_.assign(kMaxBlock, 0.0f);

    AAudioStreamBuilder* builder = nullptr;
    aaudio_result_t result = AAudio_createStreamBuilder(&builder);
    if (result != AAUDIO_OK || builder == nullptr) {
        error_ = std::string("AAudio builder: ") +
                 AAudio_convertResultToText(result);
        return false;
    }

    AAudioStreamBuilder_setDirection(builder, AAUDIO_DIRECTION_OUTPUT);
    AAudioStreamBuilder_setSharingMode(builder, AAUDIO_SHARING_MODE_SHARED);
    AAudioStreamBuilder_setPerformanceMode(
        builder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
    AAudioStreamBuilder_setFormat(builder, AAUDIO_FORMAT_PCM_FLOAT);
    AAudioStreamBuilder_setChannelCount(builder, 2);
    AAudioStreamBuilder_setUsage(builder, AAUDIO_USAGE_MEDIA);
    AAudioStreamBuilder_setContentType(builder, AAUDIO_CONTENT_TYPE_MUSIC);
    AAudioStreamBuilder_setDataCallback(builder, &AudioOutput::dataCallback,
                                       this);

    result = AAudioStreamBuilder_openStream(builder, &stream_);
    if (result != AAUDIO_OK && stream_ == nullptr) {
        AAudioStreamBuilder_setPerformanceMode(
            builder, AAUDIO_PERFORMANCE_MODE_NONE);
        result = AAudioStreamBuilder_openStream(builder, &stream_);
    }
    AAudioStreamBuilder_delete(builder);

    if (result != AAUDIO_OK || stream_ == nullptr) {
        error_ = std::string("AAudio open: ") +
                 AAudio_convertResultToText(result);
        stream_ = nullptr;
        return false;
    }

    const int sampleRate = AAudioStream_getSampleRate(stream_);
    engine_.prepare(sampleRate > 0 ? sampleRate : 48000, 0);
    running_.store(true, std::memory_order_release);

    result = AAudioStream_requestStart(stream_);
    if (result != AAUDIO_OK) {
        error_ = std::string("AAudio start: ") +
                 AAudio_convertResultToText(result);
        running_.store(false, std::memory_order_release);
        AAudioStream_close(stream_);
        stream_ = nullptr;
        return false;
    }

    error_.clear();
    __android_log_print(ANDROID_LOG_INFO, kLogTag,
                        "AAudio started at %d Hz", sampleRate);
    return true;
}

void AudioOutput::stop() {
    std::lock_guard<std::mutex> lock(streamMutex_);
    running_.store(false, std::memory_order_release);
    if (stream_ != nullptr) {
        AAudioStream_requestStop(stream_);
        AAudioStream_close(stream_);
        stream_ = nullptr;
    }
    events_.fill(NoteEvent{});
    writeIndex_.store(0, std::memory_order_release);
    readIndex_.store(0, std::memory_order_release);
}

aaudio_data_callback_result_t AudioOutput::dataCallback(
    AAudioStream* stream, void* userData, void* audioData,
    int32_t numFrames) {
    (void)stream;
    auto* output = static_cast<AudioOutput*>(userData);
    if (output == nullptr || audioData == nullptr || numFrames <= 0)
        return AAUDIO_CALLBACK_RESULT_CONTINUE;
    if (!output->running_.load(std::memory_order_acquire))
        return AAUDIO_CALLBACK_RESULT_CONTINUE;

    output->drainEvents();

    auto* interleaved = static_cast<float*>(audioData);
    int32_t written = 0;
    while (written < numFrames) {
        const int32_t block = std::min(numFrames - written, kMaxBlock);
        output->engine_.render(output->left_.data(), output->right_.data(),
                               block);
        for (int32_t frame = 0; frame < block; ++frame) {
            interleaved[(written + frame) * 2] =
                softClip(output->left_[(size_t)frame]);
            interleaved[(written + frame) * 2 + 1] =
                softClip(output->right_[(size_t)frame]);
        }
        written += block;
    }
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

bool AudioOutput::pushEvent(const NoteEvent& event) {
    const uint32_t write = writeIndex_.load(std::memory_order_relaxed);
    const uint32_t next = (write + 1) % kEventCapacity;
    if (next == readIndex_.load(std::memory_order_acquire)) return false;
    events_[write] = event;
    writeIndex_.store(next, std::memory_order_release);
    return true;
}

void AudioOutput::drainEvents() {
    uint32_t read = readIndex_.load(std::memory_order_relaxed);
    const uint32_t write = writeIndex_.load(std::memory_order_acquire);
    while (read != write) {
        const NoteEvent& event = events_[read];
        if (event.note < 0) {
            engine_.allNotesOff();
        } else if (event.on) {
            engine_.noteOn(event.note, event.velocity);
        } else {
            engine_.noteOff(event.note);
        }
        read = (read + 1) % kEventCapacity;
    }
    readIndex_.store(read, std::memory_order_release);
}

void AudioOutput::noteOn(int note, float velocity) {
    pushEvent({note, std::clamp(velocity, 0.0f, 1.0f), true});
}

void AudioOutput::noteOff(int note) {
    pushEvent({note, 0.0f, false});
}

void AudioOutput::allNotesOff() {
    pushEvent({-1, 0.0f, false});
}

} // namespace upic
