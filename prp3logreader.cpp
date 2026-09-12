#include "prp3logreader.h"

#include <QCborArray>
#include <QCborMap>
#include <QCborStreamReader>
#include <QCborValue>
#include <QFile>
#include <QFileInfo>
#include <QtEndian>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <utility>

namespace
{
constexpr int FILE_HEADER_SIZE = 16;
constexpr int RECORD_ENVELOPE_SIZE = 10;
constexpr quint16 FILE_FORMAT_VERSION = 1;
constexpr quint32 MAX_DECLARED_FILE_BYTES = 10000000;
constexpr quint64 SELF_DESCRIBED_CBOR_TAG = 55799;
constexpr quint16 RESTORATION_RESULT_VALID = 0x0001;

const char *const DECODE_ERROR_NAMES[] = { "none",          "not_present",       "truncated",
                                          "bad_magic",     "bad_length",        "bad_version",
                                          "bad_header_length", "bad_enum",      "bad_branch_count",
                                          "bad_branch_length", "bad_sample_count", "bad_validity_mask",
                                          "bad_timing",    "bad_source",        "duplicate_branch",
                                          "illegal_branch_set", "nonfinite_sample", "bad_crc" };
const char *const RESTORATION_REASON_NAMES[] = { "valid", "disabled", "no_snapshot", "snapshot_invalid",
                                                "incomplete_branch", "zero_energy",
                                                "insufficient_timing_diversity", "no_hypothesis",
                                                "degraded_single_branch", "ambiguous_hypothesis" };
const char *const SNAPSHOT_REASON_NAMES[] = { "none", "missing_a", "missing_b" };
const char *const SIGNAL_NAMES[] = { "mh", "nfm" };
const char *const BIN_NAMES[] = { "upper", "lower" };
const char *const MODE_NAMES[] = { "a", "b", "diversity" };
const char *const BRANCH_NAMES[] = { "a", "b" };

template<typename T, std::size_t N>
constexpr int arrayCount(const T (&)[N])
{
    return int(N);
}

bool reject(QString &error, const QString &scope, const char *key, const QString &expectation)
{
    error = QStringLiteral("%1.%2 %3").arg(scope, QLatin1String(key), expectation);
    return false;
}

bool cborMap(const QCborMap &map, const char *key, QCborMap &result, const QString &scope, QString &error)
{
    const auto value = map.value(QLatin1String(key));
    if (!value.isMap())
        return reject(error, scope, key, QStringLiteral("must be a map"));
    result = value.toMap();
    return true;
}

bool cborArray(const QCborMap &map, const char *key, QCborArray &result, const QString &scope, QString &error)
{
    const auto value = map.value(QLatin1String(key));
    if (!value.isArray())
        return reject(error, scope, key, QStringLiteral("must be an array"));
    result = value.toArray();
    return true;
}

bool cborInteger(const QCborMap &map, const char *key, qint64 &result, const QString &scope, QString &error)
{
    const auto value = map.value(QLatin1String(key));
    if (!value.isInteger())
        return reject(error, scope, key, QStringLiteral("must be an integer"));
    result = value.toInteger();
    return true;
}

template<typename T>
bool cborUnsigned(const QCborMap &map, const char *key, T &result, quint64 maximum, const QString &scope,
                  QString &error)
{
    qint64 value = 0;
    if (!cborInteger(map, key, value, scope, error))
        return false;
    if (value < 0 || quint64(value) > maximum)
        return reject(error, scope, key, QStringLiteral("is outside its supported unsigned range"));
    result = static_cast<T>(value);
    return true;
}

bool cborNumber(const QCborMap &map, const char *key, double &result, const QString &scope, QString &error)
{
    const auto value = map.value(QLatin1String(key));
    if (!value.isDouble())
        return reject(error, scope, key, QStringLiteral("must be a real number"));
    result = value.toDouble();
    return std::isfinite(result) ? true
                                 : reject(error, scope, key, QStringLiteral("must be finite"));
}

bool cborBool(const QCborMap &map, const char *key, bool &result, const QString &scope, QString &error)
{
    const auto value = map.value(QLatin1String(key));
    if (!value.isBool())
        return reject(error, scope, key, QStringLiteral("must be boolean"));
    result = value.toBool();
    return true;
}

bool cborString(const QCborMap &map, const char *key, QString &result, const QString &scope, QString &error)
{
    const auto value = map.value(QLatin1String(key));
    if (!value.isString())
        return reject(error, scope, key, QStringLiteral("must be text"));
    result = value.toString();
    return true;
}

bool cborBytes(const QCborMap &map, const char *key, QByteArray &result, const QString &scope, QString &error)
{
    const auto value = map.value(QLatin1String(key));
    if (!value.isByteArray())
        return reject(error, scope, key, QStringLiteral("must be a byte string"));
    result = value.toByteArray();
    return true;
}

bool expectedText(const QString &actual, const char *const *names, int count, quint8 value, QString &error,
                  const QString &scope, const char *key)
{
    return value < count && actual == QLatin1String(names[value])
        ? true
        : reject(error, scope, key, QStringLiteral("does not match its numeric value"));
}

bool numericallyEqual(double left, double right, double tolerance = 1e-6)
{
    return std::fabs(left - right) <= tolerance * std::max(1.0, std::max(std::fabs(left), std::fabs(right)));
}

bool parseTimestamp(const QCborMap &parent, const char *key, qint64 &utcMs, QString &utcIso, const QString &scope,
                    QString &error)
{
    QCborMap timestamp;
    const auto timestampScope = scope + QLatin1Char('.') + QLatin1String(key);
    if (!cborMap(parent, key, timestamp, scope, error)
        || !cborInteger(timestamp, "utc_ms", utcMs, timestampScope, error)
        || !cborString(timestamp, "utc_iso", utcIso, timestampScope, error))
        return false;

    const auto parsed = QDateTime::fromString(utcIso, Qt::ISODateWithMs);
    return parsed.isValid() && parsed.toUTC().toMSecsSinceEpoch() == utcMs
        ? true
        : reject(error, timestampScope, "utc_iso", QStringLiteral("does not represent utc_ms"));
}

bool parseCommon(const QCborMap &root, QString &recordType, QString &producerBuild, QString &error)
{
    const QString scope = QStringLiteral("root");
    QString schema;
    qint64 schemaVersion = 0;
    QCborMap producer;
    QString application;
    if (!cborString(root, "schema", schema, scope, error)
        || !cborInteger(root, "schema_version", schemaVersion, scope, error)
        || !cborString(root, "record_type", recordType, scope, error)
        || !cborMap(root, "producer", producer, scope, error)
        || !cborString(producer, "application", application, QStringLiteral("root.producer"), error)
        || !cborString(producer, "build", producerBuild, QStringLiteral("root.producer"), error))
        return false;
    if (schema != QLatin1String("nrpl.rdps3.prp3"))
        return reject(error, scope, "schema", QStringLiteral("is unsupported"));
    if (schemaVersion != 1)
        return reject(error, scope, "schema_version", QStringLiteral("is unsupported"));
    if (application != QLatin1String("rdps3"))
        return reject(error, QStringLiteral("root.producer"), "application", QStringLiteral("is unsupported"));
    if (recordType != QLatin1String("plot") && recordType != QLatin1String("event"))
        return reject(error, scope, "record_type", QStringLiteral("is unsupported"));
    return true;
}

bool parseLegacy(const QCborMap &root, Prp3PlotRecord &record, QString &error)
{
    const QString scope = QStringLiteral("root.legacy");
    QCborMap map;
    QCborArray groups;
    if (!cborMap(root, "legacy", map, QStringLiteral("root"), error)
        || !cborBool(map, "valid", record.legacy.valid, scope, error)
        || !cborUnsigned(map, "azimuth_raw", record.legacy.azimuthRaw,
                         std::numeric_limits<quint16>::max(), scope, error)
        || !cborNumber(map, "azimuth_degrees", record.legacy.azimuthDegrees, scope, error)
        || !cborUnsigned(map, "range_raw", record.legacy.rangeRaw, std::numeric_limits<quint16>::max(), scope,
                         error)
        || !cborNumber(map, "range_km", record.legacy.rangeKm, scope, error)
        || !cborUnsigned(map, "selected_cluster_doppler", record.legacy.selectedClusterDoppler,
                         std::numeric_limits<quint8>::max(), scope, error)
        || !cborUnsigned(map, "cpi_group_count", record.legacy.cpiGroupCount,
                         std::numeric_limits<quint8>::max(), scope, error)
        || !cborUnsigned(map, "amplitude", record.legacy.amplitude, std::numeric_limits<quint16>::max(), scope,
                         error)
        || !cborUnsigned(map, "trailer", record.legacy.trailer, std::numeric_limits<quint8>::max(), scope, error)
        || !cborArray(map, "groups", groups, scope, error))
        return false;

    if (record.legacy.valid && (record.legacy.cpiGroupCount < 1 || record.legacy.cpiGroupCount > 15))
        return reject(error, scope, "cpi_group_count", QStringLiteral("is invalid for a valid legacy plot"));
    const auto expectedGroupCount = std::min<int>(record.legacy.cpiGroupCount, 15);
    if (groups.size() != expectedGroupCount)
        return reject(error, scope, "groups", QStringLiteral("does not match the bounded CPI group count"));

    record.legacy.groups.reserve(groups.size());
    for (int groupIndex = 0; groupIndex < groups.size(); ++groupIndex) {
        if (!groups.at(groupIndex).isMap())
            return reject(error, scope, "groups", QStringLiteral("contains a non-map element"));
        const auto groupMap = groups.at(groupIndex).toMap();
        const auto groupScope = QStringLiteral("%1.groups[%2]").arg(scope).arg(groupIndex);
        Prp3LegacyGroup group;
        if (!cborUnsigned(groupMap, "index", group.index, 14, groupScope, error)
            || !cborUnsigned(groupMap, "doppler", group.doppler, std::numeric_limits<quint8>::max(), groupScope,
                             error)
            || !cborUnsigned(groupMap, "amplitude", group.amplitude, std::numeric_limits<quint16>::max(),
                             groupScope, error))
            return false;
        if (group.index != groupIndex)
            return reject(error, groupScope, "index", QStringLiteral("does not match array order"));
        record.legacy.groups.append(group);
    }

    if (record.inputFrame.size() < PsrDoppler::LEGACY_PLOT_SIZE)
        return reject(error, QStringLiteral("root"), "input_frame", QStringLiteral("is shorter than a legacy plot"));
    const auto *bytes = reinterpret_cast<const quint8 *>(record.inputFrame.constData());
    if (bytes[3] != 1)
        return reject(error, QStringLiteral("root"), "input_frame", QStringLiteral("is not a type-1 PRP frame"));
    const auto frameLength = PsrDoppler::plotFrameLength(bytes, record.inputFrame.size());
    if (frameLength.status != PsrDoppler::FrameStatus::READY || frameLength.frameLength != record.inputFrame.size())
        return reject(error, QStringLiteral("root"), "input_frame",
                      QStringLiteral("has an invalid DPS1 length or trailing bytes"));
    if (record.legacy.azimuthRaw != qFromBigEndian<quint16>(bytes + 4)
        || record.legacy.rangeRaw != qFromBigEndian<quint16>(bytes + 6)
        || record.legacy.selectedClusterDoppler != bytes[8] || record.legacy.cpiGroupCount != bytes[9]
        || record.legacy.amplitude != qFromBigEndian<quint16>(bytes + 10) || record.legacy.trailer != bytes[57])
        return reject(error, scope, "*", QStringLiteral("does not match the exact input-frame legacy fields"));
    if (!numericallyEqual(record.legacy.azimuthDegrees,
                          90.0 - record.legacy.azimuthRaw * 360.0 / 16384.0, 1e-5)
        || !numericallyEqual(record.legacy.rangeKm, record.legacy.rangeRaw * 75.0 / 1000.0, 1e-5))
        return reject(error, scope, "*", QStringLiteral("contains inconsistent derived polar coordinates"));
    for (const auto &group : record.legacy.groups) {
        const auto offset = 12 + group.index * 3;
        if (group.doppler != bytes[offset] || group.amplitude != qFromBigEndian<quint16>(bytes + offset + 1))
            return reject(error, scope, "groups", QStringLiteral("does not match the exact input frame"));
    }
    return true;
}

bool parseFilter(const QCborMap &root, Prp3PlotRecord &record, QString &error)
{
    const QString scope = QStringLiteral("root.filter");
    QCborMap map;
    if (!cborMap(root, "filter", map, QStringLiteral("root"), error)
        || !cborBool(map, "evaluated", record.filter.evaluated, scope, error)
        || !cborBool(map, "enabled", record.filter.enabled, scope, error)
        || !cborNumber(map, "value", record.filter.value, scope, error)
        || !cborBool(map, "drop_plot", record.filter.dropPlot, scope, error))
        return false;
    return record.filter.evaluated == record.legacy.valid
        ? true
        : reject(error, scope, "evaluated", QStringLiteral("does not match legacy.valid"));
}

bool parseBackground(const QCborMap &root, Prp3PlotRecord &record, QString &error)
{
    const QString scope = QStringLiteral("root.background");
    QCborMap map;
    return cborMap(root, "background", map, QStringLiteral("root"), error)
        && cborBool(map, "enabled", record.background.enabled, scope, error)
        && cborBool(map, "observed", record.background.observed, scope, error)
        && cborBool(map, "passed", record.background.passed, scope, error)
        && cborBool(map, "warmup", record.background.warmup, scope, error)
        && cborBool(map, "history_degraded", record.background.historyDegraded, scope, error)
        && cborUnsigned(map, "threshold", record.background.threshold, std::numeric_limits<quint16>::max(), scope,
                        error);
}

bool parseSnapshot(const QCborMap &root, Prp3PlotRecord &record, QString &error)
{
    const QString scope = QStringLiteral("root.snapshot");
    QCborMap map;
    if (!cborMap(root, "snapshot", map, QStringLiteral("root"), error)
        || !cborBool(map, "present", record.snapshot.present, scope, error)
        || !cborBool(map, "decoded", record.snapshot.decoded, scope, error)
        || !cborUnsigned(map, "decode_error", record.snapshot.decodeError,
                         arrayCount(DECODE_ERROR_NAMES) - 1, scope, error)
        || !cborString(map, "decode_error_name", record.snapshot.decodeErrorName, scope, error))
        return false;
    if (!expectedText(record.snapshot.decodeErrorName, DECODE_ERROR_NAMES, arrayCount(DECODE_ERROR_NAMES),
                      record.snapshot.decodeError, error, scope, "decode_error_name"))
        return false;

    const bool rawPresent = record.legacy.trailer & PsrDoppler::EXTENSION_PRESENT;
    if (record.snapshot.present != rawPresent)
        return reject(error, scope, "present", QStringLiteral("does not match the legacy trailer"));
    record.snapshot.evidence = rawPresent
        ? PsrDoppler::decode(reinterpret_cast<const quint8 *>(record.inputFrame.constData())
                                + PsrDoppler::LEGACY_PLOT_SIZE,
                            record.inputFrame.size() - PsrDoppler::LEGACY_PLOT_SIZE)
        : PsrDoppler::DecodeResult();
    if (record.snapshot.decoded != record.snapshot.evidence.isValid()
        || record.snapshot.decodeError != quint8(record.snapshot.evidence.error))
        return reject(error, scope, "decoded", QStringLiteral("does not match the authoritative input-frame DPS1"));
    if (!record.snapshot.decoded)
        return true;

    const auto &snapshot = record.snapshot.evidence.snapshot;
    quint16 flags = 0, scanNumber = 0, azimuthCellNumber = 0, rangeBin = 0;
    quint8 representativeCpi = 0, signal = 0, bin = 0, mode = 0, reason = 0, branchCount = 0;
    QCborArray branches;
    if (!cborUnsigned(map, "flags", flags, std::numeric_limits<quint16>::max(), scope, error)
        || !cborUnsigned(map, "scan_number", scanNumber, std::numeric_limits<quint16>::max(), scope, error)
        || !cborUnsigned(map, "azimuth_cell_number", azimuthCellNumber, std::numeric_limits<quint16>::max(), scope,
                         error)
        || !cborUnsigned(map, "range_bin", rangeBin, std::numeric_limits<quint16>::max(), scope, error)
        || !cborUnsigned(map, "representative_cpi", representativeCpi, 3, scope, error)
        || !cborUnsigned(map, "signal", signal, arrayCount(SIGNAL_NAMES) - 1, scope, error)
        || !cborString(map, "signal_name", record.snapshot.signalName, scope, error)
        || !cborUnsigned(map, "bin", bin, arrayCount(BIN_NAMES) - 1, scope, error)
        || !cborString(map, "bin_name", record.snapshot.binName, scope, error)
        || !cborUnsigned(map, "mode", mode, arrayCount(MODE_NAMES) - 1, scope, error)
        || !cborString(map, "mode_name", record.snapshot.modeName, scope, error)
        || !cborUnsigned(map, "reason", reason, arrayCount(SNAPSHOT_REASON_NAMES) - 1, scope, error)
        || !cborString(map, "reason_name", record.snapshot.reasonName, scope, error)
        || !cborUnsigned(map, "branch_count", branchCount, 2, scope, error)
        || !cborArray(map, "branches", branches, scope, error))
        return false;
    if (!expectedText(record.snapshot.signalName, SIGNAL_NAMES, arrayCount(SIGNAL_NAMES), signal, error, scope,
                      "signal_name")
        || !expectedText(record.snapshot.binName, BIN_NAMES, arrayCount(BIN_NAMES), bin, error, scope,
                         "bin_name")
        || !expectedText(record.snapshot.modeName, MODE_NAMES, arrayCount(MODE_NAMES), mode, error, scope,
                         "mode_name")
        || !expectedText(record.snapshot.reasonName, SNAPSHOT_REASON_NAMES, arrayCount(SNAPSHOT_REASON_NAMES),
                         reason, error, scope, "reason_name"))
        return false;
    if (flags != snapshot.flags || scanNumber != snapshot.scanNumber || azimuthCellNumber != snapshot.azimuthCellNumber
        || rangeBin != snapshot.rangeBin || representativeCpi != snapshot.representativeCpi
        || signal != quint8(snapshot.signal) || bin != quint8(snapshot.bin) || mode != quint8(snapshot.mode)
        || reason != quint8(snapshot.reason) || branchCount != snapshot.branchCount
        || branches.size() != snapshot.branchCount)
        return reject(error, scope, "*", QStringLiteral("does not match the authoritative input-frame DPS1 header"));

    for (int branchIndex = 0; branchIndex < branches.size(); ++branchIndex) {
        if (!branches.at(branchIndex).isMap())
            return reject(error, scope, "branches", QStringLiteral("contains a non-map element"));
        const auto branchMap = branches.at(branchIndex).toMap();
        const auto branchScope = QStringLiteral("%1.branches[%2]").arg(scope).arg(branchIndex);
        const auto &rawBranch = snapshot.branches[branchIndex];
        quint8 branch = 0, sourceSic = 0, sampleCount = 0, branchReason = 0;
        quint16 validMask = 0;
        QString branchName;
        QCborArray samples;
        if (!cborUnsigned(branchMap, "branch", branch, arrayCount(BRANCH_NAMES) - 1, branchScope, error)
            || !cborString(branchMap, "branch_name", branchName, branchScope, error)
            || !cborUnsigned(branchMap, "source_sic", sourceSic, std::numeric_limits<quint8>::max(), branchScope,
                             error)
            || !cborUnsigned(branchMap, "sample_count", sampleCount, PsrDoppler::SAMPLE_COUNT, branchScope, error)
            || !cborUnsigned(branchMap, "reason", branchReason, std::numeric_limits<quint8>::max(), branchScope,
                             error)
            || !cborUnsigned(branchMap, "valid_mask", validMask, std::numeric_limits<quint16>::max(), branchScope,
                             error)
            || !cborArray(branchMap, "samples", samples, branchScope, error))
            return false;
        if (!expectedText(branchName, BRANCH_NAMES, arrayCount(BRANCH_NAMES), branch, error, branchScope,
                          "branch_name"))
            return false;
        if (branch != quint8(rawBranch.branch) || sourceSic != rawBranch.sourceSic
            || sampleCount != rawBranch.sampleCount || branchReason != rawBranch.reason
            || validMask != rawBranch.validMask || samples.size() != PsrDoppler::SAMPLE_COUNT)
            return reject(error, branchScope, "*",
                          QStringLiteral("does not match the authoritative input-frame DPS1 branch"));

        for (int sampleIndex = 0; sampleIndex < samples.size(); ++sampleIndex) {
            if (!samples.at(sampleIndex).isMap())
                return reject(error, branchScope, "samples", QStringLiteral("contains a non-map element"));
            const auto sampleMap = samples.at(sampleIndex).toMap();
            const auto sampleScope = QStringLiteral("%1.samples[%2]").arg(branchScope).arg(sampleIndex);
            int index = 0;
            double i = 0.0, q = 0.0;
            quint16 followingIntervalUs = 0;
            if (!cborUnsigned(sampleMap, "index", index, PsrDoppler::SAMPLE_COUNT - 1, sampleScope, error)
                || !cborNumber(sampleMap, "i", i, sampleScope, error)
                || !cborNumber(sampleMap, "q", q, sampleScope, error)
                || !cborUnsigned(sampleMap, "following_interval_us", followingIntervalUs,
                                 std::numeric_limits<quint16>::max(), sampleScope, error))
                return false;
            const auto &rawSample = rawBranch.samples[sampleIndex];
            if (index != sampleIndex || !numericallyEqual(i, rawSample.i) || !numericallyEqual(q, rawSample.q)
                || followingIntervalUs != rawSample.followingIntervalUs)
                return reject(error, sampleScope, "*",
                              QStringLiteral("does not match the authoritative input-frame DPS1 sample"));
        }
    }
    return true;
}

bool parseRestoration(const QCborMap &root, Prp3PlotRecord &record, QString &error)
{
    const QString scope = QStringLiteral("root.restoration");
    QCborMap map;
    QCborArray branches;
    if (!cborMap(root, "restoration", map, QStringLiteral("root"), error)
        || !cborBool(map, "valid", record.restoration.valid, scope, error)
        || !cborUnsigned(map, "flags", record.restoration.flags, std::numeric_limits<quint16>::max(), scope, error)
        || !cborUnsigned(map, "reason", record.restoration.reason, arrayCount(RESTORATION_REASON_NAMES) - 1, scope,
                         error)
        || !cborString(map, "reason_name", record.restoration.reasonName, scope, error)
        || !cborUnsigned(map, "branch_mask", record.restoration.branchMask, 3, scope, error)
        || !cborNumber(map, "frequency_hz", record.restoration.frequencyHz, scope, error)
        || !cborNumber(map, "radial_speed_mps", record.restoration.radialSpeedMps, scope, error)
        || !cborNumber(map, "radial_speed_kmh", record.restoration.radialSpeedKmh, scope, error)
        || !cborNumber(map, "coherent_energy", record.restoration.coherentEnergy, scope, error)
        || !cborNumber(map, "fit_residual_rad", record.restoration.fitResidualRad, scope, error)
        || !cborNumber(map, "ambiguity_margin", record.restoration.ambiguityMargin, scope, error)
        || !cborUnsigned(map, "candidate_index", record.restoration.candidateIndex,
                         std::numeric_limits<quint16>::max(), scope, error)
        || !cborString(map, "candidate_index_scope", record.restoration.candidateIndexScope, scope, error)
        || !cborNumber(map, "branch_disagreement_hz", record.restoration.branchDisagreementHz, scope, error)
        || !cborArray(map, "branches", branches, scope, error))
        return false;
    if (!expectedText(record.restoration.reasonName, RESTORATION_REASON_NAMES,
                      arrayCount(RESTORATION_REASON_NAMES), record.restoration.reason, error, scope,
                      "reason_name"))
        return false;
    if (record.restoration.candidateIndexScope != QLatin1String("plot_local_sorted_candidates"))
        return reject(error, scope, "candidate_index_scope", QStringLiteral("is unsupported"));
    if (record.restoration.valid != bool(record.restoration.flags & RESTORATION_RESULT_VALID)
        || (record.restoration.valid && record.restoration.reason != 0))
        return reject(error, scope, "valid", QStringLiteral("is inconsistent with flags/reason"));
    if (record.restoration.valid
        && std::fabs(record.restoration.frequencyHz) > PsrDoppler::MAX_ABS_DOPPLER_FREQUENCY_HZ)
        return reject(error, scope, "frequency_hz", QStringLiteral("exceeds the supported physical limit"));
    if (!numericallyEqual(record.restoration.radialSpeedKmh, record.restoration.radialSpeedMps * 3.6, 1e-5))
        return reject(error, scope, "radial_speed_kmh", QStringLiteral("does not match radial_speed_mps"));
    if (branches.size() != 2)
        return reject(error, scope, "branches", QStringLiteral("must contain fixed A/B slots"));

    record.restoration.branches.reserve(branches.size());
    for (int branchIndex = 0; branchIndex < branches.size(); ++branchIndex) {
        if (!branches.at(branchIndex).isMap())
            return reject(error, scope, "branches", QStringLiteral("contains a non-map element"));
        const auto branchMap = branches.at(branchIndex).toMap();
        const auto branchScope = QStringLiteral("%1.branches[%2]").arg(scope).arg(branchIndex);
        Prp3RestorationBranch branch;
        if (!cborUnsigned(branchMap, "slot", branch.slot, 1, branchScope, error)
            || !cborString(branchMap, "slot_name", branch.slotName, branchScope, error)
            || !cborUnsigned(branchMap, "branch", branch.branch, 1, branchScope, error)
            || !cborString(branchMap, "branch_name", branch.branchName, branchScope, error)
            || !cborBool(branchMap, "valid", branch.valid, branchScope, error)
            || !cborUnsigned(branchMap, "reason", branch.reason, arrayCount(RESTORATION_REASON_NAMES) - 1,
                             branchScope, error)
            || !cborString(branchMap, "reason_name", branch.reasonName, branchScope, error)
            || !cborNumber(branchMap, "frequency_hz", branch.frequencyHz, branchScope, error)
            || !cborNumber(branchMap, "coherent_energy", branch.coherentEnergy, branchScope, error)
            || !cborNumber(branchMap, "fit_residual_rad", branch.fitResidualRad, branchScope, error)
            || !cborNumber(branchMap, "ambiguity_margin", branch.ambiguityMargin, branchScope, error)
            || !cborUnsigned(branchMap, "candidate_index", branch.candidateIndex,
                             std::numeric_limits<quint16>::max(), branchScope, error)
            || !cborString(branchMap, "candidate_index_scope", branch.candidateIndexScope, branchScope, error))
            return false;
        if (branch.slot != branchIndex
            || !expectedText(branch.slotName, BRANCH_NAMES, arrayCount(BRANCH_NAMES), branch.slot, error,
                             branchScope, "slot_name")
            || !expectedText(branch.branchName, BRANCH_NAMES, arrayCount(BRANCH_NAMES), branch.branch, error,
                             branchScope, "branch_name")
            || !expectedText(branch.reasonName, RESTORATION_REASON_NAMES,
                             arrayCount(RESTORATION_REASON_NAMES), branch.reason, error, branchScope,
                             "reason_name"))
            return false;
        if (branch.candidateIndexScope != QLatin1String("branch_local_sorted_candidates"))
            return reject(error, branchScope, "candidate_index_scope", QStringLiteral("is unsupported"));
        if (branch.valid && (branch.reason != 0
                             || std::fabs(branch.frequencyHz) > PsrDoppler::MAX_ABS_DOPPLER_FREQUENCY_HZ))
            return reject(error, branchScope, "valid", QStringLiteral("is inconsistent with reason/frequency"));
        record.restoration.branches.append(branch);
    }
    return true;
}

bool parsePlot(const QCborMap &root, const QByteArray &payload, const QString &filePath, qint64 envelopeOffset,
               const QString &producerBuild, QSharedPointer<Prp3PlotRecord> &record, QString &error)
{
    auto parsed = QSharedPointer<Prp3PlotRecord>::create();
    parsed->filePath = filePath;
    parsed->envelopeOffset = envelopeOffset;
    parsed->cborPayload = payload;
    parsed->producerBuild = producerBuild;

    qint64 sequence = 0, inputFrameSize = 0, outputApoiSize = 0;
    if (!parseTimestamp(root, "arrival", parsed->arrivalUtcMs, parsed->arrivalUtcIso, QStringLiteral("root"), error)
        || !cborInteger(root, "sequence", sequence, QStringLiteral("root"), error)
        || !cborUnsigned(root, "input_channel", parsed->inputChannel, 1, QStringLiteral("root"), error)
        || !cborInteger(root, "input_frame_size", inputFrameSize, QStringLiteral("root"), error)
        || !cborBytes(root, "input_frame", parsed->inputFrame, QStringLiteral("root"), error)
        || !cborInteger(root, "output_apoi_size", outputApoiSize, QStringLiteral("root"), error)
        || !cborBytes(root, "output_apoi", parsed->outputApoi, QStringLiteral("root"), error))
        return false;
    if (sequence < 0)
        return reject(error, QStringLiteral("root"), "sequence", QStringLiteral("must be non-negative"));
    parsed->sequence = quint64(sequence);
    if (inputFrameSize < 0 || inputFrameSize != parsed->inputFrame.size())
        return reject(error, QStringLiteral("root"), "input_frame_size",
                      QStringLiteral("does not match input_frame"));
    if (outputApoiSize < 0 || outputApoiSize != parsed->outputApoi.size())
        return reject(error, QStringLiteral("root"), "output_apoi_size",
                      QStringLiteral("does not match output_apoi"));
    if (!parseLegacy(root, *parsed, error) || !parseFilter(root, *parsed, error)
        || !parseBackground(root, *parsed, error) || !parseSnapshot(root, *parsed, error)
        || !parseRestoration(root, *parsed, error))
        return false;

    record = std::move(parsed);
    return true;
}

bool parseEvent(const QCborMap &root, QString &error)
{
    qint64 utcMs = 0;
    QString utcIso, category, message;
    return parseTimestamp(root, "timestamp", utcMs, utcIso, QStringLiteral("root"), error)
        && cborString(root, "category", category, QStringLiteral("root"), error)
        && cborString(root, "message", message, QStringLiteral("root"), error);
}

bool parsePayload(const QByteArray &payload, const QString &filePath, qint64 envelopeOffset,
                  QSharedPointer<Prp3PlotRecord> &record, bool &event, QString &error)
{
    QCborStreamReader reader(payload);
    const auto tagged = QCborValue::fromCbor(reader);
    if (reader.lastError() != QCborError::NoError)
        return reject(error, QStringLiteral("payload"), "cbor",
                      QStringLiteral("is invalid at byte %1: %2")
                          .arg(reader.currentOffset())
                          .arg(reader.lastError().toString()));
    if (reader.currentOffset() != payload.size())
        return reject(error, QStringLiteral("payload"), "cbor",
                      QStringLiteral("contains trailing data at byte %1").arg(reader.currentOffset()));
    if (!tagged.isTag() || tagged.tag() != QCborTag(SELF_DESCRIBED_CBOR_TAG) || !tagged.taggedValue().isMap())
        return reject(error, QStringLiteral("payload"), "root",
                      QStringLiteral("must be self-described CBOR tag 55799 around a map"));

    const auto root = tagged.taggedValue().toMap();
    QString recordType, producerBuild;
    if (!parseCommon(root, recordType, producerBuild, error))
        return false;
    event = recordType == QLatin1String("event");
    return event ? parseEvent(root, error)
                 : parsePlot(root, payload, filePath, envelopeOffset, producerBuild, record, error);
}

QString fileError(const QString &path, qint64 offset, const QString &message)
{
    return QStringLiteral("%1 at byte %2: %3").arg(path).arg(offset).arg(message);
}
}

