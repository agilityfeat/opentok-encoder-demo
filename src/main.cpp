#include <atomic>
#include <iostream>
#include <opentok.h>
#include <chrono>
#include <ctime>
#include <dotenv.h>
#include <sstream>
#include <random>
#include <thread>
#include <mutex>
#include <condition_variable>
#include "fmt/format.h"
#include "otk_thread.h"

enum class LogLevel {
    Debug,
    Info,
    Warning,
    Error
};

class Logger {
public:
    explicit Logger(std::string tag, LogLevel logLevel = LogLevel::Debug) : tag(std::move(tag)), logLevel(logLevel) {}

    void setLogLevel(LogLevel level) {
        logLevel = level;
    }

    template<typename... Args>
    void log(LogLevel level, const std::string &format, const Args &... args) {
        if (level >= logLevel) {
            std::string logString = fmt::format(format, args...);
            outputLog(level, logString);
        }
    }

    template<typename... Args>
    void debug(const std::string &format, const Args &... args) {
        log(LogLevel::Debug, format, args...);
    }

    template<typename... Args>
    void warn(const std::string &format, const Args &... args) {
        log(LogLevel::Warning, format, args...);
    }

    template<typename... Args>
    void error(const std::string &format, const Args &... args) {
        log(LogLevel::Error, format, args...);
    }

private:
    std::string tag;
    LogLevel logLevel;

    void outputLog(LogLevel level, const std::string &logString) {
        std::string levelString;
        switch (level) {
            case LogLevel::Debug:
                levelString = "DEBUG";
                break;
            case LogLevel::Info:
                levelString = "INFO";
                break;
            case LogLevel::Warning:
                levelString = "WARNING";
                break;
            case LogLevel::Error:
                levelString = "ERROR";
                break;
        }

        std::cout << utcTime() << " [" << levelString << "] " << "(" << tag << ") " << logString << std::endl;
    }

    static std::string utcTime() {
        std::time_t time = std::time({});
        char timeString[std::size("yyyy-mm-ddThh:mm:ss.SSSZ")];
        std::strftime(std::data(timeString), std::size(timeString),
                      "%FT%TZ", std::gmtime(&time));
        std::stringstream ss;
        ss << timeString;
        return ss.str();
    }
};

constexpr auto API_KEY_ENV = "API_KEY";
constexpr auto SESSION_ID_ENV = "SESSION_ID";
constexpr auto TOKEN_ENV = "TOKEN";

const auto getApiKey = []() {
    return std::getenv(API_KEY_ENV);
};
const auto getSessionId = []() {
    return std::getenv(SESSION_ID_ENV);
};
const auto getToken = []() {
    return std::getenv(TOKEN_ENV);
};

class OpenTokAudioPublisher {
public:
    OpenTokAudioPublisher() = default;

    bool initialize() {
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

private:
    static otk_thread_func_return_type capturer_thread_start_function(void *arg) {
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
                double val = (INT16_MAX  * 0.75) * cos(2.0 * M_PI * 4.0 * time / 10.0 );
                samples[i] = (int16_t)val;
                time += 10.0 / 480.0;
            }
            otc_audio_device_write_capture_data(samples, 480);
            usleep(10 * 1000);
        }
        _this->isPublishing_ = false;

        otk_thread_func_return_value;
    }

    static otc_bool audio_device_destroy_capturer(const otc_audio_device *audio_device,
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

    static otc_bool audio_device_start_capturer(const otc_audio_device *audio_device,
                                                void *user_data) {
        auto _this = static_cast<OpenTokAudioPublisher *>(user_data);
        if (_this == nullptr) {
            return OTC_FALSE;
        }

        _this->logger.debug(__FUNCTION__);

        _this->exitAudioCapturerThread = false;
        if (otk_thread_create(&(_this->audioCapturerThread), &capturer_thread_start_function, user_data) != 0) {
            return OTC_FALSE;
        }

        return OTC_TRUE;
    }

    static otc_bool audio_device_get_capture_settings(const otc_audio_device *audio_device,
                                                      void *user_data,
                                                      struct otc_audio_device_settings *settings) {
        if (settings == nullptr) {
            return OTC_FALSE;
        }

        settings->number_of_channels = 1;
        settings->sampling_rate = 48000;
        return OTC_TRUE;
    }


    Logger logger{"OpenTokPublisher"};

    otk_thread_t audioCapturerThread{};
    std::atomic<bool> exitAudioCapturerThread{false};

    std::atomic<bool> isPublishing_{false};
};

