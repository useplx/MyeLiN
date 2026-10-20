#pragma once

#include <complex>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace Mln::Audio {
    struct Spectrogram {
            std::size_t frames = 0;
            std::size_t bins = 0;
            std::vector<float> values;
    };

    struct EventDetectorConfig {
            float rms_reference = 0.01f;
            float flux_reference = 50.0f;
            float minimum_similarity = 0.90f;
            float full_similarity = 1.0f;
            float margin_reference = 0.02f;
    };

    std::vector<float> pcm_s16_to_f32(std::span<const int16_t> input);
    std::vector<float> mix_to_mono(std::span<const float> interleaved, std::size_t channels);
    std::vector<float> mix_to_mono_phase_safe(std::span<const float> interleaved, std::size_t channels);
    std::vector<float> resample_linear(std::span<const float> input, uint32_t source_rate,
                                       uint32_t target_rate);
    std::vector<float> resample_bandlimited(std::span<const float> input, uint32_t source_rate,
                                           uint32_t target_rate);
    std::vector<float> hann_window(std::size_t size);
    float rms(std::span<const float> input);
    std::vector<std::complex<float>> fft(std::span<const float> input);
    Spectrogram stft_power(std::span<const float> input, std::size_t fft_size,
                           std::size_t hop_size);
    std::vector<float> mel_filterbank(uint32_t sample_rate, std::size_t fft_size,
                                      std::size_t mel_bins, float minimum_hz = 0.0f,
                                      float maximum_hz = 0.0f);
    Spectrogram log_mel(std::span<const float> input, uint32_t sample_rate, std::size_t fft_size,
                        std::size_t hop_size, std::size_t mel_bins, float floor = 1.0e-6f);
    Spectrogram spectral_noise_gate(const Spectrogram &spectrum, float noise_quantile = 0.2f,
                                    float floor = 0.0f);
    float spectral_flux(const Spectrogram &spectrum);
    float event_probability(float energy, float flux, float similarity, float class_margin,
                            const EventDetectorConfig &config = {});
    std::vector<float> fingerprint(const Spectrogram &log_mel_spectrum);
    std::vector<float> temporal_fingerprint(const Spectrogram &log_mel_spectrum);
    float cosine_similarity(std::span<const float> left, std::span<const float> right);
    std::vector<float> similarity(std::span<const float> query, std::span<const float> references,
                                  std::size_t reference_count);
    std::vector<float> prototype_cnn_logits(std::span<const float> query,
                                            std::span<const float> prototypes,
                                            std::size_t class_count);
    std::vector<float> softmax(std::span<const float> logits, float temperature = 1.0f);
    std::vector<float> augment(std::span<const float> signal, float gain,
                               std::span<const float> background = {}, float background_gain = 0,
                               int64_t shift = 0, float clip = 1.0f, bool normalize = false);
}
