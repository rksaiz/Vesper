

#include "AudioEngine.h"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <cstring>
#include <random>
#include <algorithm>
#include <cmath>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

#pragma comment(lib, "avformat.lib")
#pragma comment(lib, "avcodec.lib")
#pragma comment(lib, "swresample.lib")
#pragma comment(lib, "avutil.lib")
#pragma comment(lib, "openal32.lib")

static ALenum FormatFromChannels(int channels) {
    if (channels == 1) return AL_FORMAT_MONO16;
    if (channels == 2) return AL_FORMAT_STEREO16;
    return AL_FORMAT_STEREO16;
}

AudioEngine::AudioEngine() : m_fft(FFT_SIZE, false) {
    av_log_set_level(AV_LOG_ERROR);

    m_device = alcOpenDevice(nullptr);
    if (!m_device) throw std::runtime_error("OpenAL: cannot open device");

    m_context = alcCreateContext(m_device, nullptr);
    if (!m_context || !alcMakeContextCurrent(m_context)) {
        if (m_context) alcDestroyContext(m_context);
        alcCloseDevice(m_device);
        throw std::runtime_error("OpenAL: cannot create/make context current");
    }
    alGenSources(1, &m_source);

    m_buffers.resize(NUM_BUFFERS);
    alGenBuffers(NUM_BUFFERS, m_buffers.data());

    m_fftWindow.resize(FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE; ++i)
        m_fftWindow[i] = 0.5f * (1.0f - std::cos(2.0f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));

    m_thread = std::thread(&AudioEngine::workerThread, this);
}

AudioEngine::~AudioEngine() {
    m_running = false;
    m_interrupt = true;
    m_queueCV.notify_all();
    if (m_thread.joinable()) m_thread.join();

    stop();

    if (m_fadeSource) alDeleteSources(1, &m_fadeSource);
    alDeleteSources(1, &m_source);
    if (!m_buffers.empty()) alDeleteBuffers(static_cast<ALsizei>(m_buffers.size()), m_buffers.data());

    alcMakeContextCurrent(nullptr);
    if (m_context) alcDestroyContext(m_context);
    if (m_device) alcCloseDevice(m_device);
}


void AudioEngine::play() {
    alSourcePlay(m_source);
    m_playing = true;
}
void AudioEngine::pause() {
    alSourcePause(m_source);
    m_playing = false;
}
void AudioEngine::playPause() {
    ALint st = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &st);
    if (st == AL_PLAYING) {
        alSourcePause(m_source);
        m_playing = false;
    } else {
        alSourcePlay(m_source);
        m_playing = true;
    }
}
void AudioEngine::stop() {
    m_interrupt = true;
    m_playing = false;

    alSourceStop(m_source);
    
    ALint queued = 0;
    alGetSourcei(m_source, AL_BUFFERS_QUEUED, &queued);
    while (queued--) {
        ALuint b = 0;
        alSourceUnqueueBuffers(m_source, 1, &b);
    }
}


void AudioEngine::setPlaylist(const std::vector<std::string>& files, size_t startIndex) {
    stop();
    {
        std::lock_guard<std::mutex> lock(m_playlistMutex);
        m_playlist = files;
        m_currentIdx = (startIndex < files.size()) ? startIndex : 0;
    }
    m_position = 0.0;
    m_duration = 0.0;
}

void AudioEngine::next() {
    stop();
    {
        std::lock_guard<std::mutex> lock(m_playlistMutex);
        if (m_playlist.empty()) return;
        if (m_shuffle) {
            std::random_device rd; std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dist(0, m_playlist.size() - 1);
            m_currentIdx = dist(gen);
        } else {
            m_currentIdx = (m_currentIdx + 1) % m_playlist.size();
        }
    }
}

void AudioEngine::prev() {
    stop();
    {
        std::lock_guard<std::mutex> lock(m_playlistMutex);
        if (m_playlist.empty()) return;
        m_currentIdx = (m_currentIdx == 0) ? m_playlist.size() - 1 : m_currentIdx - 1;
    }
}


