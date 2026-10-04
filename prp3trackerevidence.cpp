#include "prp3trackerevidence.h"

#include <libradardata/psrdopplersnapshot.h>

#include <QCborArray>
#include <QSet>

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr qint64 MAX_TIMESTAMP_MS = 253402300799999LL;
double optionalNumber(const QCborValue &value)
{
    return (value.isDouble() || value.isInteger()) && std::isfinite(value.toDouble())
            ? value.toDouble() : std::numeric_limits<double>::quiet_NaN();
}

bool validNumbers(const QCborValue &value)
{
    if (value.isDouble())
        return std::isfinite(value.toDouble());
    if (value.isArray()) {
        const auto array = value.toArray();
        return std::all_of(array.cbegin(), array.cend(), validNumbers);
    }
    if (value.isMap()) {
        const auto map = value.toMap();
        return std::all_of(map.cbegin(), map.cend(), [](const auto &field) { return validNumbers(field.second); });
    }
    return true;
}
}

double Prp3TrackerSample::metric(const QString &key, bool carryForward) const
{
    if (!warnings.isEmpty())
        return std::numeric_limits<double>::quiet_NaN();
    const auto value = optionalNumber(evidence.value(key));
    return carryForward && !updated() && !std::isfinite(value) && key == QStringLiteral("L_target_clutter")
            ? carriedTargetClutter : value;
}

QString Prp3TrackerSample::key() const
{
    return Prp3TrackerRecording::lifeKey(run, table, life);
}

QSharedPointer<const Prp3TrackerSample> prp3EvidenceSample(const Prp3TrackerUse &use, Prp3EvidenceScope scope)
{
    return scope == Prp3EvidenceScope::OBSERVATION ? use.sample
            : !use.life ? QSharedPointer<const Prp3TrackerSample>{}
            : scope == Prp3EvidenceScope::CONFIRMATION ? use.life->confirmation : use.life->lastScored;
}

QString Prp3TrackerRecording::inputKey(const QString &run, qint64 input)
{
    return run + QLatin1Char('/') + QString::number(input);
}

QString Prp3TrackerRecording::lifeKey(const QString &run, int table, qint64 life)
{
    return run + QLatin1Char('/') + QString::number(table) + QLatin1Char('/') + QString::number(life);
}