Prp3LogReader::Status Prp3LogReader::read(const QStringList &filePaths, Result &result, QString &error,
                                          const Progress &progress) const
{
    result = Result();
    error.clear();
    QStringList files = filePaths;
    files.removeDuplicates();
    std::sort(files.begin(), files.end());
    if (files.isEmpty()) {
        error = QStringLiteral("No PRP3 files were selected.");
        return Status::ERROR;
    }

    qint64 totalBytes = 0;
    for (QString &path : files) {
        const QFileInfo info(path);
        path = info.absoluteFilePath();
        if (!path.endsWith(QLatin1String(".prp3.cbor"), Qt::CaseInsensitive) || !info.isFile()) {
            error = QStringLiteral("Unsupported PRP source: %1. Only framed *.prp3.cbor files are accepted.").arg(path);
            return Status::ERROR;
        }
        if (info.size() > std::numeric_limits<qint64>::max() - totalBytes) {
            error = QStringLiteral("The selected PRP3 file set is too large.");
            return Status::ERROR;
        }
        totalBytes += info.size();
    }

    Result parsed;
    if (progress && !progress(0, totalBytes))
        return Status::CANCELLED;

    qint64 completedBytes = 0;
    for (const auto &path : files) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = fileError(path, 0, QStringLiteral("cannot open file: %1").arg(file.errorString()));
            return Status::ERROR;
        }
        const auto fileSize = file.size();
        const auto header = file.read(FILE_HEADER_SIZE);
        if (header.size() != FILE_HEADER_SIZE || header.left(8) != QByteArrayLiteral("PRP3CBOR")) {
            error = fileError(path, 0, QStringLiteral("invalid or truncated PRP3 file header"));
            return Status::ERROR;
        }
        const auto *headerBytes = reinterpret_cast<const uchar *>(header.constData());
        const auto version = qFromBigEndian<quint16>(headerBytes + 8);
        const auto headerSize = qFromBigEndian<quint16>(headerBytes + 10);
        const auto declaredMaximum = qFromBigEndian<quint32>(headerBytes + 12);
        if (version != FILE_FORMAT_VERSION || headerSize != FILE_HEADER_SIZE) {
            error = fileError(path, 8, QStringLiteral("unsupported file version/header size %1/%2")
                                                .arg(version)
                                                .arg(headerSize));
            return Status::ERROR;
        }
        if (declaredMaximum < FILE_HEADER_SIZE + RECORD_ENVELOPE_SIZE + 1
            || declaredMaximum > MAX_DECLARED_FILE_BYTES || fileSize > declaredMaximum) {
            error = fileError(path, 12, QStringLiteral("invalid declared maximum or oversized physical file"));
            return Status::ERROR;
        }

        while (!file.atEnd()) {
            const auto envelopeOffset = file.pos();
            const auto envelope = file.read(RECORD_ENVELOPE_SIZE);
            if (envelope.size() != RECORD_ENVELOPE_SIZE || envelope.left(4) != QByteArrayLiteral("P3R1")) {
                error = fileError(path, envelopeOffset, QStringLiteral("invalid or truncated record envelope"));
                return Status::ERROR;
            }
            const auto *envelopeBytes = reinterpret_cast<const uchar *>(envelope.constData());
            const auto payloadLength = qFromBigEndian<quint32>(envelopeBytes + 4);
            const auto expectedChecksum = qFromBigEndian<quint16>(envelopeBytes + 8);
            if (!payloadLength
                || payloadLength > declaredMaximum - FILE_HEADER_SIZE - RECORD_ENVELOPE_SIZE
                || qint64(payloadLength) > fileSize - file.pos()) {
                error = fileError(path, envelopeOffset + 4,
                                  QStringLiteral("record payload length exceeds the file/declaration"));
                return Status::ERROR;
            }
            const auto payload = file.read(payloadLength);
            if (payload.size() != int(payloadLength)) {
                error = fileError(path, envelopeOffset + RECORD_ENVELOPE_SIZE,
                                  QStringLiteral("truncated record payload"));
                return Status::ERROR;
            }
            if (qChecksum(payload.constData(), uint(payload.size()), Qt::ChecksumIso3309) != expectedChecksum) {
                error = fileError(path, envelopeOffset + 8, QStringLiteral("record checksum mismatch"));
                return Status::ERROR;
            }

            QSharedPointer<Prp3PlotRecord> record;
            bool event = false;
            QString payloadError;
            if (!parsePayload(payload, path, envelopeOffset, record, event, payloadError)) {
                error = fileError(path, envelopeOffset + RECORD_ENVELOPE_SIZE, payloadError);
                return Status::ERROR;
            }
            if (event)
                ++parsed.eventCount;
            else {
                const auto arrival = QDateTime::fromMSecsSinceEpoch(record->arrivalUtcMs, Qt::UTC);
                parsed.begin = !parsed.begin.isValid() || arrival < parsed.begin ? arrival : parsed.begin;
                parsed.end = !parsed.end.isValid() || arrival > parsed.end ? arrival : parsed.end;
                parsed.plots.append(std::move(record));
            }

            if (progress && !progress(completedBytes + file.pos(), totalBytes))
                return Status::CANCELLED;
        }
        completedBytes += fileSize;
    }

    result = std::move(parsed);
    return Status::SUCCESS;
}
