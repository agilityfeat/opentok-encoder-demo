#include <cmath>
#include "opentok_client.hpp"

OpenTokAudioPublisher::OpenTokAudioPublisher() = default;

bool OpenTokAudioPublisher::initialize() {
    struct otc_audio_device_callbacks audioDeviceCallbacks = {
            .destroy_capturer = &audio_device_destroy_capturer,
            .start_capturer = &audio_device_start_capturer,
            .get_capture_settings = &audio_device_get_capture_settings,
            .user_data = this,
    };

    if (otc_set_audio_device(&audioDeviceCallbacks) != OTC_SUCCESS) {
        logger.error("{}: Error setting audio device", __FUNCTION__);
        return false;
    }
    return true;
}

otk_thread_func_return_type OpenTokAudioPublisher::capturer_thread_start_function(void *arg) {
    auto _this = static_cast<OpenTokAudioPublisher *>(arg);
    if (_this == nullptr) {
        otk_thread_func_return_value;
    }

    _this->logger.debug(__FUNCTION__);

    int16_t samples[480];
    static double time = 0;

    _this->isPublishing_ = true;
    while (!_this->exitAudioCapturerThread.load()) {
        for (int i = 0; i < 480; i++) {
            double val = (INT16_MAX * 0.75) * cos(2.0 * M_PI * 4.0 * time / 10.0);
            samples[i] = (int16_t) val;
            time += 10.0 / 480.0;
        }
        otc_audio_device_write_capture_data(samples, 480);
        usleep(10 * 1000);
    }
    _this->isPublishing_ = false;

    otk_thread_func_return_value;
}

otc_bool OpenTokAudioPublisher::audio_device_destroy_capturer(const otc_audio_device *audio_device,
                                              void *user_data) {
    auto _this = static_cast<OpenTokAudioPublisher *>(user_data);
    if (_this == nullptr) {
        return OTC_FALSE;
    }

    _this->logger.debug(__FUNCTION__);

    _this->exitAudioCapturerThread = true;
    otk_thread_join(_this->audioCapturerThread);

    return OTC_TRUE;
}

otc_bool OpenTokAudioPublisher::audio_device_start_capturer(
        const otc_audio_device *audio_device,
        void *user_data
) {
    auto _this = static_cast<OpenTokAudioPublisher *>(user_data);
    if (_this == nullptr) {
        return OTC_FALSE;
    }

    _this->logger.debug(__FUNCTION__);

    _this->exitAudioCapturerThread = false;
    if (otk_thread_create(
            &(_this->audioCapturerThread),
            &capturer_thread_start_function,
            user_data
    ) != 0) {
        return OTC_FALSE;
    }

    return OTC_TRUE;
}

otc_bool OpenTokAudioPublisher::audio_device_get_capture_settings(const otc_audio_device *audio_device,
                                                  void *user_data,
                                                  struct otc_audio_device_settings *settings) {
    if (settings == nullptr) {
        return OTC_FALSE;
    }

    settings->number_of_channels = 1;
    settings->sampling_rate = 48000;
    return OTC_TRUE;
}
