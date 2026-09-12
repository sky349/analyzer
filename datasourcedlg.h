#ifndef DATASOURCEDLG_H
#define DATASOURCEDLG_H

#include <QtGui>
#include <QtWidgets>
#include <QElapsedTimer>

#include "ui_datasource.h"

class RDBFolderReader;
class NRadarAbstractPlot;
class NAsterixConverter;
class Asterix_Abstract;
class DataPack;
class NRadarMap;

class DataSourceDlg:public QDialog,private Ui_DataSource
{
    Q_OBJECT

public:
    DataSourceDlg(DataPack *dataPack,NRadarMap *map,QWidget *parent);
    ~DataSourceDlg();

protected slots:
    void onImport();
    void onAbortImport();

    void onBrowseFile();
    void onBrowseFolder();

    bool dataIn(qint64 msecs,const QByteArray& data);

protected:
    enum class ImportType { NONE, RDB, ASTERIX, PRP3 };

    void initRDBReader(const QString& path,bool isFile);
    void initPrp3Reader(const QStringList &paths);
    bool importPrp3();
    bool processAsterix(const QByteArray& data);
    void process(Asterix_Abstract* pkt, const QDateTime& tm, int *shiftDate=0);

    void updateMapCenter(const Asterix_Abstract *data);
    void finishUpdateMapCenter();

    QPointF getMapCenter() const;

protected:
    DataPack *dataPack;

    NAsterixConverter *converter;
    RDBFolderReader *reader;
    QStringList m_prp3Files;
    ImportType m_importType = ImportType::NONE;

    bool abortRead;

    QElapsedTimer progressImportUpdate;
    qint64 msecsStart,msecsEnd;

    uint lastProcessedTime;

    bool estimateCenterMode;
    QRectF adsbBoundingRect;

    bool shiftSSRAlt;
};

#endif // DATASOURCEDLG_H