class OpenTokVideoPublisher {
public:
    OpenTokVideoPublisher() = default;

    ~OpenTokVideoPublisher() {
        if (publisher) {
            otc_publisher_delete(publisher);
        }
    }

    bool initialize() {
        struct otc_video_capturer_callbacks videoCapturerCallbacks = {
                .init = &video_capturer_init,
                .destroy = &video_capturer_destroy,
                .start = &video_capturer_start,
                .get_capture_settings = &get_video_capturer_capture_settings,
                .user_data = this
        };

        struct otc_publisher_callbacks publisherCallbacks = {
                .on_stream_created = &on_publisher_stream_created,
                .on_stream_destroyed = &on_publisher_stream_destroyed,
                .on_error = &on_publisher_error,
                .user_data = this
        };

        publisher = otc_publisher_new("opentok-encoder-demo", &videoCapturerCallbacks, &publisherCallbacks);
        if (publisher == nullptr) {
            logger.error("OpenTokPublisher: Could not create otc publisher");
            return false;
        }
        return true;
    }

    bool publishToSession(otc_session *session) {
        if (!publisher) {
            logger.error("{}: publisher is null", __FUNCTION__);
            return false;
        }
        if (otc_session_publish(session, publisher) != OTC_SUCCESS) {
            logger.error("{}: could not publish to session", __FUNCTION__);
            return false;
        }
        return true;
    }

    bool unPublishFromSession(otc_session *session) {
        if (!publisher) {
            logger.error("{}: publisher is null", __FUNCTION__);
            return false;
        }
        if (!isPublishing_) {
            logger.error("{}: publisher is not publishing", __FUNCTION__);
            return false;
        }
        if (otc_session_unpublish(session, publisher) != OTC_SUCCESS) {
            logger.error("{}: could not un publish", __FUNCTION__);
            return false;
        }
        return true;
    }

private:
    static int generate_random_integer() {
        std::random_device rd;
        std::mt19937 mt(rd());
        std::uniform_int_distribution dist(1, RAND_MAX);
        return dist(mt);
    }

    /**
     * Video Capturer Callbacks
     */

    /**
     * This function executes on the otk thread created during video capture startup
     * On the otk thread, the program will run a loop to generate rendered frames. The frames are provided to the
     * OpenTok video publisher. The loop will sleep for a duration to achieve the desired frame rate.
     * @param user_data
     * @return
     */
    static otk_thread_func_return_type capturer_thread_start_function(void *user_data) {
        auto _this = static_cast<OpenTokVideoPublisher *>(user_data);
        if (_this == nullptr) {
            otk_thread_func_return_value;
        }

        _this->logger.debug(__FUNCTION__);
        _this->isPublishing_ = true;

        auto buffer = (uint8_t *) malloc(sizeof(uint8_t) * OpenTokVideoPublisher::width * OpenTokVideoPublisher::height * 4);

        while (!_this->exitVideoCapturerThread.load()) {
            memset(buffer, generate_random_integer() & 0xFF, OpenTokVideoPublisher::width * OpenTokVideoPublisher::height * 4);

            auto otcFrame = otc_video_frame_new(OTC_VIDEO_FRAME_FORMAT_ARGB32, OpenTokVideoPublisher::width, OpenTokVideoPublisher::height, buffer);
            if (otc_video_capturer_provide_frame(_this->videoCapturer, 0, otcFrame) != OTC_SUCCESS) {
                _this->logger.error("capturer_thread_start_function: Unable to provide frame");
            }
            if (otcFrame != nullptr) {
                otc_video_frame_delete(otcFrame);
            }
            usleep(1000 / OpenTokVideoPublisher::fps * 1000);
        }

        if (buffer != nullptr) {
            free(buffer);
        }

        otk_thread_func_return_value;
    }

    static otc_bool video_capturer_init(const otc_video_capturer *capturer, void *user_data) {
        auto _this = static_cast<OpenTokVideoPublisher *>(user_data);
        if (_this == nullptr) {
            return OTC_FALSE;
        }

        _this->logger.debug(__FUNCTION__);
        _this->videoCapturer = capturer;

        return OTC_TRUE;
    }

