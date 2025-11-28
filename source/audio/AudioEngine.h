#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <optional>
#include <functional>
#include <complex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
#include <AL/al.h>
#include <AL/alc.h>

#include "files.h"  
#include <kissfft.hh>  
#include "AudioManager.h"

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();

    void play();
    void pause();
    void playPause();
    void stop();
    void next();
    void prev();
    void seek(double seconds);

    void setPlaylist(const std::vector<std::string>& files, size_t startIndex = 0);
    void clearPlaylist() { setPlaylist({}); }

    void shuffle(bool enable)    { m_shuffle = enable; }
    void repeat(bool enable)     { m_repeat = enable; }

    bool isPlaying() const       { return m_playing.load(); }
    double position() const      { return m_position.load(); }
    double duration() const      { return m_duration.load(); }
    float volume() const         { return m_volume.load(); }
    void setVolume(float v);

    size_t currentIndex() const  { return m_currentIdx.load(); }
    std::optional<AudioMetadata> currentMetadata() const;

    using SpectrumCallback = std::function<void(const float*, int)>;
    void setSpectrumCallback(SpectrumCallback cb) { m_spectrumCb = cb; }

    void enableCrossfade(float seconds = 3.0f);

private:
    void workerThread();
    bool decodeCurrentTrack(const std::string& file);
    void updateSpectrum(const int16_t* samples, size_t count);

    
    ALCdevice*  m_device{nullptr};
    ALCcontext* m_context{nullptr};
    ALuint      m_source{0};
    std::vector<ALuint> m_buffers;
    static constexpr int NUM_BUFFERS = 8;

    AVFormatContext* m_fmt{nullptr};
    AVCodecContext*  m_codec{nullptr};
    SwrContext*      m_swr{nullptr};
    int              m_streamIdx{-1};

    std::atomic<bool> m_running{true};
    std::atomic<bool> m_playing{false};
    std::atomic<bool> m_seeking{false};
    std::thread       m_thread;
    
    mutable std::mutex m_playlistMutex;
    std::vector<std::string> m_playlist;
    std::atomic<size_t>      m_currentIdx{0};
    std::atomic<bool>        m_shuffle{false};
    std::atomic<bool>        m_repeat{false};

    std::atomic<double> m_position{0.0};
    std::atomic<double> m_duration{0.0};
    std::atomic<float>  m_volume{1.0f};

    std::queue<std::vector<int16_t>> m_pcmQueue;
    std::mutex                       m_queueMutex;
    std::condition_variable          m_queueCV;

    SpectrumCallback m_spectrumCb;
    kissfft<float>   m_fft;
    std::vector<float> m_fftWindow;
    static constexpr size_t FFT_SIZE = 2048;
    
    std::atomic<bool>  m_crossfadeEnabled{false};
    std::atomic<float> m_crossfadeSeconds{3.0f};
    ALuint             m_fadeSource{0};

    std::atomic<bool> m_interrupt{false};

    std::vector<int16_t> m_fullBuffer;
    std::mutex m_fullBufferMutex;
    int m_fullSampleRate{0};
    int m_fullChannels{0};
};

float computeRMS(const std::vector<float>& spectrum);