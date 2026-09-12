#pragma once

#include <libradardata/psrdopplersnapshot.h>

#include <algorithm>
#include <cmath>
#include <complex>
#include <iterator>
#include <limits>
#include <numeric>
#include <set>
#include <tuple>
#include <vector>

namespace Prp3PhaseDiagnostics {
constexpr double PI = 3.14159265358979323846;
constexpr double RAW_FREQUENCY_LIMIT_HZ = 20000.0;
constexpr double COMPETITIVE_RMS_DEG = 1.0;
constexpr double MIN_SIGNAL_AMPLITUDE = 40.0;
constexpr double RELATIVE_SIGNAL_AMPLITUDE = 0.2;
constexpr int CPI_TRANSITIONS = 3;
constexpr int CPI_COUNT = (PsrDoppler::SAMPLE_COUNT - 1) / CPI_TRANSITIONS;

struct PhasePair
{
    int index = 0;
    double phaseDeg = 0.0;
    double weight = 0.0;
    quint16 intervalUs = 0;
};

struct PhaseFit
{
    double frequencyHz = 0.0;
    double rmsDeg = 0.0;
};

inline double phaseResidual(const PhasePair &pair, double frequencyHz)
{
    return std::remainder(pair.phaseDeg - 360.0 * frequencyHz * pair.intervalUs / 1e6, 360.0);
}

inline double phaseRms(const std::vector<PhasePair> &pairs, double frequencyHz)
{
    const auto sums = std::accumulate(pairs.cbegin(), pairs.cend(), std::pair<double, double>{},
                                     [frequencyHz](auto sum, const auto &pair) {
        const auto residual = phaseResidual(pair, frequencyHz);
        return std::make_pair(sum.first + pair.weight * residual * residual, sum.second + pair.weight);
    });
    return sums.second > 0.0 ? std::sqrt(sums.first / sums.second) : std::numeric_limits<double>::quiet_NaN();
}

inline std::vector<PhaseFit> fitPhases(const std::vector<PhasePair> &pairs)
{
    if (pairs.empty())
        return {};

    // Wrapped least squares is quadratic between wrap boundaries. Search every segment, without alias priors.
    std::vector<double> boundaries { -RAW_FREQUENCY_LIMIT_HZ, RAW_FREQUENCY_LIMIT_HZ };
    for (const auto &pair : pairs) {
        const auto phase = pair.phaseDeg / 360.0, time = pair.intervalUs / 1e6;
        for (auto k = int(std::floor(phase - RAW_FREQUENCY_LIMIT_HZ * time - 0.5));
             k <= int(std::ceil(phase + RAW_FREQUENCY_LIMIT_HZ * time - 0.5)); ++k) {
            const auto frequency = (phase - k - 0.5) / time;
            if (std::abs(frequency) < RAW_FREQUENCY_LIMIT_HZ)
                boundaries.push_back(frequency);
        }
    }
    std::sort(boundaries.begin(), boundaries.end());
    boundaries.erase(std::unique(boundaries.begin(), boundaries.end()), boundaries.end());
    const auto denominator = std::accumulate(pairs.cbegin(), pairs.cend(), 0.0, [](double sum, const auto &pair) {
        const auto time = pair.intervalUs / 1e6;
        return sum + pair.weight * time * time;
    });
    std::vector<PhaseFit> candidates;
    const auto append = [&pairs, &candidates](double frequency) {
        candidates.push_back({ frequency, phaseRms(pairs, frequency) });
    };
    append(-RAW_FREQUENCY_LIMIT_HZ);
    append(RAW_FREQUENCY_LIMIT_HZ);
    for (auto it = boundaries.cbegin(); it + 1 != boundaries.cend(); ++it) {
        const auto midpoint = (*it + *(it + 1)) / 2.0;
        const auto frequency = std::accumulate(pairs.cbegin(), pairs.cend(), 0.0,
                                              [midpoint](double sum, const auto &pair) {
            const auto phase = pair.phaseDeg / 360.0, time = pair.intervalUs / 1e6;
            return sum + pair.weight * time * (phase - std::round(phase - midpoint * time));
        }) / denominator;
        if (frequency >= *it && frequency <= *(it + 1))
            append(frequency);
    }
    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        return a.frequencyHz < b.frequencyHz;
    });
    candidates.erase(std::unique(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        return std::abs(a.frequencyHz - b.frequencyHz) < 1e-7;
    }), candidates.end());
    std::sort(candidates.begin(), candidates.end(), [](const auto &a, const auto &b) {
        return std::tie(a.rmsDeg, a.frequencyHz) < std::tie(b.rmsDeg, b.frequencyHz);
    });
    return candidates;
}