    static otc_bool video_capturer_start(const otc_video_capturer *capturer, void *user_data) {
        auto _this = static_cast<OpenTokVideoPublisher *>(user_data);
        if (_this == nullptr) {
            return OTC_FALSE;
        }

        _this->logger.debug(__FUNCTION__);

        _this->exitVideoCapturerThread = false;
        if (otk_thread_create(&(_this->videoCapturerThread), &capturer_thread_start_function, _this) != 0) {
            _this->logger.error("Error creating otk thread");
            return OTC_FALSE;
        }
        return OTC_TRUE;
    }

    static otc_bool video_capturer_destroy(const otc_video_capturer *capturer, void *user_data) {
        auto _this = static_cast<OpenTokVideoPublisher *>(user_data);
        if (_this == nullptr) {
            return OTC_FALSE;
        }

        _this->logger.debug(__FUNCTION__);

        _this->exitVideoCapturerThread = true;
        otk_thread_join(_this->videoCapturerThread);

        return OTC_TRUE;
    }

    static otc_bool get_video_capturer_capture_settings(const otc_video_capturer *capturer,
                                                        void *user_data,
                                                        struct otc_video_capturer_settings *settings) {
        settings->format = OTC_VIDEO_FRAME_FORMAT_ARGB32;
        settings->width = OpenTokVideoPublisher::width;
        settings->height = OpenTokVideoPublisher::height;
        settings->fps = OpenTokVideoPublisher::fps;
        settings->mirror_on_local_render = OTC_FALSE;
        settings->expected_delay = 0;

        return OTC_TRUE;
    }

    /**
     * Publisher Callbacks
     */

    static void on_publisher_stream_created(otc_publisher *publisher,
                                            void *user_data,
                                            const otc_stream *stream) {
        auto _this = static_cast<OpenTokVideoPublisher*>(user_data);
        _this->logger.debug(__FUNCTION__);
    }

    static void on_publisher_stream_destroyed(otc_publisher *publisher,
                                              void *user_data,
                                              const otc_stream *stream) {
        auto _this = static_cast<OpenTokVideoPublisher*>(user_data);
        _this->logger.debug(__FUNCTION__);
    }

    static void on_publisher_error(otc_publisher *publisher,
                                   void *user_data,
                                   const char* error_string,
                                   enum otc_publisher_error_code error_code) {
        auto _this = static_cast<OpenTokVideoPublisher*>(user_data);
        _this->logger.error("{}: Publisher error. Error code: {}", __FUNCTION__, error_string);
    }

    Logger logger{"OpenTokPublisher"};

    otk_thread_t videoCapturerThread{};
    std::atomic<bool> exitVideoCapturerThread{false};

    const otc_video_capturer *videoCapturer{nullptr};
    otc_publisher *publisher{nullptr};

    std::atomic<bool> isPublishing_{false};

    const static int width = 1280;
    const static int height = 720;
    const static int fps = 1;
};

class OpenTokClient {
public:
    OpenTokClient(std::string apiKey, std::string sessionId, std::string token) : apiKey(std::move(apiKey)),
                                                               sessionId(std::move(sessionId)), token(std::move(token)) {
        if (otc_init(nullptr) != OTC_SUCCESS) {
            throw std::runtime_error("Could not init opentok library");
        }
    }

    ~OpenTokClient() {
        if (session) {
            otc_session_delete(session);
        }
        if (audioPublisher) {
            free(audioPublisher);
        }
        if (videoPublisher) {
            free(videoPublisher);
        }
        if (otc_destroy() != OTC_SUCCESS) {
            logger.error("Error destroying opentok library");
        }
    }

    bool startPublishing() {
        logger.debug(__FUNCTION__);
        if (!initializeSession()) {
            logger.error("{}: unable to initialize session", __FUNCTION__);
            return false;
        }
        if (!initializePublisher()) {
            logger.error("{}: unable to initialize publisher", __FUNCTION__);
            return false;
        }
        if (!connectSession()) {
            logger.error("{}: unable to connect session", __FUNCTION__);
            return false;
        }
        return true;
    }

