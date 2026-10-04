#include "prp3logreader.h"
#include "prp3signalwindow.h"
#include "trackerevidencepanel.h"

#include <processor/rdps3/prparchive.h>
#include <processor/rdps3/prp3plotlog.h>
#include <processor/rdps3/prp3compact.h>
#include <processor/rdps3/prefilterssr.h>
#include <libradarmap/nradarmap.h>
#include <processor/rdps3/tentativetrackevidence.h>

#include <QApplication>
#include <QCborArray>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QTemporaryDir>
#include <QTableWidget>
#include <QTabWidget>
#include <QTextBrowser>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <stdexcept>
#include <utility>

namespace {
constexpr qint64 EPOCH_MS = 946684800000LL;

void require(bool condition, const char *message)
{
    if (!condition)
        throw std::runtime_error(message);
}

QCborMap runInfo()
{
    return { { QStringLiteral("run_id"), QStringLiteral("test-run") },
             { QStringLiteral("tracker"), QCborMap{} },
             { QStringLiteral("clock_policy"), QStringLiteral("explicit_replay") },
             { QStringLiteral("period_ms"), 4000 }, { QStringLiteral("tick_ms"), 1 },
             { QStringLiteral("epoch_ms"), EPOCH_MS }, { QStringLiteral("tail_s"), 60.0 } };
}

QCborMap input(qint64 id, qint64 measurement, qint64 processing, const QByteArray &frame, bool delivered = true)
{
    return { { QStringLiteral("ordinal"), id }, { QStringLiteral("source_ordinal"), id + 10 },
             { QStringLiteral("file"), 2 }, { QStringLiteral("file_ordinal"), id + 100 },
             { QStringLiteral("scan"), id }, { QStringLiteral("measurement_ms"), measurement },
             { QStringLiteral("processing_ms"), processing }, { QStringLiteral("radar"), 51 },
             { QStringLiteral("channel"), 0 }, { QStringLiteral("north"), frame == QByteArray::fromHex("ffffff0200") },
             { QStringLiteral("synthetic"), false }, { QStringLiteral("delivered"), delivered },
             { QStringLiteral("frame"), frame }, { QStringLiteral("output_apoi"), QByteArray{} } };
}

QCborMap track(qint64 id, double time, const QString &event, int table = 51, qint64 life = 1)
{
    return { { QStringLiteral("input"), id }, { QStringLiteral("time"), time },
             { QStringLiteral("processing_time"), time }, { QStringLiteral("measurement_time"), time },
             { QStringLiteral("table"), table }, { QStringLiteral("slot"), 0 },
             { QStringLiteral("life"), life }, { QStringLiteral("event"), event },
             { QStringLiteral("status"), 1 }, { QStringLiteral("found"), 2 }, { QStringLiteral("waiting"), 0 },
             { QStringLiteral("state_ll"), QCborArray{ 60.0, 30.0 } },
             { QStringLiteral("last_score_time"), id > 1 ? QCborValue(time) : QCborValue(nullptr) } };
}

QStringList filesIn(const QString &directory)
{
    QStringList result;
    for (const auto &name : QDir(directory).entryList({ QStringLiteral("*.prp3.cbor") }, QDir::Files, QDir::Name))
        result.append(QDir(directory).filePath(name));
    return result;
}

Prp3LogReader::Result read(const QStringList &files)
{
    Prp3LogReader::Result result;
    QString error;
    const auto status = Prp3LogReader().read(files, result, error);
    if (status != Prp3LogReader::Status::SUCCESS)
        throw std::runtime_error(error.toStdString());
    return result;
}

void compactPlotsAndOutcomes()
{
    using namespace Prp3Compact;
    QTemporaryDir directory;
    Prp3PlotLog writer(directory.path(), 5000, true, Prp3PlotLog::Format::COMPACT);
    auto configuration = runInfo();
    configuration.insert(QStringLiteral("map_center"), QCborArray{ 60.0, 30.0 });
    configuration.insert(QStringLiteral("psr_only"), false);
    writer.configure(configuration);
    const QScopedPointer<NRadarMap> map(NRadarMap::createMap(60.0, 30.0));
    NRadarPlot ssr(50, QDateTime::fromMSecsSinceEpoch(EPOCH_MS, Qt::UTC), map.data());
    ssr.setSourceType(NRadarPlot::SSR);
    ssr.setSSRType(NRadarPlot::ModeS);
    ssr.setLLCoord({ 60.1234567890123, 30.9876543210987 });
    ssr.setBoardNumber(PLOT_HAS_NO_SQUAWK);
    ssr.setHeight(PLOT_HAS_NO_HEIGHT);
    ssr.setSPI(true);
    ssr.setSOS(true);
    ssr.setParrot(true);
    ssr.setHeightRelative(true);
    ssr.setOption(NRadarPlot::AircraftAddress, quint32(0xabcdef));
    ssr.setOption(NRadarPlot::AircraftId, QStringLiteral("TEST123"));
    ssr.setOption(NRadarPlot::TestPlot, false);
    QString error;
    const auto original = snapshot(ssr, error);
    require(error.isEmpty() && validateSnapshot(original, error), "complete MSSR snapshot invalid");
    const auto displayed = displayPlot(original, map.data());
    require(displayed->getLLCoord() == ssr.getLLCoord() && displayed->getOptions() == ssr.getOptions()
                && displayed->getBoardNumber() == ssr.getBoardNumber() && displayed->hasSPI()
                && displayed->hasSOS() && displayed->isParrot() && displayed->isHeightRelative(),
            "MSSR scalar/option fidelity");
    const auto id = writer.nextObservation();
    writer.writeCompact(Kind::OBSERVATION, { { Key::ID, id }, { Key::TIME, EPOCH_MS },
        { Key::PLOT, original }, { Key::STATUS, qint64(Outcome::PENDING) } });
    Prp3PlotLog::PlotRecord psr;
    psr.sequence = 1;
    psr.legacyValid = true;
    psr.inputFrame = QByteArray(58, 0);
    psr.inputFrame.replace(0, 4, QByteArray::fromHex("ffffff01"));
    psr.inputFrame[9] = 1;
    psr.restoration.reason = DopplerRestorer::Reason::AMBIGUOUS_HYPOTHESIS;
    psr.restoration.motion.mtiAvailable = true;
    psr.restoration.motion.mtiRatio = 0.1234567f;
    psr.restoration.motion.incoherenceAvailable = true;
    psr.restoration.motion.stationaryIncoherence = 0.2345678f;
    for (int i = 0; i < 30; ++i)
        writer.writePlot(psr, input(i, EPOCH_MS - i, EPOCH_MS, psr.inputFrame));
    ssr.setOption(NRadarPlot::TestPlot, true);
    const auto delivered = snapshot(ssr, error);
    writer.writeCompact(Kind::OUTCOME, { { Key::ORIGIN, id }, { Key::TIME, EPOCH_MS + 50 },
        { Key::STATUS, qint64(Outcome::FORWARDED) }, { Key::REASON, 0 },
        { Key::CHANGES, changes(original, delivered) } });
    const auto pending = writer.nextObservation();
    writer.writeCompact(Kind::OBSERVATION, { { Key::ID, pending }, { Key::TIME, EPOCH_MS + 51 },
        { Key::PLOT, original }, { Key::STATUS, qint64(Outcome::PENDING) } });
    writer.finishCompact();
    require(!writer.failed(), "compact writer failed");
    const auto files = filesIn(directory.path());
    require(files.size() > 1, "compact rotation not exercised");
    const auto result = read(files);
    require(result.plots.size() == 32 && result.tracker->lives.isEmpty() && result.tracker->inputs.isEmpty(),
            "compact original cardinality or forbidden track evidence");
    const auto recorded = result.plots.first();
    require(recorded->originalSnapshot == original && recorded->preprocessingOutcomes.size() == 1
                && recorded->preprocessingStatus == int(Outcome::FORWARDED)
                && applyChanges(original, recorded->preprocessingOutcomes.first().toMap().value(Key::CHANGES).toMap())
                    == delivered, "delayed cross-part outcome or immutable original lost");
    const auto signal = result.plots.at(1);
    require(!signal->richDiagnostics && !signal->restoration.valid
                && signal->motion.source == Prp3MotionInfo::Source::LOGGED && signal->motion.mtiAvailable
                && signal->motion.mtiRatio == psr.restoration.motion.mtiRatio
                && signal->motion.stationaryIncoherence == psr.restoration.motion.stationaryIncoherence,
            "rejected Doppler lost full-precision motion evidence");
    require(result.tracker->warnings.join(' ').contains(QStringLiteral("pending")), "pending end not reported");
    require(!read(files.mid(1)).tracker->warnings.isEmpty(), "selected part hides missing context");
    Prp3SignalWindow window;
    const auto contains = [&window](const QString &text) {
        const auto browsers = window.findChildren<QTextBrowser *>();
        return std::any_of(browsers.cbegin(), browsers.cend(), [&text](const auto *browser) {
            return browser->toPlainText().contains(text);
        });
    };
    window.setRecord(recorded);
    require(contains(QStringLiteral("TEST123")) && contains(QStringLiteral("forwarded toward tracker input")),
            "MSSR original/outcome inspection unavailable");
    window.setRecord(signal);
    require(contains(QStringLiteral("Actual live signal result")) && contains(QStringLiteral("q 0.123")),
            "rejected Doppler motion evidence missing from inspection");
    window.findChild<QTabWidget *>()->setCurrentIndex(2);
    window.show();
    QApplication::processEvents();
    require(!window.grab().isNull(), "compact observation inspection painting failed");
    window.clearRecord();
    require(!contains(QStringLiteral("Actual live signal result")), "cleared inspection retained prior evidence");
    PrpArchive replay(directory.path(), 4000, EPOCH_MS);
    PrpArchive::Event event;
    require(!replay.next(event) && !replay.isValid(), "mixed recording silently replayed PSR only");

    const QList<QVariant> values{ true, -123, quint32(0xfedcba98), qint64(-9007199254740993LL),
        QVariant::fromValue(std::numeric_limits<quint64>::max()), 1.2345678901234567,
        QStringLiteral("text"), QByteArray::fromHex("00ff8001"), QVariant::fromValue(short(-123)),
        QVariant::fromValue(ushort(65535)), QVariant::fromValue(char(-1)), QVariant::fromValue(uchar(255)),
        QVariant::fromValue(float(0.1234567)), QVariant::fromValue(static_cast<signed char>(-123)) };
    for (const auto &value : values) {
        QVariant decoded;
        error.clear();
        require(readOption(option(value, error), decoded, error) && decoded.userType() == value.userType()
                    && decoded == value, "typed scalar fidelity");
    }
    QVariant invalid;
    require(!readOption(QCborArray{ 3, qint64(-1) }, invalid, error), "negative uint option accepted");
    error.clear();
    require(!readOption(QCborArray{ qint64(1) << 40, 0 }, invalid, error), "narrowed type ID accepted");
}

void compactProducerNeutrality()
{
    using namespace Prp3Compact;
    struct Filter : PrefilterSSR
    {
        using PrefilterSSR::PrefilterSSR;
        void flush()
        {
            while (!areas.isEmpty())
                plotOut(areas.takeFirst());
        }
    };
    Logger::setFileLoggingEnabled(false);
    const QScopedPointer<NRadarMap> map(NRadarMap::createMap(60.0, 30.0));
    QTemporaryDir directory;
    const auto writer = std::make_shared<Prp3PlotLog>(directory.path(), Prp3PlotLog::MAX_FILE_BYTES, true,
                                                    Prp3PlotLog::Format::COMPACT);
    auto configuration = runInfo();
    configuration.insert(QStringLiteral("map_center"), QCborArray{ 60.0, 30.0 });
    writer->configure(configuration);
    const auto run = [&](bool recording) {
        Filter filter(map.data(), 1, 100, 1000, 60000, 0);
        if (recording)
            filter.setRecording(writer);
        QVector<QCborMap> outputs;
        QObject::connect(&filter, &PrefilterSSR::dataOut, &filter, [&outputs](const auto &plot) {
            QString error;
            outputs.append(snapshot(*plot, error));
            require(error.isEmpty(), "producer output snapshot failed");
        });
        for (int index = 0; index < 4; ++index) {
            const auto plot = QSharedPointer<NRadarPlot>::create(50,
                QDateTime::fromMSecsSinceEpoch(EPOCH_MS + index, Qt::UTC), map.data());
            plot->setSourceType(NRadarPlot::SSR);
            plot->setSSRType(index < 2 ? NRadarPlot::ChannelRBS : NRadarPlot::ModeS);
            plot->setADCoord({ 40.0, index == 3 ? 50.0 : 50000.0 });
            plot->setBoardNumber(01234);
            if (index > 0)
                plot->setHeight(3000.0);
            if (index > 1)
                plot->setOption(NRadarPlot::AircraftAddress, quint32(0xabcdef));
            filter.dataIn(plot);
        }
        filter.flush();
        if (recording)
            filter.finishRecording();
        return outputs;
    };
    const auto without = run(false), with = run(true);
    require(!without.isEmpty() && without == with && !writer->failed(), "recording changed MSSR preprocessing");
    const auto captured = read(filesIn(directory.path()));
    require(captured.plots.size() == 4 && captured.tracker->lives.isEmpty(), "producer dropped original observations");
    QVector<QCborMap> delivered;
    auto suppressed = 0;
    for (const auto &record : captured.plots) {
        require(record->preprocessingOutcomes.size() == 1, "producer outcome cardinality");
        const auto outcome = record->preprocessingOutcomes.first().toMap();
        if (record->preprocessingStatus == int(Outcome::FORWARDED))
            delivered.append(applyChanges(record->originalSnapshot, outcome.value(Key::CHANGES).toMap()));
        else
            suppressed += record->preprocessingStatus == int(Outcome::SUPPRESSED);
    }
    require(delivered == with && suppressed == 1, "recorded outcomes disagree with actual producer output");
    QMap<qint64, qint64> familyBytes;
    for (const auto &path : filesIn(directory.path())) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly) && file.seek(16), "open MSSR size fixture");
        while (!file.atEnd()) {
            const auto envelope = file.read(10);
            require(envelope.size() == 10, "MSSR size fixture envelope");
            const auto payload = file.read(qFromBigEndian<quint32>(envelope.constData() + 4));
            const auto record = QCborValue::fromCbor(payload).taggedValue().toMap();
            familyBytes[record.value(Key::KIND).toInteger()] += 10 + payload.size();
        }
    }
    qInfo("MSSR fixture: 4 originals %lld bytes, 4 outcomes %lld bytes, metadata %lld bytes (framed); "
          "3 forwarded, 1 suppressed", familyBytes.value(qint64(Kind::OBSERVATION)),
          familyBytes.value(qint64(Kind::OUTCOME)), familyBytes.value(qint64(Kind::RUN)));
}