struct PhaseEvidence
{
    std::array<double, PsrDoppler::SAMPLE_COUNT> amplitudes {};
    std::array<bool, PsrDoppler::SAMPLE_COUNT> valid {};
    std::array<bool, PsrDoppler::SAMPLE_COUNT> supported {};
    std::array<PhasePair, PsrDoppler::SAMPLE_COUNT - 1> transitions {};
    std::vector<PhasePair> pairs;
    double threshold = MIN_SIGNAL_AMPLITUDE;
};

inline PhaseEvidence phaseEvidence(const PsrDoppler::BranchSnapshot &branch, int intervalOffset)
{
    PhaseEvidence result;
    const auto &samples = branch.samples;
    std::transform(samples.cbegin(), samples.cend(), result.amplitudes.begin(), [](const auto &sample) {
        return std::hypot(double(sample.i), double(sample.q));
    });
    std::transform(samples.cbegin(), samples.cend(), result.valid.begin(), [&samples, &branch](const auto &sample) {
        return (branch.validMask & (1U << (&sample - samples.data())))
            && std::isfinite(sample.i) && std::isfinite(sample.q);
    });
    const auto peak = std::inner_product(result.amplitudes.cbegin(), result.amplitudes.cend(), result.valid.cbegin(),
                                        0.0, [](double a, double b) { return std::max(a, b); },
                                        [](double amplitude, bool valid) { return valid ? amplitude : 0.0; });
    result.threshold = std::max(MIN_SIGNAL_AMPLITUDE, RELATIVE_SIGNAL_AMPLITUDE * peak);
    std::transform(result.amplitudes.cbegin(), result.amplitudes.cend(), result.valid.cbegin(),
                   result.supported.begin(), [&result](double amplitude, bool valid) {
        return valid && amplitude >= result.threshold;
    });
    std::transform(samples.cbegin(), samples.cend() - 1, samples.cbegin() + 1, result.transitions.begin(),
                   [&samples, &result, intervalOffset](const auto &sample, const auto &next) {
        const auto n = int(&sample - samples.data());
        const auto product = std::complex<double>(next.i, next.q)
            * std::conj(std::complex<double>(sample.i, sample.q));
        return PhasePair { n, result.valid[n] && result.valid[n + 1] && std::abs(product) > 0.0
                ? std::arg(product) * 180.0 / PI : std::numeric_limits<double>::quiet_NaN(),
            result.amplitudes[n] * result.amplitudes[n + 1], samples[n + intervalOffset].followingIntervalUs };
    });
    std::copy_if(result.transitions.cbegin(), result.transitions.cend(), std::back_inserter(result.pairs),
                 [&result](const auto &pair) {
        return result.supported[pair.index] && result.supported[pair.index + 1] && pair.intervalUs;
    });
    return result;
}

inline int intervalCount(const std::vector<PhasePair> &pairs)
{
    std::set<quint16> intervals;
    std::transform(pairs.cbegin(), pairs.cend(), std::inserter(intervals, intervals.end()),
                   [](const auto &pair) { return pair.intervalUs; });
    return int(intervals.size());
}

}
