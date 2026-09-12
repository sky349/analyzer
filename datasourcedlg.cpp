

#include <libradardatabase/rdbfolder.h>
#include <version.h>
#include <asterix/asterix.h>
#include <libradardata/nasterixconverter.h>
#include <libradardata/nradarplot.h>
#include <libradarmap/nradarmap.h>

#include "datasourcedlg.h"
#include "datapack.h"
#include "prp3logreader.h"

#include <QDir>
#include <QFileInfo>

#include <cmath>

#define DEFAULT_DB_PATH NRPL_MK_PATH("var/rdps/rdb")

namespace
{
constexpr int PRP3_DISPLAY_RADAR_ID = 1;

bool isPrp3File(const QString &path)
{
    return path.endsWith(QLatin1String(".prp3.cbor"), Qt::CaseInsensitive);
}

bool looksLikeLegacyPrp(const QString &path)
{
    const auto leaf = QFileInfo(path).fileName().toLower();
    return leaf.endsWith(QLatin1String(".prp")) || leaf.endsWith(QLatin1String(".prp1"))
        || leaf.endsWith(QLatin1String(".prp2")) || leaf.contains(QLatin1String(".prp1."))
        || leaf.contains(QLatin1String(".prp2.")) || leaf.endsWith(QLatin1String("_i.txt"))
        || leaf.endsWith(QLatin1String("_o.txt"));
}

QStringList prp3FilesIn(const QString &path)
{
    const QDir directory(path);
    const auto leaves = directory.entryList({ QStringLiteral("*.prp3.cbor") }, QDir::Files | QDir::Readable,
                                            QDir::Name);
    QStringList files;
    files.reserve(leaves.size());
    for (const auto &leaf : leaves)
        files.append(directory.absoluteFilePath(leaf));
    return files;
}

void applyPrp3Options(NRadarPlot &plot, const Prp3PlotRecord &record)
{
    const auto backgroundFlags = (record.background.observed ? 0x01 : 0)
        | (record.background.passed ? 0x02 : 0) | (record.background.warmup ? 0x04 : 0)
        | (record.background.historyDegraded ? 0x08 : 0);
    plot.setOption(NRadarPlot::RAW_PSR_FILTER_VALUE, record.filter.value);
    plot.setOption(NRadarPlot::RAW_PSR_BG_FLAGS, backgroundFlags);
    plot.setOption(NRadarPlot::RAW_PSR_BG_THRESHOLD, record.background.threshold);
    plot.setOption(NRadarPlot::RAW_PSR_AMPLITUDE, record.legacy.amplitude);
    if (record.legacy.cpiGroupCount >= 1 && record.legacy.cpiGroupCount <= 15)
        plot.setOption(NRadarPlot::RAW_PSR_CPI_GROUP_COUNT, record.legacy.cpiGroupCount);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_FLAGS, record.restoration.flags);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_REASON, record.restoration.reason);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_BRANCH_MASK, record.restoration.branchMask);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_FREQUENCY_HZ, record.restoration.frequencyHz);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_ENERGY, record.restoration.coherentEnergy);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_FIT_RESIDUAL, record.restoration.fitResidualRad);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_AMBIGUITY_MARGIN, record.restoration.ambiguityMargin);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_AMBIGUITY_INDEX, record.restoration.candidateIndex);
    plot.setOption(NRadarPlot::RAW_PSR_DOPPLER_BRANCH_DISAGREEMENT_HZ,
                   record.restoration.branchDisagreementHz);
    plot.setOption(NRadarPlot::RAW_PSR_RADIAL_SPEED_MPS, record.restoration.radialSpeedMps);
    for (const auto &branch : record.restoration.branches)
        plot.setOption(branch.slot ? NRadarPlot::RAW_PSR_DOPPLER_BRANCH_B_HZ
                                   : NRadarPlot::RAW_PSR_DOPPLER_BRANCH_A_HZ,
                       branch.frequencyHz);
    if (record.filter.dropPlot)
        plot.setOption(NRadarPlot::TestPlot, true);
}
}