bool Prp3TrackerRecording::append(const QCborMap &record, QString &error)
{
    const auto reject = [&error](const QString &message) { error = message; return false; };
    const auto run = record.value(QStringLiteral("run_id")).toString();
    const auto type = record.value(QStringLiteral("record_type")).toString();
    if (run.isEmpty() || !validNumbers(record))
        return reject(QStringLiteral("Tracker recording needs a run identity and finite-or-null numeric values"));
    if (type == QStringLiteral("run")) {
        const auto part = record.value(QStringLiteral("part"));
        if (!part.isInteger() || part.toInteger() < 0 || !record.value(QStringLiteral("tracker")).isMap())
            return reject(QStringLiteral("Malformed tracker run metadata"));
        if (m_parts.contains(run) && part.toInteger() <= m_parts.value(run))
            return reject(QStringLiteral("Duplicate or out-of-order recording part"));
        if (part.toInteger() != m_parts.value(run, -1) + 1)
            warnings.append(QStringLiteral("Run %1 has missing earlier/intermediate parts").arg(run));
        if (m_endedRuns.contains(run))
            return reject(QStringLiteral("Recording part appears after run end"));
        auto previous = runs.value(run);
        auto current = record;
        previous.remove(QStringLiteral("part"));
        current.remove(QStringLiteral("part"));
        if (!previous.isEmpty() && previous != current)
            return reject(QStringLiteral("Recording configuration changes between parts"));
        m_parts.insert(run, part.toInteger());
        runs.insert(run, record);
        return true;
    }
    if (!runs.contains(run) || m_endedRuns.contains(run))
        return reject(QStringLiteral("Tracker record appears outside run boundaries"));
    const auto sequence = record.value(QStringLiteral("record_sequence"));
    if (!sequence.isInteger() || sequence.toInteger() <= m_sequences.value(run, 0))
        return reject(QStringLiteral("Missing or non-increasing record sequence"));
    if (sequence.toInteger() != m_sequences.value(run, 0) + 1)
        warnings.append(QStringLiteral("Run %1 has a gap in its record sequence").arg(run));
    m_sequences.insert(run, sequence.toInteger());
    if (type == QStringLiteral("end")) {
        m_endedRuns.insert(run);
        return true;
    }
    if (type == QStringLiteral("event"))
        return true;
    if (type == QStringLiteral("plot") || type == QStringLiteral("input")) {
        const auto input = record.value(QStringLiteral("input")).toMap();
        for (const auto *key : { "ordinal", "file", "file_ordinal", "scan", "measurement_ms", "processing_ms",
                                 "radar", "channel" })
            if (!input.value(QLatin1String(key)).isInteger())
                return reject(QStringLiteral("Missing integral input field %1").arg(QLatin1String(key)));
        for (const auto *key : { "north", "synthetic", "delivered" })
            if (!input.value(QLatin1String(key)).isBool())
                return reject(QStringLiteral("Missing boolean input field %1").arg(QLatin1String(key)));
        const auto bytes = input.value(QStringLiteral("frame"));
        const auto frame = bytes.toByteArray();
        if (!bytes.isByteArray() || frame.size() < 5 || frame.left(3) != QByteArray::fromHex("ffffff"))
            return reject(QStringLiteral("Invalid original input frame"));
        const auto typeByte = quint8(frame.at(3));
        if (typeByte == 1) {
            // The decoder borrows QByteArray's byte storage only for this call.
            const auto length = PsrDoppler::plotFrameLength(reinterpret_cast<const quint8 *>(frame.constData()),
                                                           frame.size());
            if (length.status != PsrDoppler::FrameStatus::READY || length.frameLength != frame.size())
                return reject(QStringLiteral("Invalid original plot frame length"));
        } else if (typeByte != 2 || frame.size() != 5) {
            return reject(QStringLiteral("Invalid original marker frame"));
        }
        const auto ordinal = input.value(QStringLiteral("ordinal")).toInteger();
        const auto radar = input.value(QStringLiteral("radar")).toInteger();
        const auto channel = input.value(QStringLiteral("channel")).toInteger();
        const auto file = input.value(QStringLiteral("file")).toInteger();
        const auto source = input.value(QStringLiteral("source_ordinal"));
        const auto key = inputKey(run, ordinal);
        if (ordinal < 0 || radar < 0 || radar > 255 || channel < 0 || channel > 1 || inputs.contains(key)
            || file < 0 || file > std::numeric_limits<int>::max()
            || input.value(QStringLiteral("file_ordinal")).toInteger() < 0
            || (!source.isUndefined() && (!source.isInteger() || source.toInteger() < 0))
            || input.value(QStringLiteral("measurement_ms")).toInteger() < 0
            || input.value(QStringLiteral("processing_ms")).toInteger() < 0
            || input.value(QStringLiteral("measurement_ms")).toInteger() > MAX_TIMESTAMP_MS
            || input.value(QStringLiteral("processing_ms")).toInteger() > MAX_TIMESTAMP_MS
            || input.value(QStringLiteral("north")).toBool() != (typeByte == 2 && frame.at(4) == 0))
            return reject(QStringLiteral("Invalid or duplicate input identity/clocks/channel"));
        inputs.insert(key, { run, ordinal, input.value(QStringLiteral("measurement_ms")).toInteger(),
                            input.value(QStringLiteral("processing_ms")).toInteger(), int(radar),
                            input.value(QStringLiteral("delivered")).toBool(), input });
        return true;
    }
    if (type != QStringLiteral("track"))
        return reject(QStringLiteral("Unsupported tracker record type"));
    for (const auto *key : { "table", "slot", "life", "input", "status", "found", "waiting" })
        if (!record.value(QLatin1String(key)).isInteger())
            return reject(QStringLiteral("Missing integral track field %1").arg(QLatin1String(key)));
    const auto sample = QSharedPointer<Prp3TrackerSample>::create();
    sample->run = run;
    sample->sequence = sequence.toInteger();
    sample->life = record.value(QStringLiteral("life")).toInteger();
    sample->input = record.value(QStringLiteral("input")).toInteger();
    const auto table = record.value(QStringLiteral("table")).toInteger();
    const auto slot = record.value(QStringLiteral("slot")).toInteger();
    if (table < 0 || table > 255 || slot < 0 || slot > 65535 || sample->life < 0 || sample->input < -1)
        return reject(QStringLiteral("Invalid tracker identity"));
    sample->table = int(table);
    sample->slot = int(slot);
    sample->event = record.value(QStringLiteral("event")).toString();
    const QStringList events { QStringLiteral("seed"), QStringLiteral("update"), QStringLiteral("confirm"),
                              QStringLiteral("reacquire"), QStringLiteral("free"), QStringLiteral("output"),
                              QStringLiteral("evidence"), QStringLiteral("evidence_end") };
    if (!events.contains(sample->event))
        return reject(QStringLiteral("Unknown tracker lifecycle event"));
    sample->time = optionalNumber(record.value(QStringLiteral("time")));
    sample->processingTime = optionalNumber(record.value(QStringLiteral("processing_time")));
    sample->lastScoreTime = optionalNumber(record.value(QStringLiteral("last_score_time")));
    const auto ll = record.value(QStringLiteral("state_ll")).toArray();
    if (!std::isfinite(sample->time) || !std::isfinite(sample->processingTime) || ll.size() != 2
        || sample->time < 0.0 || sample->time > MAX_TIMESTAMP_MS / 1000.0
        || sample->processingTime < 0.0 || sample->processingTime > MAX_TIMESTAMP_MS / 1000.0
        || !std::isfinite(optionalNumber(ll.at(0))) || !std::isfinite(optionalNumber(ll.at(1)))
        || std::abs(ll.at(0).toDouble()) > 90.0 || std::abs(ll.at(1).toDouble()) > 180.0)
        return reject(QStringLiteral("Missing tracker time or geographic position"));
    sample->stateLl = QPointF(ll.at(0).toDouble(), ll.at(1).toDouble());
    sample->fields = record;
    sample->phase = record.value(QStringLiteral("phase")).toString();
    sample->reason = record.value(QStringLiteral("reason")).toString();
    sample->evidence = record.value(QStringLiteral("evidence")).toMap();
    if (sample->event == QStringLiteral("evidence")) {
        for (const auto *key : { "samples", "distinct_scans", "scored_hits", "hypothesis_hits",
                                 "nominal_misses_scored", "nominal_misses_unscored", "dof" }) {
            const auto value = sample->evidence.value(QLatin1String(key));
            if (!value.isInteger() || value.toInteger() < 0)
                return reject(QStringLiteral("Invalid evidence counter %1").arg(QLatin1String(key)));
        }
        for (const auto *key : { "hypothesis_enabled", "hypothesis_evaluated", "score_updated", "score_available" })
            if (!sample->evidence.value(QLatin1String(key)).isBool())
                return reject(QStringLiteral("Missing evidence availability field %1").arg(QLatin1String(key)));
        for (const auto *key : { "Jcv", "Jcv_dof", "NIS", "L_target_poisson", "L_persistent_poisson",
                                 "L_clutter_poisson", "L_target_clutter" }) {
            const auto value = sample->evidence.value(QLatin1String(key));
            if (!value.isNull() && !std::isfinite(optionalNumber(value)))
                return reject(QStringLiteral("Evidence %1 must be finite or null").arg(QLatin1String(key)));
        }
        if (sample->evidence.value(QStringLiteral("score_available")).toBool()
            != std::isfinite(optionalNumber(sample->evidence.value(QStringLiteral("L_target_clutter")))))
            return reject(QStringLiteral("Evidence availability disagrees with recorded score"));
        if (sample->updated() && (!sample->evidence.value(QStringLiteral("score_available")).toBool()
                                 || !sample->evidence.value(QStringLiteral("hypothesis_enabled")).toBool()
                                 || sample->evidence.value(QStringLiteral("hypothesis_reason")).toString()
                                         != QStringLiteral("scored")))
            return reject(QStringLiteral("Updated evidence has inconsistent availability/reason"));
    }
    auto &life = lives[sample->key()];
    if (!life) {
        life = QSharedPointer<Prp3TrackerLife>::create();
        life->run = run;
        life->id = sample->life;
        life->table = sample->table;
        life->slot = sample->slot;
    }
    if (life->slot != sample->slot)
        return reject(QStringLiteral("Track slot changes within one lifetime"));
    if (sample->event == QStringLiteral("seed")) {
        if (life->seeded || life->ended)
            sample->warnings.append(QStringLiteral("Duplicate seed within one lifetime"));
        life->seeded = true;
    } else if (!life->seeded) {
        sample->warnings.append(QStringLiteral("Seed is absent from this recording"));
    }
    if (life->ended && sample->event != QStringLiteral("output"))
        sample->warnings.append(QStringLiteral("Lifecycle event follows free; tracker lifetime anomaly"));
    if (life->ended && sample->event == QStringLiteral("output"))
        sample->warnings.append(QStringLiteral("Output follows free; may reference cleared tracker state"));
    life->ended = life->ended || sample->event == QStringLiteral("free");
    life->samples.append(sample);
    return true;
}