    bool stopPublishing() {
        logger.debug(__FUNCTION__);
        if (!session) {
            return false;
        }
        if (!videoPublisher) {
            logger.debug("{}: publisher is null", __FUNCTION__);
            return false;
        }
        if (!videoPublisher->unPublishFromSession(session)) {
            logger.debug("{}: error unpublishing from session", __FUNCTION__);
            return false;
        }
        if (isConnected_ && otc_session_disconnect(session) != OTC_SUCCESS) {
            logger.debug("{}: error disconnecting session", __FUNCTION__);
            return false;
        }
        return true;
    }

private:
    bool initializeSession() {
        logger.debug(__FUNCTION__);
        if (session != nullptr) {
            logger.error("Session already initialized");
        }

        struct otc_session_callbacks sessionCallbacks{
                .on_connected = &on_session_connected,
                .on_disconnected = &on_session_disconnected,
                .on_error = &on_session_error,
                .user_data = this
        };

        session = otc_session_new(
                apiKey.c_str(),
                sessionId.c_str(),
                &sessionCallbacks
        );
        if (session == nullptr) {
            logger.error("Could not create opentok session");
            return false;
        }

        return true;
    }

    bool initializePublisher() {
        logger.debug(__FUNCTION__);

        audioPublisher = new OpenTokAudioPublisher();
        if (!audioPublisher->initialize()) {
            logger.error("{}: Could not initialize audio publisher");
            return false;
        }

        videoPublisher = new OpenTokVideoPublisher();
        if (!videoPublisher->initialize()) {
            logger.error("{}: Could not initialize video publisher");
            return false;
        }

        return true;
    }

    bool connectSession() {
        if (session == nullptr) {
            logger.error("{}: Could not create opentok session", __FUNCTION__);
            return false;
        }

        if (otc_session_connect(session, token.c_str()) != OTC_SUCCESS) {
            logger.error("{}: could not connect session", __FUNCTION__);
            return false;
        }
        return true;
    }

    /**
     * Session Callbacks
     */

    static void on_session_connected(otc_session *session, void *user_data) {
        auto _this = static_cast<OpenTokClient *>(user_data);
        _this->logger.debug(__FUNCTION__);

        _this->isConnected_ = true;

        if (session == nullptr) {
            _this->logger.error("{}: session is null", __FUNCTION__);
            return;
        }
        if (_this->videoPublisher == nullptr) {
            _this->logger.error("{}: publisher is null", __FUNCTION__);
            return;
        }

        if (!_this->videoPublisher->publishToSession(session)) {
            _this->logger.error("{}: could not publish to session", __FUNCTION__);
            return;
        }

        _this->logger.debug("{}: session successfully connected", __FUNCTION__);
    }

    static void on_session_disconnected(otc_session *session, void *user_data) {
        auto _this = static_cast<OpenTokClient *>(user_data);
        _this->logger.debug(__FUNCTION__);
        _this->isConnected_ = false;
    }

    static void on_session_error(otc_session *session, void *user_data, const char *error_string,
                                 enum otc_session_error_code error) {
        auto _this = static_cast<OpenTokClient *>(user_data);
        _this->logger.debug("{}: {}", __FUNCTION__, error_string);
    }
    std::string apiKey;
    std::string sessionId;
    std::string token;

    otc_session *session{nullptr};
    OpenTokVideoPublisher *videoPublisher{nullptr};
    OpenTokAudioPublisher *audioPublisher{nullptr};
    Logger logger{"OpenTokClient"};

    std::atomic<bool> isConnected_{false};
};

int main() {
    dotenv::init();

    Logger logger{"Main"};

    auto apiKey = getApiKey();
    auto sessionId = getSessionId();
    auto token = getToken();
    logger.debug("Creating OpenTok Client, API Key: {}, Session ID: {}", apiKey, sessionId);

    OpenTokClient client(apiKey, sessionId, token);

    logger.debug("Client created");

    if (client.startPublishing()) {
        logger.debug("Publisher started successfully");
    } else {
        logger.error("Could not start publishing");
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(30));

    if (client.stopPublishing()) {
        logger.debug("Publisher stopped successfully");
    } else {
        logger.error("Could not stop publishing");
        return 1;
    }

    return 0;
}
