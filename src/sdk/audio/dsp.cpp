#include "mln/audio/dsp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace Mln::Audio {
    std::vector<float> pcm_s16_to_f32(std::span<const int16_t> input) {
        std::vector<float> output(input.size());
        std::transform(input.begin(), input.end(), output.begin(),
                       [](int16_t value) { return static_cast<float>(value) / 32768.0f; });
        return output;
    }

    std::vector<float> mix_to_mono(std::span<const float> input, std::size_t channels) {
        if (channels == 0 || input.size() % channels != 0)
            throw std::invalid_argument("channel count does not divide the PCM buffer");
        std::vector<float> output(input.size() / channels, 0.0f);
        for (std::size_t frame = 0; frame < output.size(); ++frame)
            for (std::size_t channel = 0; channel < channels; ++channel)
                output[frame] += input[frame * channels + channel] / static_cast<float>(channels);
        return output;
    }

    std::vector<float> resample_linear(std::span<const float> input, uint32_t source_rate,
                                       uint32_t target_rate) {
        if (source_rate == 0 || target_rate == 0)
            throw std::invalid_argument("sample rates must be positive");
        if (input.empty() || source_rate == target_rate)
            return {input.begin(), input.end()};
        const std::size_t size = static_cast<std::size_t>(
            std::llround(static_cast<double>(input.size()) * target_rate / source_rate));
        std::vector<float> output(size);
        for (std::size_t index = 0; index < size; ++index) {
            const double position = static_cast<double>(index) * source_rate / target_rate;
            const std::size_t left = std::min(static_cast<std::size_t>(position), input.size() - 1);
            const std::size_t right = std::min(left + 1, input.size() - 1);
            const float fraction = static_cast<float>(position - left);
            output[index] = input[left] + (input[right] - input[left]) * fraction;
        }
        return output;
    }

    std::vector<float> hann_window(std::size_t size) {
        if (size < 2)
            throw std::invalid_argument("window size must be at least two");
        std::vector<float> output(size);
        for (std::size_t index = 0; index < size; ++index)
            output[index] =
                0.5f - 0.5f * std::cos(2.0f * std::numbers::pi_v<float> *
                                       static_cast<float>(index) / static_cast<float>(size - 1));
        return output;
    }

    float rms(std::span<const float> input) {
        if (input.empty())
            return 0.0f;
        double sum = 0.0;
        for (float value : input)
            sum += static_cast<double>(value) * value;
        return static_cast<float>(std::sqrt(sum / input.size()));
    }

    std::vector<std::complex<float>> fft(std::span<const float> input) {
        const std::size_t size = input.size();
        if (size == 0 || (size & (size - 1)) != 0)
            throw std::invalid_argument("FFT size must be a non-zero power of two");
        std::vector<std::complex<float>> output(input.begin(), input.end());
        for (std::size_t index = 1, reversed = 0; index < size; ++index) {
            std::size_t bit = size >> 1;
            while (reversed & bit) {
                reversed ^= bit;
                bit >>= 1;
            }
            reversed ^= bit;
            if (index < reversed)
                std::swap(output[index], output[reversed]);
        }
        for (std::size_t length = 2; length <= size; length <<= 1) {
            const float angle = -2.0f * std::numbers::pi_v<float> / static_cast<float>(length);
            const std::complex<float> step(std::cos(angle), std::sin(angle));
            for (std::size_t start = 0; start < size; start += length) {
                std::complex<float> phase(1.0f, 0.0f);
                for (std::size_t offset = 0; offset < length / 2; ++offset) {
                    const auto even = output[start + offset];
                    const auto odd = output[start + offset + length / 2] * phase;
                    output[start + offset] = even + odd;
                    output[start + offset + length / 2] = even - odd;
                    phase *= step;
                }
            }
        }
        return output;
    }

    Spectrogram stft_power(std::span<const float> input, std::size_t fft_size,
                           std::size_t hop_size) {
        if (hop_size == 0 || input.size() < fft_size)
            return {};
        const auto window = hann_window(fft_size);
        Spectrogram output;
        output.frames = 1 + (input.size() - fft_size) / hop_size;
        output.bins = fft_size / 2 + 1;
        output.values.resize(output.frames * output.bins);
        std::vector<float> frame(fft_size);
        for (std::size_t frame_index = 0; frame_index < output.frames; ++frame_index) {
            const std::size_t start = frame_index * hop_size;
            for (std::size_t sample = 0; sample < fft_size; ++sample)
                frame[sample] = input[start + sample] * window[sample];
            const auto spectrum = fft(frame);
            for (std::size_t bin = 0; bin < output.bins; ++bin)
                output.values[frame_index * output.bins + bin] = std::norm(spectrum[bin]);
        }
        return output;
    }

    namespace {
        float hz_to_mel(float hz) { return 2595.0f * std::log10(1.0f + hz / 700.0f); }
        float mel_to_hz(float mel) { return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f); }
    }

    std::vector<float> mel_filterbank(uint32_t sample_rate, std::size_t fft_size,
                                      std::size_t mel_bins, float minimum_hz, float maximum_hz) {
        if (sample_rate == 0 || fft_size == 0 || mel_bins == 0)
            throw std::invalid_argument("Mel filterbank dimensions must be positive");
        maximum_hz = maximum_hz > 0 ? maximum_hz : static_cast<float>(sample_rate) / 2.0f;
        const std::size_t spectrum_bins = fft_size / 2 + 1;
        std::vector<float> points(mel_bins + 2);
        const float minimum_mel = hz_to_mel(minimum_hz);
        const float maximum_mel = hz_to_mel(maximum_hz);
        for (std::size_t index = 0; index < points.size(); ++index)
            points[index] =
                mel_to_hz(minimum_mel + (maximum_mel - minimum_mel) * static_cast<float>(index) /
                                            static_cast<float>(mel_bins + 1));
        std::vector<float> output(mel_bins * spectrum_bins, 0.0f);
        for (std::size_t mel = 0; mel < mel_bins; ++mel) {
            for (std::size_t bin = 0; bin < spectrum_bins; ++bin) {
                const float hz = static_cast<float>(bin) * sample_rate / fft_size;
                float weight = 0.0f;
                if (hz >= points[mel] && hz <= points[mel + 1])
                    weight = (hz - points[mel]) / (points[mel + 1] - points[mel]);
                else if (hz > points[mel + 1] && hz <= points[mel + 2])
                    weight = (points[mel + 2] - hz) / (points[mel + 2] - points[mel + 1]);
                output[mel * spectrum_bins + bin] = std::max(0.0f, weight);
            }
        }
        return output;
    }

    Spectrogram log_mel(std::span<const float> input, uint32_t sample_rate, std::size_t fft_size,
                        std::size_t hop_size, std::size_t mel_bins, float floor) {
        const auto power = stft_power(input, fft_size, hop_size);
        const auto filters = mel_filterbank(sample_rate, fft_size, mel_bins);
        Spectrogram output{power.frames, mel_bins, std::vector<float>(power.frames * mel_bins)};
        for (std::size_t frame = 0; frame < power.frames; ++frame)
            for (std::size_t mel = 0; mel < mel_bins; ++mel) {
                double energy = 0.0;
                for (std::size_t bin = 0; bin < power.bins; ++bin)
                    energy +=
                        power.values[frame * power.bins + bin] * filters[mel * power.bins + bin];
                output.values[frame * mel_bins + mel] =
                    std::log(std::max(floor, static_cast<float>(energy)));
            }
        return output;
    }

    Spectrogram spectral_noise_gate(const Spectrogram &spectrum, float noise_quantile,
                                    float floor) {
        if (noise_quantile < 0.0f || noise_quantile > 1.0f || !std::isfinite(floor))
            throw std::invalid_argument("spectral noise gate parameters are invalid");
        if (spectrum.values.size() != spectrum.frames * spectrum.bins)
            throw std::invalid_argument("spectrogram dimensions do not match its values");
        Spectrogram output{spectrum.frames, spectrum.bins,
                           std::vector<float>(spectrum.values.size(), floor)};
        if (spectrum.frames == 0 || spectrum.bins == 0)
            return output;
        const std::size_t quantile_index = static_cast<std::size_t>(
            noise_quantile * static_cast<float>(spectrum.frames - 1));
        std::vector<float> values(spectrum.frames);
        for (std::size_t bin = 0; bin < spectrum.bins; ++bin) {
            for (std::size_t frame = 0; frame < spectrum.frames; ++frame)
                values[frame] = spectrum.values[frame * spectrum.bins + bin];
            std::nth_element(values.begin(), values.begin() + quantile_index, values.end());
            const float noise_floor = values[quantile_index];
            for (std::size_t frame = 0; frame < spectrum.frames; ++frame)
                output.values[frame * spectrum.bins + bin] = std::max(
                    floor, spectrum.values[frame * spectrum.bins + bin] - noise_floor);
        }
        return output;
    }

    float spectral_flux(const Spectrogram &spectrum) {
        if (spectrum.frames < 2 || spectrum.bins == 0)
            return 0.0f;
        double sum = 0.0;
        for (std::size_t frame = 1; frame < spectrum.frames; ++frame)
            for (std::size_t bin = 0; bin < spectrum.bins; ++bin) {
                const float delta = spectrum.values[frame * spectrum.bins + bin] -
                                    spectrum.values[(frame - 1) * spectrum.bins + bin];
                sum += std::max(0.0f, delta);
            }
        return static_cast<float>(sum / ((spectrum.frames - 1) * spectrum.bins));
    }

    float event_probability(float energy, float flux, float similarity, float class_margin,
                            const EventDetectorConfig &config) {
        if (config.rms_reference <= 0.0f || config.flux_reference <= 0.0f ||
            config.minimum_similarity < -1.0f || config.full_similarity > 1.0f ||
            config.minimum_similarity >= config.full_similarity || config.margin_reference <= 0.0f)
            throw std::invalid_argument("invalid audio event detector configuration");
        const float activity = std::clamp(std::max(0.0f, energy) / config.rms_reference +
                                              std::max(0.0f, flux) / config.flux_reference,
                                          0.0f, 1.0f);
        const float match = std::clamp((similarity - config.minimum_similarity) /
                                           (config.full_similarity - config.minimum_similarity),
                                       0.0f, 1.0f);
        const float separation =
            std::clamp(std::max(0.0f, class_margin) / config.margin_reference, 0.0f, 1.0f);
        return activity * match * separation;
    }

    std::vector<float> fingerprint(const Spectrogram &spectrum) {
        std::vector<float> output(spectrum.bins, 0.0f);
        if (spectrum.frames == 0)
            return output;
        for (std::size_t frame = 0; frame < spectrum.frames; ++frame)
            for (std::size_t bin = 0; bin < spectrum.bins; ++bin)
                output[bin] += spectrum.values[frame * spectrum.bins + bin] /
                               static_cast<float>(spectrum.frames);
        const float mean = [&] {
            double sum = 0.0;
            for (float value : output)
                sum += value;
            return static_cast<float>(sum / output.size());
        }();
        double norm = 0.0;
        for (float &value : output) {
            value -= mean;
            norm += static_cast<double>(value) * value;
        }
        norm = std::sqrt(norm);
        if (norm > std::numeric_limits<float>::epsilon())
            for (float &value : output)
                value = static_cast<float>(value / norm);
        return output;
    }

    std::vector<float> temporal_fingerprint(const Spectrogram &spectrum) {
        std::vector<float> output(spectrum.bins * 3, 0.0f);
        if (spectrum.frames == 0 || spectrum.bins == 0)
            return output;
        auto contrast = std::span<float>(output).first(spectrum.bins);
        auto deviations = std::span<float>(output).subspan(spectrum.bins, spectrum.bins);
        auto dynamics = std::span<float>(output).last(spectrum.bins);
        std::vector<float> means(spectrum.bins, 0.0f);
        std::vector<float> frame_energies(spectrum.frames, 0.0f);
        for (std::size_t frame = 0; frame < spectrum.frames; ++frame) {
            for (std::size_t bin = 0; bin < spectrum.bins; ++bin)
                frame_energies[frame] += spectrum.values[frame * spectrum.bins + bin] /
                                         static_cast<float>(spectrum.bins);
        }
        auto ranked_energies = frame_energies;
        const auto threshold_position = ranked_energies.begin() + ranked_energies.size() / 2;
        std::nth_element(ranked_energies.begin(), threshold_position, ranked_energies.end());
        const float activity_threshold = *threshold_position;
        const std::size_t active_frames =
            std::max<std::size_t>(1, std::count_if(frame_energies.begin(), frame_energies.end(),
                                                   [activity_threshold](float energy) {
                                                       return energy >= activity_threshold;
                                                   }));
        for (std::size_t frame = 0; frame < spectrum.frames; ++frame) {
            if (frame_energies[frame] < activity_threshold)
                continue;
            double frame_sum = 0.0;
            for (std::size_t bin = 0; bin < spectrum.bins; ++bin)
                frame_sum += spectrum.values[frame * spectrum.bins + bin];
            const float frame_mean = static_cast<float>(frame_sum / spectrum.bins);
            double frame_norm = 0.0;
            for (std::size_t bin = 0; bin < spectrum.bins; ++bin) {
                const float centered = spectrum.values[frame * spectrum.bins + bin] - frame_mean;
                frame_norm += static_cast<double>(centered) * centered;
            }
            frame_norm = std::sqrt(frame_norm);
            for (std::size_t bin = 0; bin < spectrum.bins; ++bin) {
                const float value = spectrum.values[frame * spectrum.bins + bin];
                means[bin] += value / static_cast<float>(active_frames);
                if (frame_norm > std::numeric_limits<float>::epsilon())
                    contrast[bin] += static_cast<float>((value - frame_mean) / frame_norm) /
                                     static_cast<float>(active_frames);
            }
        }
        for (std::size_t frame = 0; frame < spectrum.frames; ++frame)
            for (std::size_t bin = 0; bin < spectrum.bins; ++bin) {
                if (frame_energies[frame] < activity_threshold)
                    continue;
                const float centered = spectrum.values[frame * spectrum.bins + bin] - means[bin];
                deviations[bin] += centered * centered / static_cast<float>(active_frames);
                if (frame != 0 && frame_energies[frame - 1] >= activity_threshold)
                    dynamics[bin] += std::abs(spectrum.values[frame * spectrum.bins + bin] -
                                              spectrum.values[(frame - 1) * spectrum.bins + bin]) /
                                     static_cast<float>(active_frames);
            }
        for (float &value : deviations)
            value = std::sqrt(value);
        for (std::size_t block = 0; block < 3; ++block) {
            auto values = std::span<float>(output).subspan(block * spectrum.bins, spectrum.bins);
            double sum = 0.0;
            for (float value : values)
                sum += value;
            const float mean = static_cast<float>(sum / values.size());
            double norm = 0.0;
            for (float &value : values) {
                value -= mean;
                norm += static_cast<double>(value) * value;
            }
            norm = std::sqrt(norm);
            if (norm > std::numeric_limits<float>::epsilon())
                for (float &value : values)
                    value = static_cast<float>(value / norm);
        }
        const float combined_norm = std::sqrt(3.0f);
        for (float &value : output)
            value /= combined_norm;
        return output;
    }

    float cosine_similarity(std::span<const float> left, std::span<const float> right) {
        if (left.size() != right.size() || left.empty())
            throw std::invalid_argument("fingerprints must have equal non-zero dimensions");
        double dot = 0.0, left_norm = 0.0, right_norm = 0.0;
        for (std::size_t index = 0; index < left.size(); ++index) {
            dot += static_cast<double>(left[index]) * right[index];
            left_norm += static_cast<double>(left[index]) * left[index];
            right_norm += static_cast<double>(right[index]) * right[index];
        }
        const double denominator = std::sqrt(left_norm * right_norm);
        return denominator > 0.0 ? static_cast<float>(dot / denominator) : 0.0f;
    }

    std::vector<float> similarity(std::span<const float> query, std::span<const float> references,
                                  std::size_t reference_count) {
        if (reference_count == 0 || references.size() != query.size() * reference_count)
            throw std::invalid_argument("reference fingerprint bank has invalid dimensions");
        std::vector<float> scores(reference_count);
        for (std::size_t index = 0; index < reference_count; ++index)
            scores[index] =
                cosine_similarity(query, references.subspan(index * query.size(), query.size()));
        return scores;
    }

    std::vector<float> prototype_cnn_logits(std::span<const float> query,
                                            std::span<const float> prototypes,
                                            std::size_t class_count) {
        if (query.empty() || class_count == 0 || prototypes.size() != query.size() * class_count)
            throw std::invalid_argument("CNN prototype bank has invalid dimensions");
        std::vector<float> positive(query.size());
        std::vector<float> negative(query.size());
        for (std::size_t index = 0; index < query.size(); ++index) {
            positive[index] = std::max(0.0f, query[index]);
            negative[index] = std::max(0.0f, -query[index]);
        }
        std::vector<float> hidden_positive(positive.size());
        std::vector<float> hidden_negative(negative.size());
        std::transform(positive.begin(), positive.end(), hidden_positive.begin(),
                       [](float value) { return std::max(0.0f, value); });
        std::transform(negative.begin(), negative.end(), hidden_negative.begin(),
                       [](float value) { return std::max(0.0f, value); });
        std::vector<float> logits(class_count, 0.0f);
        for (std::size_t class_index = 0; class_index < class_count; ++class_index)
            for (std::size_t feature_index = 0; feature_index < query.size(); ++feature_index) {
                const float weight = prototypes[class_index * query.size() + feature_index];
                logits[class_index] += weight * hidden_positive[feature_index] -
                                       weight * hidden_negative[feature_index];
            }
        return logits;
    }

    std::vector<float> softmax(std::span<const float> logits, float temperature) {
        if (logits.empty() || temperature <= 0.0f)
            throw std::invalid_argument("softmax requires logits and a positive temperature");
        const float maximum = *std::max_element(logits.begin(), logits.end());
        std::vector<float> output(logits.size());
        double denominator = 0.0;
        for (std::size_t index = 0; index < logits.size(); ++index) {
            output[index] = std::exp((logits[index] - maximum) * temperature);
            denominator += output[index];
        }
        for (float &value : output)
            value = static_cast<float>(value / denominator);
        return output;
    }

    std::vector<float> augment(std::span<const float> signal, float gain,
                               std::span<const float> background, float background_gain,
                               int64_t shift, float clip, bool normalize) {
        if (clip <= 0.0f)
            throw std::invalid_argument("clip must be positive");
        std::vector<float> output(signal.size());
        const int64_t size = static_cast<int64_t>(signal.size());
        for (int64_t index = 0; index < size; ++index) {
            const int64_t source = ((index - shift) % size + size) % size;
            float value = signal[static_cast<std::size_t>(source)] * gain;
            if (!background.empty())
                value += background[static_cast<std::size_t>(index) % background.size()] *
                         background_gain;
            output[static_cast<std::size_t>(index)] = std::clamp(value, -clip, clip);
        }
        if (normalize) {
            float peak = 0.0f;
            for (float value : output)
                peak = std::max(peak, std::abs(value));
            if (peak > 0.0f)
                for (float &value : output)
                    value /= peak;
        }
        return output;
    }
}