void Prp3TrackerRecording::resolve()
{
    for (auto it = runs.cbegin(); it != runs.cend(); ++it)
        if (!m_endedRuns.contains(it.key()))
            warnings.append(QStringLiteral("Run %1 has no end record (live/open or partial recording)").arg(it.key()));
    for (const auto &life : std::as_const(lives)) {
        // Evidence may follow seed or precede update/confirmation; match the exact committed observation.
        QMap<QPair<qint64, double>, QSharedPointer<Prp3TrackerSample>> observations;
        for (const auto &sample : std::as_const(life->samples)) {
            if (sample->input >= 0 && !inputs.contains(inputKey(sample->run, sample->input)))
                sample->warnings.append(QStringLiteral("Referenced source input is missing"));
            if (sample->event != QStringLiteral("evidence"))
                continue;
            const auto key = qMakePair(sample->input, sample->time);
            if (const auto previous = observations.value(key)) {
                const auto warning = QStringLiteral("Duplicate evidence for one committed observation");
                previous->warnings.append(warning);
                sample->warnings.append(warning);
            }
            observations.insert(key, sample);
        }
        QSharedPointer<const Prp3TrackerSample> previousScore;
        for (const auto &sample : std::as_const(life->samples)) {
            const auto observation = observations.value({ sample->input, sample->time });
            if (observation && (sample->event == QStringLiteral("seed") || sample->event == QStringLiteral("update")
                                || sample->event == QStringLiteral("confirm"))) {
                sample->evidence = observation->evidence;
                sample->phase = observation->phase;
                sample->lastScoreTime = observation->lastScoreTime;
                sample->warnings.append(observation->warnings);
            }
            sample->warnings.removeDuplicates();
            sample->carriedTargetClutter = sample->carriedScoreTime = std::numeric_limits<double>::quiet_NaN();
            if (std::isfinite(sample->metric(QStringLiteral("L_target_clutter")))) {
                previousScore = sample;
            } else if (!sample->updated() && sample->warnings.isEmpty() && previousScore
                       && previousScore->time <= sample->time) {
                // Display-only carry: retain raw fields and never borrow from a later observation or another lifetime.
                sample->carriedTargetClutter = previousScore->metric(QStringLiteral("L_target_clutter"));
                sample->carriedScoreTime = previousScore->time;
            }
            life->warnings.append(sample->warnings);
            if (sample->event == QStringLiteral("confirm") && !life->confirmation)
                life->confirmation = sample;
            if (sample->event == QStringLiteral("evidence") && sample->updated() && sample->warnings.isEmpty())
                life->lastScored = sample;
            if (sample->event == QStringLiteral("seed") || sample->event == QStringLiteral("update"))
                inputUses[inputKey(sample->run, sample->input)].append({ life, sample });
        }
        life->warnings.removeDuplicates();
    }
    warnings.removeDuplicates();
}