void compactIndependentValidation()
{
    // Numeric IDs and framing are deliberately specified independently of the production writer.
    const QCborMap metadata{ { QStringLiteral("psr_only"), true }, { QStringLiteral("plot_only"), true },
        { QStringLiteral("clock_policy"), QStringLiteral("live_ingress") },
        { QStringLiteral("map_center"), QCborArray{ 60.0, 30.0 } },
        { QStringLiteral("producer"), QCborMap{ { QStringLiteral("application"), QStringLiteral("rdps3") },
                                               { QStringLiteral("build"), QStringLiteral("fixture") } } } };
    const QCborMap run{ { 0, 3 }, { 1, 0 }, { 16, 0 }, { 17, QStringLiteral("independent") }, { 15, metadata } };
    auto frame = QByteArray(58, 0);
    frame.replace(0, 4, QByteArray::fromHex("ffffff01"));
    frame[9] = 1;
    const QCborMap plot{ { 0, 3 }, { 1, 1 }, { 2, 1 }, { 3, 1 }, { 4, 51 }, { 5, EPOCH_MS }, { 6, 0 },
                         { 7, frame }, { 8, QByteArray{} }, { 11, 2 }, { 20, EPOCH_MS } };
    const QCborMap end{ { 0, 3 }, { 1, 5 }, { 2, 2 }, { 5, EPOCH_MS }, { 15, QCborArray{ 1, 0, 0 } } };
    const auto bytes = [](const QVector<QCborMap> &records) {
        QByteArray result(16, 0);
        result.replace(0, 8, QByteArrayLiteral("PRP3CBOR"));
        qToBigEndian<quint16>(1, result.data() + 8);
        qToBigEndian<quint16>(16, result.data() + 10);
        qToBigEndian<quint32>(300000000, result.data() + 12);
        for (const auto &record : records) {
            const auto payload = QCborValue(QCborTag(55799), record).toCbor();
            QByteArray envelope(10, 0);
            envelope.replace(0, 4, QByteArrayLiteral("P3R1"));
            qToBigEndian<quint32>(quint32(payload.size()), envelope.data() + 4);
            qToBigEndian<quint16>(qChecksum(payload.constData(), uint(payload.size()), Qt::ChecksumIso3309),
                                 envelope.data() + 8);
            result += envelope + payload;
        }
        return result;
    };
    QTemporaryDir directory;
    const auto check = [&](const QByteArray &payload, bool accepted) {
        QFile file(directory.filePath(QStringLiteral("fixture.prp3.cbor")));
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate)
                    && file.write(payload) == payload.size() && file.flush(), "write independent fixture");
        file.close();
        Prp3LogReader::Result result;
        QString error;
        const auto status = Prp3LogReader().read({ file.fileName() }, result, error);
        require((status == Prp3LogReader::Status::SUCCESS) == accepted, "independent compact validation");
        return result;
    };
    const auto valid = bytes({ run, plot, end });
    require(check(valid, true).plots.size() == 1, "independent compact plot missing");
    QByteArray extension(300, 0);
    extension.replace(0, 4, QByteArrayLiteral("DPS1"));
    qToBigEndian<quint16>(300, extension.data() + 4);
    extension[6] = 1;
    extension[7] = 26;
    qToBigEndian<quint16>(1, extension.data() + 8);
    extension[17] = 1;
    extension[19] = 2;
    extension[20] = 2;
    qToBigEndian<quint16>(136, extension.data() + 22);
    for (int branch = 0; branch < 2; ++branch) {
        const auto start = 26 + 136 * branch;
        extension[start] = char(branch);
        extension[start + 1] = char(branch ? 0x44 : 0x40);
        extension[start + 2] = 13;
        qToBigEndian<quint16>(0x1fff, extension.data() + start + 4);
        for (int slot = 0; slot < 13; ++slot) {
            qToBigEndian<quint32>(0x3f800001U + quint32(branch * 13 + slot), extension.data() + start + 6 + slot * 10);
            qToBigEndian<quint32>(0xbf800001U + quint32(branch * 13 + slot), extension.data() + start + 10 + slot * 10);
            qToBigEndian<quint16>(quint16(1340 + slot), extension.data() + start + 14 + slot * 10);
        }
    }
    qToBigEndian<quint16>(qChecksum(extension.constData(), 298), extension.data() + 298);
    frame[57] = char(0x80);
    auto signalPlot = plot;
    signalPlot.insert(7, frame + extension);
    const auto signal = check(bytes({ run, signalPlot, end }), true).plots.first();
    require(signal->inputFrame == frame + extension && signal->snapshot.decoded, "received sample bytes lost");
    for (int branch = 0; branch < 2; ++branch)
        for (int slot = 0; slot < 13; ++slot) {
            const auto &sample = signal->snapshot.evidence.snapshot.branches[branch].samples[slot];
            quint32 iBits = 0, qBits = 0;
            std::memcpy(&iBits, &sample.i, sizeof(iBits));
            std::memcpy(&qBits, &sample.q, sizeof(qBits));
            require(iBits == 0x3f800001U + quint32(branch * 13 + slot)
                        && qBits == 0xbf800001U + quint32(branch * 13 + slot)
                        && sample.followingIntervalUs == 1340 + slot, "sample precision or PRI changed");
        }
    extension[6] = 2;
    signalPlot.insert(7, frame + extension);
    const auto futureSignal = check(bytes({ run, signalPlot, end }), true).plots.first();
    require(futureSignal->inputFrame == frame + extension && !futureSignal->snapshot.decoded
                && futureSignal->snapshot.decodeError == quint8(PsrDoppler::DecodeError::BAD_VERSION),
            "future signal layout discarded or guessed");
    check(valid.left(valid.size() - 1), false);
    auto corrupt = valid;
    corrupt[corrupt.size() - 1] = char(quint8(corrupt.at(corrupt.size() - 1)) ^ 1);
    check(corrupt, false);
    auto optional = plot;
    optional.insert(1234, QStringLiteral("optional field"));
    check(bytes({ run, optional, end }), true);
    auto unsupported = plot;
    unsupported.insert(1, 6);
    check(bytes({ run, unsupported }), false);
    auto optionalEvent = plot;
    optionalEvent.insert(1, 128);
    require(!check(bytes({ run, optionalEvent }), true).tracker->warnings.isEmpty(), "optional coverage not qualified");
    auto gap = plot;
    gap.insert(2, 3);
    auto selected = run;
    selected.insert(16, 2);
    check(bytes({ selected, plot, gap }), false);
    auto future = plot;
    future.insert(0, 4);
    check(bytes({ run, future }), false);
    auto wrongCount = end;
    wrongCount.insert(15, QCborArray{ 2, 0, 0 });
    check(bytes({ run, plot, wrongCount }), false);
    require(!check(bytes({ run, plot }), true).tracker->warnings.isEmpty(), "open recording presented as complete");
}

