#pragma once

#include <QCborMap>
#include <QMap>
#include <QPointF>
#include <QSharedPointer>
#include <QSet>
#include <QStringList>
#include <QVector>

#include <limits>

struct Prp3TrackerInput
{
    QString run;
    qint64 ordinal = -1;
    qint64 measurementMs = 0;
    qint64 processingMs = 0;
    int radar = 0;
    bool delivered = false;
    QCborMap fields;
};

struct Prp3TrackerSample
{
    QString run, event, phase, reason;
    qint64 life = 0, input = -1, sequence = 0;
    int table = 0, slot = 0;
    double time = 0.0, processingTime = 0.0;
    double lastScoreTime = std::numeric_limits<double>::quiet_NaN();
    QPointF stateLl;
    QCborMap fields, evidence;
    QStringList warnings;

    double carriedTargetClutter = std::numeric_limits<double>::quiet_NaN();
    double carriedScoreTime = std::numeric_limits<double>::quiet_NaN();

    double metric(const QString &key, bool carryForward = false) const;
    bool updated() const { return evidence.value(QStringLiteral("score_updated")).toBool(); }
    QString key() const;
};

struct Prp3TrackerLife
{
    QString run;
    qint64 id = 0;
    int table = 0, slot = 0;
    QVector<QSharedPointer<Prp3TrackerSample>> samples;
    QSharedPointer<Prp3TrackerSample> confirmation, lastScored;
    QStringList warnings;
    bool seeded = false, ended = false;
};

struct Prp3TrackerUse
{
    QSharedPointer<const Prp3TrackerLife> life;
    QSharedPointer<const Prp3TrackerSample> sample;
};

enum class Prp3EvidenceScope { OBSERVATION, CONFIRMATION, LAST_SCORED };
QSharedPointer<const Prp3TrackerSample> prp3EvidenceSample(const Prp3TrackerUse &use, Prp3EvidenceScope scope);

class Prp3TrackerRecording
{
public:
    bool append(const QCborMap &record, QString &error);
    void resolve();
    static QString inputKey(const QString &run, qint64 input);
    static QString lifeKey(const QString &run, int table, qint64 life);

    QMap<QString, QCborMap> runs;
    QMap<QString, Prp3TrackerInput> inputs;
    QMap<QString, QSharedPointer<Prp3TrackerLife>> lives;
    QMap<QString, QVector<Prp3TrackerUse>> inputUses;
    QStringList warnings;

private:
    QMap<QString, qint64> m_parts, m_sequences;
    QSet<QString> m_endedRuns;
};