DataSourceDlg::DataSourceDlg(DataPack *dataPack,NRadarMap *map,QWidget *parent):QDialog(parent),reader(0)
{
    setupUi(this);

    connect(btnImport,SIGNAL(clicked()),this,SLOT(onImport()));
    connect(btnAbort,SIGNAL(clicked()),this,SLOT(onAbortImport()));

    connect(btnBrowseFile,SIGNAL(clicked()),this,SLOT(onBrowseFile()));
    connect(btnBrowseFolder,SIGNAL(clicked()),this,SLOT(onBrowseFolder()));

    converter=new NAsterixConverter(map);
    this->dataPack=dataPack;

    shiftSSRAlt = QCoreApplication::arguments().contains("-1000");
}

DataSourceDlg::~DataSourceDlg()
{
    if(reader) delete reader;
    if(converter) delete converter;
}

QPointF DataSourceDlg::getMapCenter() const
{
    QPointF predicted=adsbBoundingRect.center();

    double lat,lon;
    if(!NRadarMap::readGeoString(edCenter->text(),&lat,&lon))
        return predicted;

    QPointF defined(lat,lon);

    if(predicted.isNull()) return defined;

    if(QLineF(predicted,defined).length() > 5.0) return predicted;

    return defined;
}

void DataSourceDlg::onImport()
{
    stackActions->setCurrentIndex(1);
    progressImport->setValue(0);
    qApp->processEvents();

    abortRead=false;
    adsbBoundingRect=QRectF();
    estimateCenterMode=true;

    converter->setCenterPoint(getMapCenter());

    if (m_importType == ImportType::PRP3) {
        const bool imported = importPrp3();
        stackActions->setCurrentIndex(0);
        if (imported)
            accept();
        return;
    }

    if (m_importType == ImportType::RDB && reader) {
        QDateTime dtFrom,dtTo;
        dtFrom=dateFrom->dateTime();
        dtFrom.setTimeSpec(Qt::UTC);
        dtTo=dateTo->dateTime();
        dtTo.setTimeSpec(Qt::UTC);

        msecsStart=dtFrom.toMSecsSinceEpoch();
        msecsEnd=dtTo.toMSecsSinceEpoch();

        progressImportUpdate.start();

        reader->readAll(dtFrom,dtTo,this,"dataIn");

        finishUpdateMapCenter();

        dataPack->begin=dtFrom;
        dataPack->end=dtTo;
        dataPack->center=getMapCenter();

        stackActions->setCurrentIndex(0);

        accept();
        return;
    }

    if (m_importType == ImportType::ASTERIX && radioFile->isChecked()) {
        QFile input(edFile->text());
        if (!input.exists() || !input.open(QIODevice::ReadOnly)) {
            stackActions->setCurrentIndex(0);
            QMessageBox::critical(this, tr("Data import"),
                                  tr("Cannot open %1: %2").arg(input.fileName(), input.errorString()));
            return;
        }

        QByteArray data=input.readAll();
        input.close();

        const bool res = !data.isEmpty() && processAsterix(data);
        stackActions->setCurrentIndex(0);
        if (!res) {
            QMessageBox::critical(this, tr("Data import"), tr("The file contains no supported ASTERIX plots."));
            return;
        }

        dataPack->begin=dataPack->data.first()->getTime();
        dataPack->end=dataPack->data.last()->getTime();
        dataPack->center=getMapCenter();

        accept();
        return;
    }

    stackActions->setCurrentIndex(0);
    QMessageBox::warning(this, tr("Data import"), tr("Choose an RDB, ASTERIX, or framed PRP3 source first."));
}

void DataSourceDlg::onAbortImport()
{
    abortRead=true;
}

void DataSourceDlg::onBrowseFile()
{
    radioFile->setChecked(true);

    QString start=edFile->text();
    QString path = QFileDialog::getOpenFileName(
        this, tr("Choose PRP3, RDB, or ASTERIX data"), start,
        tr("Supported data (*.prp3.cbor *.rdb);;PRP3 Doppler logs (*.prp3.cbor);;"
           "RDB files (*.rdb);;ASTERIX dumps (*)"));
    if(path.isNull()) return;

    edFile->setText(path);
    if (isPrp3File(path)) {
        initPrp3Reader({ path });
        return;
    }
    if (looksLikeLegacyPrp(path)) {
        m_importType = ImportType::NONE;
        m_prp3Files.clear();
        if (reader) {
            delete reader;
            reader = nullptr;
        }
        dateFrom->setEnabled(false);
        dateTo->setEnabled(false);
        QMessageBox::warning(this, tr("Unsupported PRP format"),
                             tr("Only Doppler-capable framed *.prp3.cbor logs are supported; PRP1 and PRP2 are not."));
        return;
    }
    if (path.endsWith(QLatin1String(".rdb"), Qt::CaseInsensitive)) {
        initRDBReader(path, true);
        return;
    }

    if (reader) {
        delete reader;
        reader = nullptr;
    }
    m_prp3Files.clear();
    m_importType = ImportType::ASTERIX;
    dateFrom->setEnabled(false);
    dateTo->setEnabled(false);
}