void compactFailureAndSize()
{
    using namespace Prp3Compact;
    QTemporaryDir directory;
    auto configuration = runInfo();
    configuration.insert(QStringLiteral("map_center"), QCborArray{ 60.0, 30.0 });
    configuration.insert(QStringLiteral("psr_only"), true);
    QFile obstruction(directory.filePath(QStringLiteral("file-not-directory")));
    require(obstruction.open(QIODevice::WriteOnly), "create directory obstruction");
    obstruction.close();
    Prp3PlotLog blocked(obstruction.fileName(), Prp3PlotLog::MAX_FILE_BYTES, true, Prp3PlotLog::Format::COMPACT);
    require(blocked.failed(), "directory creation failure not exposed");

    Prp3PlotLog limited(directory.path(), 256, true, Prp3PlotLog::Format::COMPACT);
    limited.configure(configuration);
    limited.finishCompact();
    require(limited.failed(), "oversized metadata failure not exposed");
    Prp3LogReader::Result incomplete;
    QString error;
    require(Prp3LogReader().read(filesIn(directory.path()), incomplete, error) == Prp3LogReader::Status::SUCCESS
                && !incomplete.tracker->warnings.isEmpty(),
            "failed metadata write presented as a complete recording");

    Prp3PlotLog::PlotRecord plot;
    plot.sequence = 1;
    plot.legacyValid = true;
    plot.inputFrame = QByteArray(58, 0);
    plot.inputFrame.replace(0, 4, QByteArray::fromHex("ffffff01"));
    const auto rich = Prp3PlotLog::encodePlot(plot);
    auto retained = QCborValue::fromCbor(rich).taggedValue().toMap();
    auto restoration = retained.value(QStringLiteral("restoration")).toMap();
    for (const auto *key : { "branches", "coherent_energy", "fit_residual_rad", "ambiguity_margin",
                             "candidate_index", "candidate_index_scope", "branch_disagreement_hz" })
        restoration.remove(QLatin1String(key));
    retained.insert(QStringLiteral("restoration"), restoration);
    const auto withoutDetails = QCborValue(QCborTag(55799), retained).toCbor(QCborValue::UseFloat).size();
    qInfo("Size fixture (missing snapshot): legacy plot payload %d bytes; same legacy payload after deleting only "
          "excluded estimator details %d bytes; removed details %d bytes; no track-family bytes included",
          rich.size(), withoutDetails, rich.size() - withoutDetails);
}

