#pragma once

#include <string>
#include <atomic>
#include <thread>
#include <mutex>
#include <functional>
#include <optional>
#include <vector>
#include "files.h"

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
#include <AL/al.h>
#include <AL/alc.h>

#include <kissfft.hh>  
#include "AudioManager.h"

class AudioEngine {
public:
    AudioEngine();
    ~AudioEngine();
    
    void loadAndPlay(const std::string& filePath);
    void play();
    void pause();
    void playPause();
    void stop();
    void seek(double seconds);
    void setVolume(float v);

    bool isPlaying() const { return m_playing.load(); }
    double position() const { return m_position.load(); }
    double duration() const { return m_duration.load(); }
    float volume() const { return m_volume.load(); }
    std::string currentFile() const;
    std::optional<AudioMetadata> currentMetadata() const;

    using SpectrumCallback = std::function<void(const float*, int)>;
    void setSpectrumCallback(SpectrumCallback cb) { m_spectrumCb = cb; }

private:
    void workerThread();
    bool decodeFile(const std::string& path);
    void updateSpectrum(const int16_t* samples);
    
    ALCdevice*  m_device{nullptr};
    ALCcontext* m_context{nullptr};
    ALuint      m_source{0};
    ALuint      m_buffer{0};
    
    AVFormatContext* m_fmt{nullptr};
    AVCodecContext*  m_codec{nullptr};
    SwrContext*      m_swr{nullptr};
    int              m_streamIdx{-1};
    
    std::vector<int16_t> m_pcm;
    int m_sampleRate{0};
    
    std::atomic<bool>    m_running{true};
    std::atomic<bool>    m_playing{false};
    std::atomic<double>  m_position{0.0};
    std::atomic<double>  m_duration{0.0};
    std::atomic<float>   m_volume{0.5f};
    std::string          m_currentFile;
    std::atomic<bool> m_repeat{false};
    std::atomic<bool> m_shuffle{false};
    
    SpectrumCallback     m_spectrumCb;
    kissfft<float>       m_fft;
    std::vector<float>   m_window;
    static constexpr size_t FFT_SIZE = 2048;

    std::thread m_thread;
    std::atomic<bool> m_needNewTrack{false};
    std::string m_pendingFile;
    std::mutex m_trackMutex;
};

float computeRMS(const std::vector<float>& spectrum);