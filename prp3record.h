#pragma once

#include <libradardata/psrdopplersnapshot.h>

#include <QByteArray>
#include <QCborArray>
#include <QCborMap>
#include <QString>
#include <QVector>

struct Prp3LegacyGroup
{
    int index = 0;
    quint8 doppler = 0;
    quint16 amplitude = 0;
};

struct Prp3LegacyInfo
{
    bool valid = false;
    quint16 azimuthRaw = 0;
    double azimuthDegrees = 0.0;
    quint16 rangeRaw = 0;
    double rangeKm = 0.0;
    quint8 selectedClusterDoppler = 0;
    quint8 cpiGroupCount = 0;
    quint16 amplitude = 0;
    quint8 trailer = 0;
    QVector<Prp3LegacyGroup> groups;
};

struct Prp3FilterInfo
{
    bool evaluated = false;
    bool enabled = false;
    double value = 0.0;
    bool dropPlot = false;
};

struct Prp3BackgroundInfo
{
    bool enabled = false;
    bool observed = false;
    bool passed = false;
    bool warmup = false;
    bool historyDegraded = false;
    quint16 threshold = 0;
};

struct Prp3SnapshotInfo
{
    bool present = false;
    bool decoded = false;
    quint8 decodeError = quint8(PsrDoppler::DecodeError::NOT_PRESENT);
    QString decodeErrorName;
    QString signalName;
    QString binName;
    QString modeName;
    QString reasonName;
    PsrDoppler::DecodeResult evidence;
};

struct Prp3RestorationBranch
{
    quint8 slot = 0;
    QString slotName;
    quint8 branch = 0;
    QString branchName;
    bool valid = false;
    quint8 reason = 0;
    QString reasonName;
    double frequencyHz = 0.0;
    double coherentEnergy = 0.0;
    double fitResidualRad = 0.0;
    double ambiguityMargin = 0.0;
    quint16 candidateIndex = 0;
    QString candidateIndexScope;
};

struct Prp3RestorationInfo
{
    bool valid = false;
    quint16 flags = 0;
    quint8 reason = 0;
    QString reasonName;
    quint8 branchMask = 0;
    double frequencyHz = 0.0;
    double radialSpeedMps = 0.0;
    double radialSpeedKmh = 0.0;
    double coherentEnergy = 0.0;
    double fitResidualRad = 0.0;
    double ambiguityMargin = 0.0;
    quint16 candidateIndex = 0;
    QString candidateIndexScope;
    double branchDisagreementHz = 0.0;
    QVector<Prp3RestorationBranch> branches;
};

// F-108 target/clutter motion evidence: the logged rdps3 result when the producer evaluated it, otherwise recomputed
// from the decoded snapshot with the production DopplerRestorer defaults.
struct Prp3MotionInfo
{
    enum class Source { NONE, LOGGED, RECOMPUTED };

    Source source = Source::NONE;
    quint8 motionClass = 0; // DopplerRestorer::MotionClass: 0 unavailable, 1 stationary-like, 2 undecided, 3 moving
    bool mtiAvailable = false;
    bool incoherenceAvailable = false;
    double mtiRatio = 0.0;
    double stationaryIncoherence = 0.0;
    double peakSnrDb = 0.0;
    int supportedSamples = 0;
};

struct Prp3PlotRecord
{
    QString runId;
    qint64 inputOrdinal = -1;
    int radarId = 1;
    bool richDiagnostics = true;
    bool compact = false;
    QCborMap originalSnapshot;
    QCborArray preprocessingOutcomes;
    QCborMap runInfo;
    int preprocessingStatus = 0;
    QString filePath;
    qint64 envelopeOffset = 0;
    QByteArray cborPayload;
    QString producerBuild;
    qint64 arrivalUtcMs = 0;
    QString arrivalUtcIso;
    quint64 sequence = 0;
    quint8 inputChannel = 0;
    QByteArray inputFrame;
    QByteArray outputApoi;
    Prp3LegacyInfo legacy;
    Prp3FilterInfo filter;
    Prp3BackgroundInfo background;
    Prp3SnapshotInfo snapshot;
    Prp3RestorationInfo restoration;
    Prp3MotionInfo motion;
};