void roundTripEvidenceAndUi(bool historical)
{
    QTemporaryDir directory;
    require(directory.isValid(), "temporary recording directory");
    Prp3PlotLog writer(directory.path());
    TentativeMotion::Evidence scorer;
    TentativeMotion::HypothesisParameters parameters;
    parameters.enabled = true;
    auto configuration = Prp3PlotLog::hypothesisConfiguration(parameters);
    if (historical)
        configuration.remove(QStringLiteral("trajectory_fit"));
    auto run = runInfo();
    run.insert(QStringLiteral("tracker"), QCborMap{ { QStringLiteral("hypotheses"), configuration } });
    writer.configure(run);
    TentativeMotion::Diagnostics final;
    for (int scan = 0; scan < 6; ++scan) {
        const auto time = EPOCH_MS / 1000.0 + scan * 4.0;
        TentativeMotion::Observation observation{
            { { scan * 400.0, 10000.0 } }, { { { { 10000.0, 1200.0 } }, { { 1200.0, 6400.0 } } } },
            time, 0, { quint64(scan + 30), 60.0 / 2e7, 1e6, 60, 20, 1, (1u << 20) - 1 }, 4.0, true };
        final = scorer.append(observation, 0.005, parameters);
        // Historical fixture values are recorded diagnostics, not a recalculated fit of these observations.
        if (historical && scan >= 2) {
            final.cvResidual = 12.5;
            final.cvReducedResidual = 12.5 / final.degreesOfFreedom;
            final.fittedPosition = { { 123.0, 456.0 } };
            final.fittedVelocity = { { 78.0, 9.0 } };
        }
        scorer.recordArrival(observation);
        Prp3PlotLog::PlotRecord plot;
        plot.arrivalUtcMs = qint64(time * 1000);
        plot.sequence = quint64(scan);
        plot.inputFrame = QByteArray(PsrDoppler::LEGACY_PLOT_SIZE, 0);
        plot.inputFrame.replace(0, 4, QByteArray::fromHex("ffffff01"));
        plot.azimuthDegrees = 90.0;
        writer.writePlot(plot, input(scan, plot.arrivalUtcMs, plot.arrivalUtcMs, plot.inputFrame));
        if (!scan)
            writer.writeRecord(QStringLiteral("track"), track(scan, time, QStringLiteral("seed")));
        auto evidence = track(scan, time, QStringLiteral("evidence"));
        evidence.insert(QStringLiteral("phase"), scan < 2 ? QStringLiteral("tentative") : QStringLiteral("post_confirm"));
        evidence.insert(QStringLiteral("evidence"), Prp3PlotLog::evidence(observation, final, true));
        writer.writeRecord(QStringLiteral("track"), evidence);
        if (scan)
            writer.writeRecord(QStringLiteral("track"), track(scan, time, QStringLiteral("update")));
        if (scan == 1)
            writer.writeRecord(QStringLiteral("track"), track(scan, time, QStringLiteral("confirm")));
    }
    writer.writeRecord(QStringLiteral("end"), {});
    require(!writer.failed(), "writer failure");
    const auto result = read(filesIn(directory.path()));
    require(result.plots.size() == 6 && result.tracker->lives.size() == 1, "round-trip cardinality");
    const auto life = result.tracker->lives.first();
    require(life->confirmation && life->lastScored, "missing scope snapshots");
    require(std::isnan(life->confirmation->metric(QStringLiteral("L_target_clutter"))), "future score at confirmation");
    require(life->lastScored->metric(QStringLiteral("L_target_clutter")) == final.hypotheses.targetVsClutter,
            "actual scorer H_T/H_C changed in round trip");
    const auto &recorded = life->lastScored->evidence;
    require(historical ? life->lastScored->metric(QStringLiteral("Jcv")) == final.cvResidual
                       : std::isnan(life->lastScored->metric(QStringLiteral("Jcv"))),
            "recorded fit availability or historical value changed");
    const auto metadata = result.tracker->runs.first().value(QStringLiteral("tracker")).toMap()
                                  .value(QStringLiteral("hypotheses")).toMap();
    require(historical ? !metadata.contains(QStringLiteral("trajectory_fit"))
                       : metadata.value(QStringLiteral("trajectory_fit")).toString()
                                == QStringLiteral("not-computed-online-1"), "fit provenance lost");
    for (const auto *key : { "Jcv", "Jcv_dof" })
        require(recorded.contains(QLatin1String(key))
                        && (historical ? recorded.value(QLatin1String(key)).toDouble() > 0.0
                                       : recorded.value(QLatin1String(key)).isNull()), "fit scalar contract changed");
    for (const auto *key : { "fit_xy", "fit_vxy" }) {
        const auto values = recorded.value(QLatin1String(key)).toArray();
        require(values.size() == 2 && (historical ? values.at(0).isDouble() && values.at(1).isDouble()
                                                 : values.at(0).isNull() && values.at(1).isNull()),
                "fit vector contract changed");
    }
    require(life->warnings.isEmpty(), "complete lifetime flagged invalid");
    const auto uses = result.tracker->inputUses.value(Prp3TrackerRecording::inputKey(QStringLiteral("test-run"), 5));
    require(uses.size() == 1, "source input join");
    TrackerEvidencePanel panel(nullptr);
    panel.setRecording(result.tracker);
    panel.inspect(uses);
    auto *scope = panel.findChild<QComboBox *>(QStringLiteral("trackerScope"));
    auto *availability = panel.findChild<QComboBox *>(QStringLiteral("trackerAvailability"));
    auto *colours = panel.findChild<QCheckBox *>(QStringLiteral("trackerColours"));
    require(scope && availability && colours, "evidence controls missing");
    const auto *minimum = panel.findChild<QDoubleSpinBox *>(QStringLiteral("trackerMinimum"));
    auto *maximum = panel.findChild<QDoubleSpinBox *>(QStringLiteral("trackerMaximum"));
    auto *range = panel.findChild<QCheckBox *>(QStringLiteral("trackerRange"));
    require(minimum && maximum && range, "range controls missing");
    availability->setCurrentIndex(1);
    require(panel.accepts(uses), "finite current observation rejected");
    scope->setCurrentIndex(1);
    require(!panel.accepts(uses), "confirmation filter borrowed a future score");
    scope->setCurrentIndex(2);
    require(panel.accepts(uses), "last-score lifetime filter");
    colours->setChecked(true);
    require(panel.colour(uses).isValid(), "score colour missing");
    range->setChecked(true);
    maximum->setValue(minimum->value() - 1.0);
    require(!panel.accepts(uses), "reversed numeric bounds accepted a score");
    range->setChecked(false);
    auto *metric = panel.findChild<QComboBox *>(QStringLiteral("trackerMetric"));
    require(metric, "metric selector missing");
    metric->setCurrentIndex(metric->findData(QStringLiteral("Jcv_dof")));
    for (const auto selectedScope : { 0, 2 }) {
        scope->setCurrentIndex(selectedScope);
        availability->setCurrentIndex(0);
        require(panel.accepts(uses), "unavailable fit hid an otherwise inspectable track");
        require(historical ? panel.colour(uses) != QColor(125, 125, 125)
                           : panel.colour(uses) == QColor(125, 125, 125), "fit availability colour incorrect");
        availability->setCurrentIndex(1);
        require(panel.accepts(uses) == historical, "finite-fit filter confused null and zero");
        availability->setCurrentIndex(2);
        require(panel.accepts(uses) != historical, "unavailable-fit filter incorrect");
        availability->setCurrentIndex(3);
        require(panel.accepts(uses), "unavailable fit invalidated new likelihood evidence");
    }
    const auto *history = panel.findChild<QTableWidget *>();
    require(history && history->rowCount() > 0
                    && (history->item(history->rowCount() - 1, 4)->text() == QStringLiteral("unavailable"))
                            != historical, "history misrepresented fit availability");
    require(historical || panel.summary(uses).contains(QStringLiteral("unavailable")), "fit summary invented zero");
    // Missing scalar keys remain malformed; a null value is the compatibility contract.
    for (const auto *key : { "Jcv", "Jcv_dof" }) {
        Prp3TrackerRecording invalid;
        QString error;
        require(invalid.append(result.tracker->runs.first(), error), "invalid-fixture run rejected");
        auto fields = life->lastScored->fields;
        fields.insert(QStringLiteral("record_sequence"), 1);
        auto missing = recorded;
        missing.remove(QLatin1String(key));
        fields.insert(QStringLiteral("evidence"), missing);
        require(!invalid.append(fields, error) && error.contains(QLatin1String(key)), "missing fit key accepted");
    }
    metric->setCurrentIndex(0);
    availability->setCurrentIndex(1);
    auto other = uses.first();
    const auto otherLife = QSharedPointer<Prp3TrackerLife>::create(*life);
    otherLife->table = 100;
    otherLife->confirmation.clear();
    other.life = otherLife;
    scope->setCurrentIndex(1);
    require(panel.colour({ uses.first(), other }) == QColor(125, 125, 125), "ambiguous source not grey");
    panel.resize(1200, 700);
    panel.show();
    QApplication::processEvents();
    require(!panel.grab().isNull(), "evidence panel/chart painting failed");
    panel.setRecording({});
    panel.inspect({});
    require(!panel.accepts({}), "finite-only filter accepted missing evidence");
}

