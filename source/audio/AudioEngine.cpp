#include "AudioEngine.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <thread>
#include <mutex>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}

static ALenum FormatFromChannels(int channels) {
    if (channels == 1) return AL_FORMAT_MONO16;
    if (channels == 2) return AL_FORMAT_STEREO16;
    return AL_FORMAT_STEREO16;
}

AudioEngine::AudioEngine() : m_fft(FFT_SIZE, false), m_running(true) {
    av_log_set_level(AV_LOG_ERROR);

    m_device = alcOpenDevice(nullptr);
    if (!m_device) {
        throw std::runtime_error("OpenAL: Failed to open device");
    }

    m_context = alcCreateContext(m_device, nullptr);
    if (!m_context || !alcMakeContextCurrent(m_context)) {
        alcCloseDevice(m_device);
        throw std::runtime_error("OpenAL: Failed to create or set context");
    }

    alGenSources(1, &m_source);
    alGenBuffers(1, &m_buffer);
    
    m_window.resize(FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE; ++i) {
        m_window[i] = 0.5f * (1.0f - std::cos(
            2.0f * static_cast<float>(M_PI) * i / (FFT_SIZE - 1)));
    }
    
    m_thread = std::thread(&AudioEngine::workerThread, this);
}

AudioEngine::~AudioEngine() {
    m_running = false;
    if (m_thread.joinable()) m_thread.join();

    stop();

    alDeleteSources(1, &m_source);
    alDeleteBuffers(1, &m_buffer);

    alcMakeContextCurrent(nullptr);
    if (m_context) alcDestroyContext(m_context);
    if (m_device) alcCloseDevice(m_device);
}

void AudioEngine::loadAndPlay(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_trackMutex);

    alSourceStop(m_source);
    m_playing = false;
    m_position.store(0.0);

    alSourcei(m_source, AL_BUFFER, 0);
    if (m_buffer != 0) {
        alDeleteBuffers(1, &m_buffer);
        m_buffer = 0;
    }

    if (!decodeFile(filePath)) {
        std::cerr << "Failed to load audio: " << filePath << "\n";
        m_currentFile.clear();
        m_duration.store(0.0);
        return;
    }
    
    alSourcei(m_source, AL_BUFFER, m_buffer);
    alSourcef(m_source, AL_GAIN, m_volume.load());

    alSourcePlay(m_source);
    m_playing = true;
    m_currentFile = filePath;
}

void AudioEngine::play() {
    if (m_buffer != 0) {
        alSourcePlay(m_source);
        m_playing = true;
    }
}

void AudioEngine::pause() {
    if (m_buffer != 0) {
        alSourcePause(m_source);
        m_playing = false;
    }
}

void AudioEngine::playPause() {
    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_PLAYING)
        pause();
    else
        play();
}

void AudioEngine::stop() {
    if (m_buffer != 0) {
        alSourceStop(m_source);
        m_playing = false;
        m_position.store(0.0);
    }
}

void AudioEngine::seek(double seconds) {
    if (seconds < 0) seconds = 0;
    if (seconds > m_duration.load()) seconds = m_duration.load();

    ALint state;
    alGetSourcei(m_source, AL_SOURCE_STATE, &state);
    if (state == AL_PLAYING || state == AL_PAUSED)
        alSourcef(m_source, AL_SEC_OFFSET, static_cast<float>(seconds));

    m_position.store(seconds);
}

void AudioEngine::setVolume(float v) {
    v = std::clamp(v, 0.0f, 2.0f);
    m_volume.store(v);
    alSourcef(m_source, AL_GAIN, v);
}

std::string AudioEngine::currentFile() const {
    return m_currentFile;
}

std::optional<AudioMetadata> AudioEngine::currentMetadata() const {
    if (m_currentFile.empty()) return std::nullopt;
    auto map = AddAudioFile(m_currentFile);
    auto it = map.find(m_currentFile);
    if (it != map.end()) return it->second;
    return std::nullopt;
}