void AudioEngine::workerThread() {
    while (m_running) {
        std::string currentFile;
        {
            std::lock_guard<std::mutex> lock(m_playlistMutex);
            if (m_currentIdx >= m_playlist.size()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                continue;
            }
            currentFile = m_playlist[m_currentIdx];
        }
        m_interrupt = false;
        bool ok = decodeCurrentTrack(currentFile);
        if (m_interrupt) {
            
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        
        if (!m_repeat) {
            std::lock_guard<std::mutex> lock(m_playlistMutex);
            if (!m_playlist.empty()) {
                if (m_shuffle) {
                    std::random_device rd; std::mt19937 gen(rd());
                    std::uniform_int_distribution<size_t> dist(0, m_playlist.size() - 1);
                    m_currentIdx = dist(gen);
                } else {
                    m_currentIdx = (m_currentIdx + 1) % m_playlist.size();
                }
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

bool AudioEngine::decodeCurrentTrack(const std::string& file) {
    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, file.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "[AudioEngine] Cannot open file: " << file << std::endl;
        return false;
    }
    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        avformat_close_input(&fmt_ctx);
        std::cerr << "[AudioEngine] No stream info" << std::endl;
        return false;
    }

    int stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (stream_idx < 0) {
        avformat_close_input(&fmt_ctx);
        return false;
    }
    AVStream* audio_stream = fmt_ctx->streams[stream_idx];

    const AVCodec* codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);
    if (!codec) { avformat_close_input(&fmt_ctx); return false; }
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar);
    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    const AVSampleFormat out_fmt = AV_SAMPLE_FMT_S16;
    int out_rate = codec_ctx->sample_rate;
    int out_channels = 2;

    SwrContext* swr = swr_alloc();
    if (!swr) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVChannelLayout out_layout;
    av_channel_layout_default(&out_layout, out_channels);

    AVChannelLayout in_layout = codec_ctx->ch_layout;
    if (in_layout.nb_channels == 0) {
        int ch = audio_stream->codecpar->ch_layout.nb_channels;
        if (ch <= 0) ch = audio_stream->codecpar->ch_layout.nb_channels;
        if (ch <= 0) ch = 2;
        av_channel_layout_default(&in_layout, ch);
    }

    av_opt_set_chlayout(swr, "in_chlayout", &in_layout, 0);
    av_opt_set_chlayout(swr, "out_chlayout", &out_layout, 0);
    av_opt_set_int(swr, "in_sample_rate", codec_ctx->sample_rate, 0);
    av_opt_set_int(swr, "out_sample_rate", out_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", codec_ctx->sample_fmt, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", out_fmt, 0);

    if (swr_init(swr) < 0) {
        std::cerr << "[AudioEngine] swr_init failed" << std::endl;
        swr_free(&swr);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    std::vector<uint8_t> tmpBytes;
    tmpBytes.clear();

    while (!m_interrupt && av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index != stream_idx) {
            av_packet_unref(packet);
            continue;
        }
        if (avcodec_send_packet(codec_ctx, packet) < 0) {
            av_packet_unref(packet);
            continue;
        }
        av_packet_unref(packet);

        while (!m_interrupt && avcodec_receive_frame(codec_ctx, frame) == 0) {
            int64_t delay = swr_get_delay(swr, codec_ctx->sample_rate);
            int out_samples = static_cast<int>(av_rescale_rnd(delay + frame->nb_samples, out_rate, codec_ctx->sample_rate, AV_ROUND_UP));
            uint8_t** outptr = nullptr;
            if (av_samples_alloc_array_and_samples(&outptr, nullptr, out_channels, out_samples, out_fmt, 0) < 0) {
                continue;
            }

            int converted = swr_convert(swr, outptr, out_samples, (const uint8_t**)frame->data, frame->nb_samples);
            if (converted <= 0) {
                av_freep(&outptr[0]);
                av_freep(&outptr);
                continue;
            }

            int bytes = av_samples_get_buffer_size(nullptr, out_channels, converted, out_fmt, 1);
            size_t prevSize = tmpBytes.size();
            tmpBytes.resize(prevSize + bytes);
            memcpy(tmpBytes.data() + prevSize, outptr[0], bytes);

            av_freep(&outptr[0]);
            av_freep(&outptr);
        }
    }

    if (!m_interrupt) {
        avcodec_send_packet(codec_ctx, nullptr);
        while (!m_interrupt && avcodec_receive_frame(codec_ctx, frame) == 0) {
            int64_t delay = swr_get_delay(swr, codec_ctx->sample_rate);
            int out_samples = static_cast<int>(av_rescale_rnd(delay + frame->nb_samples, out_rate, codec_ctx->sample_rate, AV_ROUND_UP));
            uint8_t** outptr = nullptr;
            if (av_samples_alloc_array_and_samples(&outptr, nullptr, out_channels, out_samples, out_fmt, 0) < 0) {
                continue;
            }
            int converted = swr_convert(swr, outptr, out_samples, (const uint8_t**)frame->data, frame->nb_samples);
            if (converted > 0) {
                int bytes = av_samples_get_buffer_size(nullptr, out_channels, converted, out_fmt, 1);
                size_t prevSize = tmpBytes.size();
                tmpBytes.resize(prevSize + bytes);
                memcpy(tmpBytes.data() + prevSize, outptr[0], bytes);
            }
            av_freep(&outptr[0]);
            av_freep(&outptr);
        }
    }

    if (m_interrupt) {
        av_frame_free(&frame);
        av_packet_free(&packet);
        if (swr) swr_free(&swr);
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(m_fullBufferMutex);
        m_fullBuffer.clear();
        m_fullBuffer.resize(tmpBytes.size() / sizeof(int16_t));
        memcpy(m_fullBuffer.data(), tmpBytes.data(), tmpBytes.size());
        m_fullSampleRate = out_rate;
        m_fullChannels = out_channels;
    }

    if (!tmpBytes.empty()) {
        size_t totalSamples = tmpBytes.size() / (sizeof(int16_t) * out_channels);
        m_duration.store(static_cast<double>(totalSamples) / static_cast<double>(out_rate));
    } else {
        m_duration.store(0.0);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    if (swr) swr_free(&swr);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    if (m_interrupt) return false;
    if (m_fullBuffer.empty()) {
        std::cerr << "[AudioEngine] no PCM decoded for file: " << file << std::endl;
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(m_fullBufferMutex);
        ALuint buf = m_buffers[0];
        ALenum alfmt = FormatFromChannels(m_fullChannels);
        alBufferData(buf, alfmt, m_fullBuffer.data(), static_cast<ALsizei>(m_fullBuffer.size() * sizeof(int16_t)), m_fullSampleRate);
        alSourcei(m_source, AL_BUFFER, static_cast<ALint>(buf));
        alSourcef(m_source, AL_GAIN, m_volume.load());
        alSourcePlay(m_source);
        m_playing = true;
    }

    while (!m_interrupt && m_running) {
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);

        float sec = 0.0f;
        alGetSourcef(m_source, AL_SEC_OFFSET, &sec);
        m_position.store(static_cast<double>(sec));

        if (m_spectrumCb) {
            std::lock_guard<std::mutex> lock(m_fullBufferMutex);
            if (!m_fullBuffer.empty() && m_fullSampleRate > 0) {
                int64_t sampleIndex = static_cast<int64_t>(sec * m_fullSampleRate) * m_fullChannels;
               
                if (sampleIndex < 0) sampleIndex = 0;
                if (static_cast<size_t>(sampleIndex + FFT_SIZE * 2) <= m_fullBuffer.size()) {
                    
                    std::vector<int16_t> win(FFT_SIZE * m_fullChannels);
                    memcpy(win.data(), m_fullBuffer.data() + sampleIndex, FFT_SIZE * m_fullChannels * sizeof(int16_t));
                    updateSpectrum(reinterpret_cast<const int16_t*>(win.data()), win.size());
                } else {                  
                    size_t available = (m_fullBuffer.size() > static_cast<size_t>(sampleIndex)) ? (m_fullBuffer.size() - sampleIndex) : 0;
                    if (available >= static_cast<size_t>(m_fullChannels)) {
                        std::vector<int16_t> temp(FFT_SIZE * m_fullChannels, 0);
                        memcpy(temp.data(), m_fullBuffer.data() + sampleIndex, available * sizeof(int16_t));
                        updateSpectrum(reinterpret_cast<const int16_t*>(temp.data()), temp.size());
                    }
                }
            }
        }
        if (state != AL_PLAYING) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }

    if (m_interrupt) {
        alSourceStop(m_source);
        m_playing = false;
        return false;
    }
  
    m_playing = false;
    m_position.store(m_duration.load());
    return true;
}

void AudioEngine::updateSpectrum(const int16_t* samples, size_t count) {
    if (!m_spectrumCb) return;
    if (count < FFT_SIZE * m_fullChannels) return; 
    
    std::vector<std::complex<float>> in(FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE; ++i) {
        int16_t s = samples[i * m_fullChannels]; 
        float fs = static_cast<float>(s) / 32768.0f;
        in[i] = std::complex<float>(fs * m_fftWindow[i], 0.0f);
    }
    std::vector<std::complex<float>> out(FFT_SIZE);
    m_fft.transform(in.data(), out.data());
    std::vector<float> bins(64, 0.0f);
    for (int i = 0; i < 64; ++i) bins[i] = std::abs(out[i]);
    m_spectrumCb(bins.data(), 64);
}

void AudioEngine::seek(double seconds) {
    if (seconds < 0.0) seconds = 0.0;
    double dur = m_duration.load();
    if (dur > 0.0 && seconds > dur) seconds = dur;
    
    ALint state = 0;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_PLAYING || state == AL_PAUSED) {
        alSourcef(m_source, AL_SEC_OFFSET, static_cast<float>(seconds));
        m_position.store(seconds);
    } else {       
        m_position.store(seconds);
    }
}

void AudioEngine::enableCrossfade(float seconds) {
    m_crossfadeEnabled = true;
    m_crossfadeSeconds = seconds;
    if (m_fadeSource == 0) alGenSources(1, &m_fadeSource);
}

float computeRMS(const std::vector<float>& spectrum) {
    if (spectrum.empty()) return 0.0f;
    float sumSquares = 0.0f;
    for (float v : spectrum)
        sumSquares += v * v;
    return std::sqrt(sumSquares / spectrum.size());
}