void carriedObservationEvidence()
{
    const auto recording = QSharedPointer<Prp3TrackerRecording>::create();
    QString error;
    auto run = runInfo();
    run.insert(QStringLiteral("record_type"), QStringLiteral("run"));
    run.insert(QStringLiteral("part"), 0);
    require(recording->append(run, error), "carry fixture run rejected");
    qint64 sequence = 0;
    const auto append = [&](QCborMap fields) {
        fields.insert(QStringLiteral("run_id"), QStringLiteral("test-run"));
        fields.insert(QStringLiteral("record_type"), QStringLiteral("track"));
        fields.insert(QStringLiteral("record_sequence"), ++sequence);
        require(recording->append(fields, error), "carry fixture track rejected");
    };
    const auto evidence = [&](double time, QCborValue value, bool updated) {
        auto fields = track(-1, time, QStringLiteral("evidence"));
        QCborMap metrics;
        for (const auto *key : { "samples", "distinct_scans", "scored_hits", "hypothesis_hits",
                                 "nominal_misses_scored", "nominal_misses_unscored", "dof" })
            metrics.insert(QLatin1String(key), 0);
        for (const auto *key : { "Jcv", "Jcv_dof", "NIS", "L_target_poisson", "L_persistent_poisson",
                                 "L_clutter_poisson" })
            metrics.insert(QLatin1String(key), nullptr);
        metrics.insert(QStringLiteral("L_target_clutter"), value);
        metrics.insert(QStringLiteral("hypothesis_enabled"), true);
        metrics.insert(QStringLiteral("hypothesis_evaluated"), updated);
        metrics.insert(QStringLiteral("score_updated"), updated);
        metrics.insert(QStringLiteral("score_available"), !value.isNull());
        metrics.insert(QStringLiteral("hypothesis_reason"), updated ? QStringLiteral("scored")
                                                                   : QStringLiteral("not_evaluated"));
        fields.insert(QStringLiteral("evidence"), metrics);
        append(fields);
    };
    append(track(-1, 0.0, QStringLiteral("seed")));
    evidence(1.0, 2.0, true);
    append(track(-1, 1.0, QStringLiteral("update")));
    append(track(-1, 2.0, QStringLiteral("update")));
    evidence(3.0, nullptr, false);
    append(track(-1, 3.0, QStringLiteral("update")));
    evidence(4.0, 12.0, true);
    append(track(-1, 4.0, QStringLiteral("update")));
    append(track(-1, 5.0, QStringLiteral("update")));
    append(track(-1, 5.0, QStringLiteral("confirm")));
    append(track(999, 6.0, QStringLiteral("update")));
    append(track(-1, 3.5, QStringLiteral("update")));
    append(track(-1, 7.0, QStringLiteral("update")));
    append(track(-1, 8.0, QStringLiteral("seed"), 51, 2));
    append(track(-1, 8.0, QStringLiteral("seed"), 100));
    recording->resolve();
    const auto life = recording->lives.value(Prp3TrackerRecording::lifeKey(QStringLiteral("test-run"), 51, 1));
    const auto uses = [&life](int index) { return QVector<Prp3TrackerUse>{ { life, life->samples.at(index) } }; };
    TrackerEvidencePanel panel(nullptr);
    panel.setRecording(recording);
    auto *colours = panel.findChild<QCheckBox *>(QStringLiteral("trackerColours"));
    auto *availability = panel.findChild<QComboBox *>(QStringLiteral("trackerAvailability"));
    auto *scope = panel.findChild<QComboBox *>(QStringLiteral("trackerScope"));
    auto *metric = panel.findChild<QComboBox *>(QStringLiteral("trackerMetric"));
    auto *range = panel.findChild<QCheckBox *>(QStringLiteral("trackerRange"));
    auto *minimum = panel.findChild<QDoubleSpinBox *>(QStringLiteral("trackerMinimum"));
    auto *maximum = panel.findChild<QDoubleSpinBox *>(QStringLiteral("trackerMaximum"));
    colours->setChecked(true);
    const auto grey = QColor(125, 125, 125);
    require(panel.colour(uses(0)) == grey, "seed borrowed future evidence");
    require(panel.colour(uses(2)) != grey && panel.colour(uses(2)) == panel.colour(uses(3))
                    && panel.colour(uses(3)) == panel.colour(uses(5)), "unchanged points lost preceding colour");
    require(panel.colour(uses(7)) != panel.colour(uses(3)) && panel.colour(uses(7)) == panel.colour(uses(8)),
            "new score did not replace the carried colour");
    require(panel.colour(uses(10)) == grey && panel.colour(uses(11)) == grey,
            "invalid sample or regressing time borrowed evidence");
    require(panel.colour(uses(12)) == panel.colour(uses(7)), "valid later point failed to retain last score");
    for (const auto &other : std::as_const(recording->lives)) {
        if (other == life)
            continue;
        require(panel.colour({ { other, other->samples.first() } }) == grey, "carry crossed table/lifetime boundary");
    }
    require(panel.colour({ uses(3).first(), uses(8).first() }) == grey, "carry hid ambiguous uses");
    availability->setCurrentIndex(1);
    require(panel.accepts(uses(3)), "finite filter rejected carried score");
    availability->setCurrentIndex(2);
    require(!panel.accepts(uses(3)), "unavailable filter accepted carried score");
    availability->setCurrentIndex(3);
    require(!panel.accepts(uses(3)) && panel.accepts(uses(2)), "carry fabricated a score update");
    availability->setCurrentIndex(0);
    minimum->setValue(1.0);
    maximum->setValue(3.0);
    range->setChecked(true);
    require(panel.accepts(uses(3)) && !panel.accepts(uses(8)), "range filter ignored carried values");
    range->setChecked(false);
    metric->setCurrentIndex(metric->findData(QStringLiteral("Jcv")));
    require(panel.colour(uses(3)) == grey, "H_T/H_C carry fabricated another metric");
    metric->setCurrentIndex(0);
    scope->setCurrentIndex(1);
    require(panel.colour(uses(8)) == grey, "confirmation scope changed recorded availability");
    scope->setCurrentIndex(2);
    require(panel.colour(uses(0)) == panel.colour(uses(8)), "last-score scope changed");
    scope->setCurrentIndex(0);
    panel.inspect(uses(3));
    require(std::isnan(life->samples.at(3)->metric(QStringLiteral("L_target_clutter")))
                    && !life->samples.at(3)->updated() && life->samples.at(3)->carriedScoreTime == 1.0,
            "carry overwrote raw evidence or lost provenance");
    const auto *history = panel.findChild<QTableWidget *>();
    require(history->item(3, 3)->text() == QStringLiteral("unavailable"), "history fabricated recorded evidence");
}

