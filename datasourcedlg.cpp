

#include <rdb/rdbfolder.h>
#include <version.h>
#include <asterix/asterix.h>
#include <radardata/nasterixconverter.h>
#include <radardata/nradarplot.h>
#include <nradarmap.h>

#include "datasourcedlg.h"
#include "datapack.h"

#define DEFAULT_DB_PATH NRPL_MK_PATH("var/rdps/rdb")

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

    if(/*radioFolder->isChecked() &&*/ reader)
    {
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
    }
    else if(radioFile->isChecked() && !edFile->text().endsWith(".rdb"))
    {
        QFile input(edFile->text());
        if(!input.exists() || !input.open(QIODevice::ReadOnly)) return;

        QByteArray data=input.readAll();
        input.close();

        bool res=processAsterix(data);

        dataPack->begin=dataPack->data.first()->getTime();
        dataPack->end=dataPack->data.last()->getTime();
        dataPack->center=getMapCenter();

        stackActions->setCurrentIndex(0);

        if(res) accept();
    }
}

void DataSourceDlg::onAbortImport()
{
    abortRead=true;
}

void DataSourceDlg::onBrowseFile()
{
    radioFile->setChecked(true);

    QString start=edFile->text();
    QString path=QFileDialog::getOpenFileName(this,tr("Choose RDB file or ASTERIX dump"),start);
    if(path.isNull()) return;

    edFile->setText(path);
    if(path.endsWith(".rdb"))
        initRDBReader(path,true);
    else if(reader)
    {
        delete reader;
        reader = 0;
    }
}

void DataSourceDlg::onBrowseFolder()
{
    radioFolder->setChecked(true);

    QString start=edFolder->text();
    if(!start.length()) start=DEFAULT_DB_PATH;
    QString path=QFileDialog::getExistingDirectory(this,tr("Choose RDB files folder"),start);
    if(path.isNull()) return;

    edFolder->setText(path);

    initRDBReader(path,false);
}

void DataSourceDlg::initRDBReader(const QString& path,bool isFile)
{
    if(reader) delete reader;

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
            Asterix_21_13 pkt13;
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
        Asterix_21_13 pkt13;
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
        const Asterix_21_13 *astrx=(const Asterix_21_13*)data;
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
