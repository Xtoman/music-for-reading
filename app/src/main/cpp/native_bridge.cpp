#include <android/log.h>
#include <jni.h>
#include <oboe/Oboe.h>

#include <atomic>
#include <memory>
#include <mutex>

#include "AudioEngine.h"

#define LOG_TAG "ReadingMusic"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace {

std::unique_ptr<readingmusic::AudioEngine> gEngine;
std::shared_ptr<oboe::AudioStream> gStream;
std::mutex gStreamMutex;
std::atomic<bool> gStreamReady{false};

class AudioCallback : public oboe::AudioStreamDataCallback {
public:
    oboe::DataCallbackResult onAudioReady(
        oboe::AudioStream* stream,
        void* audioData,
        int32_t numFrames) override {

        auto* output = static_cast<float*>(audioData);
        const int channels = stream->getChannelCount();

        if (gEngine) {
            gEngine->render(output, numFrames, channels);
        } else {
            for (int i = 0; i < numFrames * channels; ++i) {
                output[i] = 0.0f;
            }
        }
        return oboe::DataCallbackResult::Continue;
    }
};

static AudioCallback gCallback;

bool openStream() {
    oboe::AudioStreamBuilder builder;
    builder.setDirection(oboe::Direction::Output)
        ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
        ->setSharingMode(oboe::SharingMode::Shared)
        ->setFormat(oboe::AudioFormat::Float)
        ->setChannelCount(oboe::ChannelCount::Stereo)
        ->setDataCallback(&gCallback);

    oboe::Result result = builder.openStream(gStream);
    if (result != oboe::Result::OK) {
        LOGE("Failed to open stream: %s", oboe::convertToText(result));
        return false;
    }

    result = gStream->start();
    if (result != oboe::Result::OK) {
        LOGE("Failed to start stream: %s", oboe::convertToText(result));
        gStream.reset();
        return false;
    }

    gStreamReady.store(true);
    return true;
}

void closeStream() {
    std::lock_guard<std::mutex> lock(gStreamMutex);
    if (gStream) {
        gStream->stop();
        gStream->close();
        gStream.reset();
    }
    gStreamReady.store(false);
}

} // namespace

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeInit(JNIEnv*, jobject) {
    if (!gEngine) {
        gEngine = std::make_unique<readingmusic::AudioEngine>();
    }
    return JNI_TRUE;
}

JNIEXPORT jboolean JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeStart(JNIEnv*, jobject) {
    if (!gEngine) {
        gEngine = std::make_unique<readingmusic::AudioEngine>();
    }

    std::lock_guard<std::mutex> lock(gStreamMutex);
    if (!gStreamReady.load()) {
        if (!openStream()) {
            return JNI_FALSE;
        }
    }
    gEngine->start();
    return JNI_TRUE;
}

JNIEXPORT void JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeStop(JNIEnv*, jobject) {
    if (gEngine) {
        gEngine->stop();
    }
}

JNIEXPORT void JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeSetStyle(JNIEnv*, jobject, jint style) {
    if (gEngine) {
        gEngine->setStyle(static_cast<readingmusic::MusicStyle>(style));
    }
}

JNIEXPORT void JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeSetVolume(JNIEnv*, jobject, jfloat volume) {
    if (gEngine) {
        gEngine->setVolume(volume);
    }
}

JNIEXPORT void JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeSetFadeSeconds(JNIEnv*, jobject, jfloat seconds) {
    if (gEngine) {
        gEngine->setFadeSeconds(seconds);
    }
}

JNIEXPORT void JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeRequestSleepFade(JNIEnv*, jobject, jfloat seconds) {
    if (gEngine) {
        gEngine->requestSleepFade(seconds);
    }
}

JNIEXPORT jboolean JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeIsRunning(JNIEnv*, jobject) {
    return gEngine && gEngine->isRunning() ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_readingmusic_app_audio_NativeAudioEngine_nativeRelease(JNIEnv*, jobject) {
    if (gEngine) {
        gEngine->stop();
    }
    closeStream();
    gEngine.reset();
}

} // extern "C"