void explicitScheduleAndLegacy()
{
    QTemporaryDir directory;
    Prp3PlotLog writer(directory.path());
    writer.configure(runInfo());
    const auto north = QByteArray::fromHex("ffffff0200");
    const auto sector = QByteArray::fromHex("ffffff0201");
    const QVector<QCborMap> schedule{
        input(0, EPOCH_MS, EPOCH_MS, north), input(1, EPOCH_MS, EPOCH_MS, north, false),
        input(2, EPOCH_MS + 4000, EPOCH_MS + 4000, north),
        input(3, EPOCH_MS + 3990, EPOCH_MS + 4000, sector) };
    for (const auto &event : schedule)
        writer.writeRecord(QStringLiteral("input"), { { QStringLiteral("input"), event } });
    writer.writeRecord(QStringLiteral("end"), {});
    PrpArchive archive(directory.path(), 4000, EPOCH_MS);
    archive.retainSuppressedInputs(true);
    PrpArchive::Event event;
    int index = 0;
    while (archive.next(event)) {
        require(index < schedule.size(), "invented input/marker on v2 replay");
        const auto expected = schedule.at(index++);
        require(event.processingTimeMs == expected.value(QStringLiteral("processing_ms")).toInteger()
                        && event.measurementTimeMs == expected.value(QStringLiteral("measurement_ms")).toInteger(),
                "explicit clocks reinterpreted");
        require(event.inputOrdinal == expected.value(QStringLiteral("source_ordinal")).toInteger()
                        && event.fileIndex == 2 && event.fileOrdinal == index - 1 + 100,
                "original source ordinals lost");
        require(event.delivered == expected.value(QStringLiteral("delivered")).toBool(), "marker redelivered");
    }
    require(archive.isValid() && index == 4 && archive.stats().backwardMeasurements == 1, "schedule replay failed");
    QTemporaryDir oldDirectory;
    Prp3PlotLog oldWriter(oldDirectory.path());
    Prp3PlotLog::PlotRecord plot;
    plot.inputFrame = QByteArray(PsrDoppler::LEGACY_PLOT_SIZE, 0);
    plot.inputFrame.replace(0, 4, QByteArray::fromHex("ffffff01"));
    plot.azimuthDegrees = 90.0;
    oldWriter.writePlot(plot);
    const auto old = read(filesIn(oldDirectory.path()));
    require(old.plots.size() == 1 && old.tracker->lives.isEmpty(), "legacy schema import regressed");
    QFile file(filesIn(oldDirectory.path()).first());
    require(file.open(QIODevice::ReadWrite), "open corruption fixture");
    require(file.seek(file.size() - 1) && file.write(QByteArray(1, char(0xff))) == 1 && file.flush(), "corrupt fixture");
    Prp3LogReader::Result invalid;
    QString error;
    require(Prp3LogReader().read(filesIn(oldDirectory.path()), invalid, error) == Prp3LogReader::Status::ERROR
                    && invalid.plots.isEmpty(), "corrupt record silently accepted or partially imported");
}

