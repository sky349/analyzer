#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iterator>
#include <limits>
#include <map>
#include <numeric>
#include <tuple>
#include <utility>
#include <vector>

namespace Prp3RadialDiagnostics {
constexpr double UNAVAILABLE = std::numeric_limits<double>::quiet_NaN();
constexpr double RADIAL_GATE_M = 200.0;
constexpr double ANGULAR_GATE_RAD = 0.35 * 3.14159265358979323846 / 180.0;
constexpr double MAX_SPEED_MPS = 350.0;

struct Point
{
    int row = 0;
    int source = 0;
    std::int64_t order = 0;
    int scan = 0;
    double time = 0.0;
    double range = 0.0;
    double cosAzimuth = 1.0;
    double sinAzimuth = 0.0;
};

struct Estimate
{
    int chain = -1;
    int samples = 0;
    double radialMps = UNAVAILABLE;
    double standardErrorMps = UNAVAILABLE;
    double spanSeconds = 0.0;
};

struct Chain
{
    std::vector<int> rows;
};

// Geometry and ingress time only: no Doppler, phase, amplitude or restoration validity enters association.
// advance() evaluates one hypothesis/extraction at a time for the comparison window's GUI timer batches.
class Analysis
{
public:
    Analysis() = default;
    Analysis(std::vector<Point> points, int rowCount)
        : m_points(std::move(points)), m_estimates(rowCount), m_available(m_points.size(), true)
    {
        std::stable_sort(m_points.begin(), m_points.end(), [](const auto &a, const auto &b) {
            return std::tie(a.source, a.order, a.row) < std::tie(b.source, b.order, b.row);
        });
        int partition = -1;
        for (size_t index = 0; index < m_points.size(); ++index) {
            const auto &point = m_points[index];
            if (!index || point.source != m_points[index - 1].source
                || point.scan < m_points[index - 1].scan || point.time < m_points[index - 1].time) {
                m_partitions.emplace_back();
                ++partition;
            }
            m_partitions[partition][point.scan].push_back(index);
        }
        beginPartition();
    }

    bool advance()
    {
        if (m_partition >= m_partitions.size())
            return false;
        const auto &scans = m_partitions[m_partition];
        if (m_start < m_starts.size()) {
            const auto start = m_starts[m_start];
            const auto &first = scans.at(start), &last = scans.at(start + 6);
            const auto i = first[m_first], j = last[m_last];
            const auto &a = m_points[i], &b = m_points[j];
            const auto dt = b.time - a.time;
            if (dt > 0.0) {
                const auto model = Model { a.time, x(a), y(a), (x(b) - x(a)) / dt, (y(b) - y(a)) / dt };
                const auto middle = scans.find(start + 3);
                if (std::hypot(model.vx, model.vy) <= MAX_SPEED_MPS
                    && std::any_of(middle->second.cbegin(), middle->second.cend(), [this, &model](size_t id) {
                           return distance(id, model) <= 1.0;
                       })) {
                    const auto candidate = fitSelect(model);
                    if (eligible(candidate))
                        m_candidates.emplace(candidate.ids, candidate);
                }
            }
            if (++m_last == last.size()) {
                m_last = 0;
                if (++m_first == first.size()) {
                    m_first = 0;
                    ++m_start;
                }
            }
            return true;
        }
        if (!m_extracting) {
            m_ordered.reserve(m_candidates.size());
            std::transform(m_candidates.cbegin(), m_candidates.cend(), std::back_inserter(m_ordered),
                           [](const auto &entry) { return entry.second; });
            m_candidates.clear();
            std::stable_sort(m_ordered.begin(), m_ordered.end(), [](const auto &a, const auto &b) {
                return a.ids.size() != b.ids.size() ? a.ids.size() > b.ids.size() : a.error < b.error;
            });
            m_extracting = true;
        }
        if (m_candidate < m_ordered.size()) {
            const auto selected = fitSelect(m_ordered[m_candidate++].model);
            if (eligible(selected))
                retain(selected);
            return true;
        }
        ++m_partition;
        beginPartition();
        return m_partition < m_partitions.size();
    }

    const std::vector<Estimate> &estimates() const { return m_estimates; }
    const std::vector<Chain> &chains() const { return m_chains; }

private:
    struct Model
    {
        double time = 0.0;
        double x = 0.0;
        double y = 0.0;
        double vx = 0.0;
        double vy = 0.0;
    };
    struct Candidate
    {
        std::vector<size_t> ids;
        Model model;
        double error = 0.0;
    };

    static double x(const Point &point) { return point.range * point.cosAzimuth; }
    static double y(const Point &point) { return point.range * point.sinAzimuth; }

    double distance(size_t index, const Model &model) const
    {
        const auto &point = m_points[index];
        const auto dx = x(point) - model.x - (point.time - model.time) * model.vx;
        const auto dy = y(point) - model.y - (point.time - model.time) * model.vy;
        const auto radial = (dx * point.cosAzimuth + dy * point.sinAzimuth) / RADIAL_GATE_M;
        const auto tangent = (dx * point.sinAzimuth - dy * point.cosAzimuth)
            / std::max(100.0, point.range * ANGULAR_GATE_RAD);
        return radial * radial + tangent * tangent;
    }

    std::vector<size_t> select(const Model &model) const
    {
        std::vector<size_t> ids;
        for (const auto &scan : m_partitions[m_partition]) {
            const auto score = [this, &model](size_t index) {
                return m_available[index] ? distance(index, model) : std::numeric_limits<double>::infinity();
            };
            const auto best = std::min_element(scan.second.cbegin(), scan.second.cend(),
                [&score](size_t a, size_t b) { return score(a) < score(b); });
            if (score(*best) <= 1.0)
                ids.push_back(*best);
        }
        return ids;
    }

