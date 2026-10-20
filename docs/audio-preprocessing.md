# Native audio preprocessing

`Mln::Audio::resample_bandlimited` performs sample-rate conversion with a
normalized Blackman-windowed sinc kernel. The cutoff tracks the lower Nyquist
frequency with a transition band; rational phases are reused within each call.
Use it before audio features when reducing sample rate. `resample_linear`
remains available for callers that intentionally need linear interpolation.

The output length is rounded to preserve duration. Boundary samples are extended
with the nearest sample. Rates must be positive; excessively large ratios and
phase tables are rejected rather than allocating unbounded kernel memory.

`Mln::Audio::mix_to_mono_phase_safe` normally averages channels. If cancellation
reduces the mixed energy below one sixteenth of the strongest channel's energy,
it preserves that channel instead. This is useful for recognition with spatial
audio and inverted stereo channels. It is not intended as a multichannel playback
downmix or as a replacement for channel-layout-aware routing.

Tests `mln_audio_resample_bandlimited` and `mln_audio_phase_safe` cover passband
amplitude, suppression of 12 kHz before conversion to 16 kHz, DC preservation,
44.1/48/96 kHz input, identical channels and inverted stereo.

All operations execute natively. No Python feature preprocessing is required.