void DataSourceDlg::onBrowseFolder()
{
    radioFolder->setChecked(true);

    QString start=edFolder->text();
    if(!start.length()) start=DEFAULT_DB_PATH;
    QString path = QFileDialog::getExistingDirectory(this, tr("Choose PRP3 or RDB files folder"), start);
    if(path.isNull()) return;

    edFolder->setText(path);

    const auto prp3Files = prp3FilesIn(path);
    prp3Files.isEmpty() ? initRDBReader(path, false) : initPrp3Reader(prp3Files);
}

void DataSourceDlg::initRDBReader(const QString& path,bool isFile)
{
    if(reader) delete reader;
    reader = nullptr;
    m_prp3Files.clear();
    m_importType = ImportType::NONE;

    reader = !isFile ? new RDBFolderReader(path) : new RDBFolderReader(QStringList({path}));
    if(reader->isEmpty())
    {
        if(!isFile) qDebug("Folder '%s' doesn't contain valid RDB files.",qPrintable(reader->getPath()));
        else qDebug("File '%s' is not a valid RDB file.",qPrintable(path));

        dateFrom->setEnabled(false);
        dateTo->setEnabled(false);
        delete reader;
        reader=0;
        return;
    }

    QDateTime tm1,tm2;

    dateFrom->setEnabled(true);
    dateTo->setEnabled(true);

    reader->getRange(tm1,tm2);
    dateFrom->setDateTime(tm1);
    dateTo->setDateTime(tm2);
    m_importType = ImportType::RDB;
}

void DataSourceDlg::initPrp3Reader(const QStringList &paths)
{
    if (reader) {
        delete reader;
        reader = nullptr;
    }
    m_prp3Files = paths;
    m_importType = m_prp3Files.isEmpty() ? ImportType::NONE : ImportType::PRP3;
    dateFrom->setEnabled(false);
    dateTo->setEnabled(false);
}

bool DataSourceDlg::importPrp3()
{
    Prp3LogReader logReader;
    Prp3LogReader::Result result;
    QString error;
    progressImportUpdate.start();
    const auto status = logReader.read(m_prp3Files, result, error, [this](qint64 completed, qint64 total) {
        progressImport->setValue(total ? int(90.0 * completed / total) : 0);
        if (progressImportUpdate.elapsed() > 100) {
            qApp->processEvents();
            progressImportUpdate.restart();
        }
        return !abortRead;
    });
    if (status == Prp3LogReader::Status::CANCELLED)
        return false;
    if (status == Prp3LogReader::Status::ERROR) {
        QMessageBox::critical(this, tr("PRP3 import failed"), error);
        return false;
    }
    if (result.plots.isEmpty()) {
        QMessageBox::critical(this, tr("PRP3 import failed"),
                              tr("The selected PRP3 source contains no plot records."));
        return false;
    }

    dataPack->clear();
    for (int plotIndex = 0; plotIndex < result.plots.size(); ++plotIndex) {
        if (progressImportUpdate.elapsed() > 100) {
            qApp->processEvents();
            progressImportUpdate.restart();
            if (abortRead) {
                dataPack->clear();
                return false;
            }
        }

        const auto &record = result.plots.at(plotIndex);
        const auto arrival = QDateTime::fromMSecsSinceEpoch(record->arrivalUtcMs, Qt::UTC);
        int consumed = -1;
        const auto decoded = record->outputApoi.isEmpty()
            ? QSharedPointer<NRadarAbstractPlot>()
            : converter->convertFromAPOI(record->outputApoi, PRP3_DISPLAY_RADAR_ID, consumed, arrival);
        auto plot = !decoded.isNull() && consumed == record->outputApoi.size()
                && decoded->getType() == NRadarAbstractPlot::TypePlot
            ? qSharedPointerDynamicCast<NRadarPlot>(decoded)
            : QSharedPointer<NRadarPlot>();
        if (plot.isNull()) {
            plot = QSharedPointer<NRadarPlot>::create(PRP3_DISPLAY_RADAR_ID, arrival, converter->getRadarMap());
            plot->setSourceType(NRadarPlot::PSR);
            plot->setPlotAssociation(NRadarPlot::NotAssociated);
            plot->setADCoord(QPointF(std::fmod(record->legacy.azimuthRaw * 360.0 / 16384.0, 360.0),
                                    record->legacy.rangeKm * 1000.0));
        }
        applyPrp3Options(*plot, *record);

        dataPack->data.append(plot.data());
        dataPack->savedData.append(plot);
        dataPack->prp3Data.insert(plot.data(), record);
        progressImport->setValue(90 + int(10.0 * (plotIndex + 1) / result.plots.size()));
    }

    dataPack->begin = result.begin;
    dataPack->end = result.end;
    dataPack->center = getMapCenter();
    qInfo() << "Imported PRP3 plots/events:" << result.plots.size() << result.eventCount;
    return true;
}