    Model fit(const std::vector<size_t> &ids) const
    {
        const auto origin = m_points[ids.front()].time;
        const auto sum = std::accumulate(ids.cbegin(), ids.cend(), Model{}, [this, origin](auto result, size_t id) {
            result.time += m_points[id].time - origin;
            result.x += x(m_points[id]);
            result.y += y(m_points[id]);
            return result;
        });
        Model model { origin + sum.time / ids.size(), sum.x / ids.size(), sum.y / ids.size(), 0.0, 0.0 };
        const auto variance = std::accumulate(ids.cbegin(), ids.cend(), 0.0, [this, &model](double value, size_t id) {
            const auto dt = m_points[id].time - model.time;
            model.vx += dt * (x(m_points[id]) - model.x);
            model.vy += dt * (y(m_points[id]) - model.y);
            return value + dt * dt;
        });
        model.vx = variance > 0.0 ? model.vx / variance : UNAVAILABLE;
        model.vy = variance > 0.0 ? model.vy / variance : UNAVAILABLE;
        return model;
    }

    Candidate fitSelect(Model model) const
    {
        std::vector<size_t> ids;
        for (int iteration = 0; iteration < 5; ++iteration) {
            const auto next = select(model);
            if (next.size() < 8)
                return {};
            if (next == ids)
                break;
            ids = next;
            model = fit(ids);
            if (!std::isfinite(model.vx) || !std::isfinite(model.vy))
                return {};
        }
        if (select(model) != ids)
            return {};
        const auto error = std::accumulate(ids.cbegin(), ids.cend(), 0.0,
            [this, &model](double value, size_t id) { return value + distance(id, model); }) / ids.size();
        return { std::move(ids), model, error };
    }

    bool eligible(const Candidate &candidate) const
    {
        if (candidate.ids.size() < 8 || std::hypot(candidate.model.vx, candidate.model.vy) > MAX_SPEED_MPS)
            return false;
        const auto &first = m_points[candidate.ids.front()], &last = m_points[candidate.ids.back()];
        return last.time - first.time >= 35.0
            && double(candidate.ids.size()) / (last.scan - first.scan + 1) >= 0.6
            && std::adjacent_find(candidate.ids.cbegin(), candidate.ids.cend(), [this](size_t a, size_t b) {
                   return m_points[a].time >= m_points[b].time;
               }) == candidate.ids.cend();
    }

    void retain(const Candidate &candidate)
    {
        Chain chain;
        for (const auto id : candidate.ids) {
            auto window = candidate.ids;
            std::stable_sort(window.begin(), window.end(), [this, id](size_t a, size_t b) {
                return std::abs(m_points[a].time - m_points[id].time)
                    < std::abs(m_points[b].time - m_points[id].time);
            });
            window.resize(std::min(size_t(9), window.size()));
            const auto origin = m_points[id].time;
            const auto mean = std::accumulate(window.cbegin(), window.cend(), std::pair<double, double>{},
                [this, origin, count = window.size()](auto value, size_t index) {
                    return std::make_pair(value.first + (m_points[index].time - origin) / count,
                                          value.second + m_points[index].range / count);
                });
            const auto sums = std::accumulate(window.cbegin(), window.cend(), std::pair<double, double>{},
                [this, origin, mean](auto value, size_t index) {
                    const auto dt = m_points[index].time - origin - mean.first;
                    return std::make_pair(value.first + dt * dt,
                                          value.second + dt * (m_points[index].range - mean.second));
                });
            const auto slope = sums.second / sums.first;
            const auto squaredError = std::accumulate(window.cbegin(), window.cend(), 0.0,
                [this, origin, mean, slope](double value, size_t index) {
                    const auto residual = m_points[index].range - mean.second
                        - slope * (m_points[index].time - origin - mean.first);
                    return value + residual * residual;
                });
            const auto ends = std::minmax_element(window.cbegin(), window.cend(), [this](size_t a, size_t b) {
                return m_points[a].time < m_points[b].time;
            });
            m_estimates[m_points[id].row] = { int(m_chains.size()), int(window.size()), slope,
                std::sqrt(squaredError / (window.size() - 2) / sums.first),
                m_points[*ends.second].time - m_points[*ends.first].time };
            chain.rows.push_back(m_points[id].row);
            m_available[id] = false;
        }
        m_chains.push_back(std::move(chain));
    }

    void beginPartition()
    {
        m_starts.clear();
        m_candidates.clear();
        m_ordered.clear();
        m_start = m_first = m_last = m_candidate = 0;
        m_extracting = false;
        if (m_partition >= m_partitions.size())
            return;
        const auto &scans = m_partitions[m_partition];
        size_t offset = 0;
        for (const auto &scan : scans) {
            if (offset++ % 3 == 0 && scans.count(scan.first + 3) && scans.count(scan.first + 6))
                m_starts.push_back(scan.first);
        }
    }

    std::vector<Point> m_points;
    std::vector<Estimate> m_estimates;
    std::vector<bool> m_available;
    std::vector<std::map<int, std::vector<size_t>>> m_partitions;
    std::vector<int> m_starts;
    std::map<std::vector<size_t>, Candidate> m_candidates;
    std::vector<Candidate> m_ordered;
    std::vector<Chain> m_chains;
    size_t m_partition = 0;
    size_t m_start = 0;
    size_t m_first = 0;
    size_t m_last = 0;
    size_t m_candidate = 0;
    bool m_extracting = false;
};
}