void rotationLifetimesAndInvalidRecords()
{
    QTemporaryDir directory;
    Prp3PlotLog writer(directory.path(), 4000);
    writer.configure(runInfo());
    const auto north = QByteArray::fromHex("ffffff0200");
    const auto seconds = EPOCH_MS / 1000.0;
    writer.writeRecord(QStringLiteral("input"), { { QStringLiteral("input"), input(0, EPOCH_MS, EPOCH_MS, north) } });
    writer.writeRecord(QStringLiteral("track"), track(0, seconds, QStringLiteral("seed")));
    writer.writeRecord(QStringLiteral("track"), track(0, seconds, QStringLiteral("seed"), 100));
    writer.writeRecord(QStringLiteral("track"), track(0, seconds, QStringLiteral("free")));
    writer.writeRecord(QStringLiteral("track"), track(0, seconds, QStringLiteral("output")));
    writer.writeRecord(QStringLiteral("track"), track(0, seconds, QStringLiteral("update")));
    writer.writeRecord(QStringLiteral("track"), track(0, seconds, QStringLiteral("seed"), 51, 2));
    writer.writeRecord(QStringLiteral("track"), track(999, seconds + 1, QStringLiteral("update"), 51, 2));
    for (int i = 1; i < 20; ++i)
        writer.writeRecord(QStringLiteral("input"),
                           { { QStringLiteral("input"), input(i, EPOCH_MS + i, EPOCH_MS + i, north) } });
    writer.writeRecord(QStringLiteral("end"), {});
    require(!writer.failed(), "rotating writer failure");
    const auto files = filesIn(directory.path());
    require(files.size() > 2, "rotation fixture failed to rotate");
    const auto complete = read(files);
    require(complete.tracker->lives.size() == 3, "slot reuse/table identity collapsed lifetimes");
    require(complete.tracker->inputUses.value(Prp3TrackerRecording::inputKey(QStringLiteral("test-run"), 0)).size() == 4,
            "one-to-many input references lost");
    const auto old = complete.tracker->lives.value(Prp3TrackerRecording::lifeKey(QStringLiteral("test-run"), 51, 1));
    const auto reused = complete.tracker->lives.value(Prp3TrackerRecording::lifeKey(QStringLiteral("test-run"), 51, 2));
    require(old->warnings.size() == 2 && reused->warnings.size() == 1, "lifecycle/missing-reference warnings absent");
    const auto partial = read(files.mid(1));
    require(!partial.tracker->warnings.isEmpty(), "partial part selection presented as complete");
    PrpArchive archive(directory.path(), 4000, EPOCH_MS);
    PrpArchive::Event event;
    int count = 0;
    while (archive.next(event))
        ++count;
    require(archive.isValid() && count == 20, "rotated archive changed input stream");

    const auto rejectInput = [&north](const QString &key, const QCborValue &value) {
        QTemporaryDir malformed;
        Prp3PlotLog log(malformed.path());
        log.configure(runInfo());
        auto fields = input(0, EPOCH_MS, EPOCH_MS, north);
        fields.insert(key, value);
        log.writeRecord(QStringLiteral("input"), { { QStringLiteral("input"), fields } });
        log.writeRecord(QStringLiteral("end"), {});
        Prp3LogReader::Result result;
        QString error;
        require(Prp3LogReader().read(filesIn(malformed.path()), result, error) == Prp3LogReader::Status::ERROR
                        && result.plots.isEmpty(), "malformed v2 input accepted by Analyzer");
        PrpArchive invalid(malformed.path(), 4000, EPOCH_MS);
        PrpArchive::Event ignored;
        while (invalid.next(ignored)) {}
        require(!invalid.isValid(), "malformed v2 input accepted by replay");
    };
    rejectInput(QStringLiteral("channel"), qint64(1) << 40);
    rejectInput(QStringLiteral("processing_ms"), std::numeric_limits<qint64>::max());
    rejectInput(QStringLiteral("file"), -1);
    rejectInput(QStringLiteral("north"), false);
    rejectInput(QStringLiteral("frame"), QByteArray(2, 0));

    QTemporaryDir unsupported;
    QFile original(files.first());
    require(original.open(QIODevice::ReadOnly), "open unsupported-version fixture source");
    const auto bytes = original.readAll();
    const auto oldSize = qFromBigEndian<quint32>(bytes.constData() + 20);
    auto run = QCborValue::fromCbor(bytes.mid(26, int(oldSize))).taggedValue().toMap();
    run.insert(QStringLiteral("schema_version"), 99);
    const auto payload = QCborValue(QCborTag(55799), run).toCbor();
    QByteArray envelope(10, 0);
    envelope.replace(0, 4, QByteArrayLiteral("P3R1"));
    qToBigEndian<quint32>(quint32(payload.size()), envelope.data() + 4);
    qToBigEndian<quint16>(qChecksum(payload.constData(), uint(payload.size()), Qt::ChecksumIso3309),
                         envelope.data() + 8);
    QFile badVersion(unsupported.filePath(QStringLiteral("future.prp3.cbor")));
    require(badVersion.open(QIODevice::WriteOnly), "create unsupported-version fixture");
    const auto changed = bytes.left(16) + envelope + payload + bytes.mid(26 + int(oldSize));
    require(badVersion.write(changed) == changed.size() && badVersion.flush(), "write unsupported-version fixture");
    Prp3LogReader::Result result;
    QString error;
    require(Prp3LogReader().read({ badVersion.fileName() }, result, error) == Prp3LogReader::Status::ERROR,
            "unsupported payload schema accepted by Analyzer");
    PrpArchive future(badVersion.fileName(), 4000, EPOCH_MS);
    require(!future.next(event) && !future.isValid(), "unsupported payload schema accepted by replay");
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    try {
        if (argc > 1) {
            const QString source = QString::fromLocal8Bit(argv[1]);
            const auto result = read(QFileInfo(source).isDir() ? filesIn(source) : QStringList{ source });
            qint64 samples = 0, finite = 0, confirmationFinite = 0, anomalous = 0;
            for (const auto &life : result.tracker->lives) {
                samples += life->samples.size();
                anomalous += !life->warnings.isEmpty();
                confirmationFinite += life->confirmation
                        && std::isfinite(life->confirmation->metric(QStringLiteral("L_target_clutter")));
                for (const auto &sample : life->samples)
                    finite += sample->event == QStringLiteral("evidence")
                            && std::isfinite(sample->metric(QStringLiteral("L_target_clutter")));
            }
            qInfo("Import: %d plots, %d lifetimes, %lld events, %lld finite H_T/H_C, %lld finite confirmations, "
                  "%lld warned lifetimes, %d coverage warnings", result.plots.size(), result.tracker->lives.size(),
                  samples, finite, confirmationFinite, anomalous, result.tracker->warnings.size());
            return 0;
        }
        roundTripEvidenceAndUi(false);
        compactPlotsAndOutcomes();
        compactProducerNeutrality();
        compactIndependentValidation();
        compactFailureAndSize();
        roundTripEvidenceAndUi(true);
        carriedObservationEvidence();
        explicitScheduleAndLegacy();
        rotationLifetimesAndInvalidRecords();
        qInfo("PRP3 producer/consumer, exact scores, scopes, UI filters, clocks, markers and legacy checks passed");
        return 0;
    } catch (const std::exception &error) {
        qCritical("%s", error.what());
        return 1;
    }
}