bool DataSourceDlg::processAsterix(const QByteArray& data)
{
    const quint8* buf=(const quint8*)data.data();
    int err,len,pos=0;

    QDateTime dt=QDateTime::currentDateTimeUtc();
    lastProcessedTime=0;

    while(1)
    {
        len=0;

        int shiftDate=0;

        switch(buf[0])
        {
        case 34: {
            Asterix_34 pkt;
            if((len=pkt.read(buf,data.size(),&err)) && err==0)
                process(&pkt,dt,&shiftDate);
        } break;
        case 48: {
            Asterix_48 pkt;
            if((len=pkt.read(buf,data.size(),&err)) && err==0)
                process(&pkt,dt,&shiftDate);
        } break;
        case 62: {
            Asterix_62 pkt;
            if((len=pkt.read(buf,data.size(),&err)) && err==0)
                process(&pkt,dt,&shiftDate);
        } break;
        case 21: {
            Asterix_21 pkt13;
            Asterix_21_023 pkt023;
            if((len=pkt13.read(buf,data.size(),&err)) && err==0)
            {
                if(estimateCenterMode) updateMapCenter(&pkt13);
                process(&pkt13,dt,&shiftDate);
            }
            else if((len=pkt023.read(buf,data.size(),&err)) && err==0)
            {
                if(estimateCenterMode) updateMapCenter(&pkt023);
                process(&pkt023,dt,&shiftDate);
            }
        } break;

        default: break;
        }

        if(!len || pos+5>=data.size())
            break;

        if(shiftDate)
        {
            dt=dt.addDays(shiftDate);
            continue;
        }

        pos+=len;
        buf+=len;

        progressImport->setValue((qint64)pos*100/data.size());
    }
    return (dataPack->data.size()>0);
}

bool DataSourceDlg::dataIn(qint64 msecs,const QByteArray& data)
{
    progressImport->setValue((msecs-msecsStart)*100/(msecsEnd-msecsStart));
    if(progressImportUpdate.elapsed()>100)
    {
        qApp->processEvents();
        progressImportUpdate.restart();

        if(abortRead)
            return false;
    }

    QDateTime dt=QDateTime::fromMSecsSinceEpoch(msecs).toUTC();

    const quint8* buf=(const quint8*)data.data();
    int err;
    switch(buf[0])
    {
    case 34: {
        Asterix_34 pkt;
        if(pkt.read(buf,data.size(),&err) && err==0)
            process(&pkt,dt);
        } break;
    case 48: {
        Asterix_48 pkt;
        if(pkt.read(buf,data.size(),&err) && err==0)
            process(&pkt,dt);
        } break;
    case 62: {
        Asterix_62 pkt;
        if(pkt.read(buf,data.size(),&err) && err==0)
            process(&pkt,dt);
        } break;
    case 21: {
        Asterix_21 pkt13;
        Asterix_21_023 pkt023;
        if(pkt13.read(buf,data.size(),&err) && err==0)
        {
            if(estimateCenterMode) updateMapCenter(&pkt13);
            process(&pkt13,dt);
        }
        else if(pkt023.read(buf,data.size(),&err) && err==0)
        {
            if(estimateCenterMode) updateMapCenter(&pkt023);
            process(&pkt023,dt);
        }
        } break;
    }

    return !abortRead;
}