void AudioEngine::workerThread() {
    while (m_running) {
        ALint state = 0;
        alGetSourcei(m_source, AL_SOURCE_STATE, &state);

        if (state == AL_PLAYING) {
            float offset = 0.0f;
            alGetSourcef(m_source, AL_SEC_OFFSET, &offset);
            m_position.store(static_cast<double>(offset));

            if (m_spectrumCb && !m_pcm.empty() && m_sampleRate > 0) {
                int64_t sampleIndex = static_cast<int64_t>(offset * m_sampleRate) * 2;

                if (sampleIndex + FFT_SIZE * 2 <= static_cast<int64_t>(m_pcm.size())) {
                    updateSpectrum(m_pcm.data() + sampleIndex);
                }
            }
        } else if (state != AL_PAUSED) {
            m_playing = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
}

bool AudioEngine::decodeFile(const std::string& path) {
    m_pcm.clear();

    AVFormatContext* fmt_ctx = nullptr;
    if (avformat_open_input(&fmt_ctx, path.c_str(), nullptr, nullptr) < 0) {
        std::cerr << "Failed to open file: " << path << std::endl;
        return false;
    }

    if (avformat_find_stream_info(fmt_ctx, nullptr) < 0) {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    int stream_idx = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
    if (stream_idx < 0) {
        avformat_close_input(&fmt_ctx);
        return false;
    }

    AVStream* audio_stream = fmt_ctx->streams[stream_idx];
    const AVCodec* codec = avcodec_find_decoder(audio_stream->codecpar->codec_id);

    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, audio_stream->codecpar);

    if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
        avcodec_free_context(&codec_ctx);
        avformat_close_input(&fmt_ctx);
        return false;
    }

    SwrContext* swr = swr_alloc();
    av_opt_set_chlayout(swr, "in_chlayout", &codec_ctx->ch_layout, 0);
    av_opt_set_int(swr, "in_sample_rate", codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "in_sample_fmt", codec_ctx->sample_fmt, 0);

    AVChannelLayout out_layout = {};
    av_channel_layout_default(&out_layout, 2);
    av_opt_set_chlayout(swr, "out_chlayout", &out_layout, 0);
    av_opt_set_int(swr, "out_sample_rate", codec_ctx->sample_rate, 0);
    av_opt_set_sample_fmt(swr, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    swr_init(swr);
    m_sampleRate = codec_ctx->sample_rate;

    AVPacket* packet = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();

    while (av_read_frame(fmt_ctx, packet) >= 0) {
        if (packet->stream_index == stream_idx) {
            if (avcodec_send_packet(codec_ctx, packet) == 0) {
                while (avcodec_receive_frame(codec_ctx, frame) == 0) {
                    int out_samples = swr_get_out_samples(swr, frame->nb_samples);
                    uint8_t* out_buffer = nullptr;
                    int out_linesize = 0;
                    av_samples_alloc(&out_buffer, &out_linesize, 2,
                                     out_samples, AV_SAMPLE_FMT_S16, 0);

                    int converted = swr_convert(
                        swr, &out_buffer, out_samples,
                        (const uint8_t**)frame->data, frame->nb_samples);

                    int data_size = av_samples_get_buffer_size(
                        nullptr, 2, converted, AV_SAMPLE_FMT_S16, 1);

                    m_pcm.insert(
                        m_pcm.end(),
                        (int16_t*)out_buffer,
                        (int16_t*)(out_buffer + data_size)
                    );

                    av_freep(&out_buffer);
                }
            }
        }
        av_packet_unref(packet);
    }

    if (!m_pcm.empty()) {
        m_duration.store(
            static_cast<double>(m_pcm.size()) /
            (m_sampleRate * 2)
        );
    } else {
        m_duration.store(0.0);
    }

    av_frame_free(&frame);
    av_packet_free(&packet);
    swr_free(&swr);
    avcodec_free_context(&codec_ctx);
    avformat_close_input(&fmt_ctx);

    if (m_pcm.empty()) return false;

    alGenBuffers(1, &m_buffer);
    alBufferData(m_buffer,
                 FormatFromChannels(2),
                 m_pcm.data(),
                 static_cast<ALsizei>(m_pcm.size() * sizeof(int16_t)),
                 m_sampleRate);

    return alGetError() == AL_NO_ERROR;
}

void AudioEngine::updateSpectrum(const int16_t* samples) {
    if (!m_spectrumCb) return;

    std::vector<std::complex<float>> in(FFT_SIZE);
    for (size_t i = 0; i < FFT_SIZE; ++i) {
        float s = samples[i * 2] / 32768.0f; 
        in[i] = std::complex<float>(s * m_window[i], 0.0f);
    }

    std::vector<std::complex<float>> out(FFT_SIZE);
    m_fft.transform(in.data(), out.data());

    std::vector<float> bins(64);
    for (int i = 0; i < 64; ++i)
        bins[i] = std::abs(out[i]);

    m_spectrumCb(bins.data(), 64);
}

float computeRMS(const std::vector<float>& spectrum) {
    if (spectrum.empty()) return 0.0f;
    float sum = 0.0f;
    for (float v : spectrum) sum += v * v;
    return std::sqrt(sum / spectrum.size());
}