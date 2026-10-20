#include "mln/audio/dsp.hpp"

#include <array>
#include <cmath>
#include <numbers>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace {
    bool near(float left, float right, float tolerance = 1.0e-4f) {
        return std::abs(left - right) <= tolerance;
    }

    void require(bool condition) {
        if (!condition)
            throw std::runtime_error("audio numeric equivalence check failed");
    }

    std::vector<float> tone(std::size_t size, float cycles) {
        std::vector<float> output(size);
        for (std::size_t index = 0; index < size; ++index)
            output[index] = std::sin(2.0f * std::numbers::pi_v<float> * cycles * index / size);
        return output;
    }
}

int main(int argc, char **argv) {
    require(argc == 2);
    const std::string_view test = argv[1];
    if (test == "window") {
        const auto value = Mln::Audio::hann_window(5);
        require(near(value[0], 0) && near(value[1], .5f) && near(value[2], 1) && near(value[4], 0));
    } else if (test == "phase_safe") {
        const std::array<float, 6> stereo{.5f, -.5f, .1f, -.1f, -.8f, .8f};
        const auto mono = Mln::Audio::mix_to_mono_phase_safe(stereo, 2);
        require(near(mono[0], .5f) && near(mono[1], .1f) && near(mono[2], -.8f));
        require(Mln::Audio::mix_to_mono_phase_safe(std::array<float, 2>{.5f, .5f}, 2)[0] == .5f);
        require(Mln::Audio::mix_to_mono_phase_safe(stereo, 1) == std::vector<float>(stereo.begin(), stereo.end()));
    } else if (test == "resample_bandlimited") {
        for (const uint32_t rate : {44'100, 48'000, 96'000}) {
            const auto low = Mln::Audio::resample_bandlimited(tone(rate, 1000), rate, 16'000);
            const auto high = Mln::Audio::resample_bandlimited(tone(rate, 12000), rate, 16'000);
            require(low.size() == 16'000 && high.size() == 16'000);
            require(near(Mln::Audio::rms(std::span(low).subspan(100, 15800)), std::sqrt(.5f), .002f));
            require(Mln::Audio::rms(std::span(high).subspan(100, 15800)) < .001f);
            const auto constant = Mln::Audio::resample_bandlimited(std::vector<float>(rate, .3f), rate, 16'000);
            for (float value : constant) require(near(value, .3f));
        }
    } else if (test == "rms") {
        const std::array<float, 4> value{1, -1, 1, -1};
        require(near(Mln::Audio::rms(value), 1));
    } else if (test == "stft" || test == "magnitude") {
        const auto value = Mln::Audio::stft_power(tone(16, 2), 8, 4);
        require(value.frames == 3 && value.bins == 5 && value.values.size() == 15);
        for (float sample : value.values)
            require(sample >= 0 && std::isfinite(sample));
    } else if (test == "mel_filterbank") {
        const auto value = Mln::Audio::mel_filterbank(16'000, 16, 4);
        require(value.size() == 4 * 9);
        for (float weight : value)
            require(weight >= 0 && weight <= 1);
    } else if (test == "logmel") {
        const auto value = Mln::Audio::log_mel(tone(32, 4), 16'000, 16, 8, 4);
        require(value.frames == 3 && value.bins == 4);
        for (float sample : value.values)
            require(std::isfinite(sample));
    } else if (test == "spectral_flux") {
        Mln::Audio::Spectrogram value{2, 2, {1, 3, 2, 1}};
        require(near(Mln::Audio::spectral_flux(value), .5f));
    } else if (test == "noise_gate") {
        Mln::Audio::Spectrogram value{4, 2, {1, 10, 2, 10, 5, 14, 3, 12}};
        const auto gated = Mln::Audio::spectral_noise_gate(value, .34f);
        const std::array<float, 8> expected{0, 0, 0, 0, 3, 4, 1, 2};
        require(gated.frames == 4 && gated.bins == 2 && gated.values.size() == expected.size());
        for (std::size_t index = 0; index < expected.size(); ++index)
            require(near(gated.values[index], expected[index]));
    } else if (test == "event_detector") {
        require(near(Mln::Audio::event_probability(.02f, 0, 1, .03f), 1));
        require(near(Mln::Audio::event_probability(.02f, 0, .8f, .03f), 0));
        require(near(Mln::Audio::event_probability(.02f, 0, 1, 0), 0));
        require(near(Mln::Audio::event_probability(0, 0, 1, .03f), 0));
    } else if (test == "fingerprint") {
        Mln::Audio::Spectrogram value{2, 3, {1, 2, 3, 1, 2, 3}};
        const auto fingerprint = Mln::Audio::fingerprint(value);
        require(fingerprint.size() == 3 &&
                near(Mln::Audio::cosine_similarity(fingerprint, fingerprint), 1));
        const auto temporal = Mln::Audio::temporal_fingerprint(value);
        require(temporal.size() == 9 && near(Mln::Audio::cosine_similarity(temporal, temporal), 1));
    } else if (test == "classifier") {
        const std::array<float, 2> query{1, 0};
        const std::array<float, 4> references{1, 0, 0, 1};
        const auto scores = Mln::Audio::similarity(query, references, 2);
        require(near(scores[0], 1) && near(scores[1], 0));
        const auto logits = Mln::Audio::prototype_cnn_logits(query, references, 2);
        const auto probabilities = Mln::Audio::softmax(logits, 8.0f);
        require(near(logits[0], 1) && near(logits[1], 0));
        require(probabilities[0] > .99f && probabilities[1] < .01f);
    } else if (test == "augmentation") {
        const std::array<float, 3> signal{.25f, .5f, 1};
        const auto output = Mln::Audio::augment(signal, 2, {}, 0, 1, 1, false);
        require(near(output[0], 1) && near(output[1], .5f) && near(output[2], 1));
    } else {
        require(false);
    }
}