void DataSourceDlg::process(Asterix_Abstract* pkt,const QDateTime& tm,int *shiftDate)
{
    if(pkt->cat()==48 && shiftSSRAlt) //handle old RDPS records with altitude +1000ft
        ((Asterix_48*)pkt)->i048_090.mode_C -= 40;

    QSharedPointer<NRadarAbstractPlot> plot=converter->convert(pkt,-1,tm.date());
    if(plot.isNull()) return;

    if(shiftDate)
    {
        *shiftDate=0;
        uint pt=plot->getTime().toTime_t();

        if(lastProcessedTime)
        {
            if(pt<lastProcessedTime-36000)
                *shiftDate=1;
            else if(pt>lastProcessedTime+36000)
                *shiftDate=-1;

            if(*shiftDate) return;
        }

        lastProcessedTime=pt;
    }

    dataPack->data<<plot.data();
    dataPack->savedData<<plot;
}

void DataSourceDlg::updateMapCenter(const Asterix_Abstract *data)
{
    QPointF tmp;
    if(data->uap()==1)
    {
        const Asterix_21_023 *astrx=(const Asterix_21_023*)data;
        if(!astrx->i021_080.data || !astrx->fspec.i021_130) return;
        tmp=QPointF((double)astrx->i021_130.latitude*180.0/8388608,(double)astrx->i021_130.longitude*180.0/8388608);
    }
    else
    {
        const Asterix_21 *astrx=(const Asterix_21*)data;
        if(!astrx->i021_080.data || !astrx->fspec.i021_130) return;
        tmp=QPointF((double)astrx->i021_130.latitude*180.0/8388608,(double)astrx->i021_130.longitude*180.0/8388608);
    }

    if(adsbBoundingRect.isNull())
    {
        adsbBoundingRect.setTopLeft(tmp);
        adsbBoundingRect.setBottomRight(tmp+QPointF(0.001f,0.001f));
    }

    if(tmp.x()<adsbBoundingRect.left())
        adsbBoundingRect.setLeft(tmp.x());
    else if(tmp.x()>adsbBoundingRect.right())
        adsbBoundingRect.setRight(tmp.x());

    if(tmp.y()<adsbBoundingRect.top())
        adsbBoundingRect.setTop(tmp.y());
    else if(tmp.y()>adsbBoundingRect.bottom())
        adsbBoundingRect.setBottom(tmp.y());

    converter->setCenterPoint(adsbBoundingRect.center());

    const NRadarMap* map=converter->getRadarMap();
    QPointF xy1=map->convertLLToXY(adsbBoundingRect.topLeft());
    QPointF xy2=map->convertLLToXY(adsbBoundingRect.bottomRight());

    if(QLineF(xy1,xy2).length() > 1200000)
        finishUpdateMapCenter();
}

void DataSourceDlg::finishUpdateMapCenter()
{
    if(!estimateCenterMode) return;

    if(!adsbBoundingRect.isNull())
    {
        converter->setCenterPoint(getMapCenter());

        const NRadarMap* map=converter->getRadarMap();
        int count=0;

        QMutableListIterator<QSharedPointer<NRadarAbstractPlot> > it(dataPack->savedData);
        while(it.hasNext())
        {
            it.next();

            if(it.value()->getType()!=NRadarAbstractPlot::TypePlot && it.value()->getType()!=NRadarAbstractPlot::TypeTrack)
                continue;

            QSharedPointer<NRadarPlot> plot=qSharedPointerCast<NRadarPlot>(it.value());

            if(plot->getSource()!=NRadarPlot::ADSB) continue;


            QPointF ll = plot->getLLCoord();
            plot->setLLCoord(ll); //update internally cached XY coordinates

            count++;
        }

        qDebug("... adjusted %d ADSB plots",count);
    }

    estimateCenterMode=false;
}
