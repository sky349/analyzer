
#include <radardata/nradartrackplot.h>
#include <radarview/nunitsconverter.h>
#include <radarview/nradardecoration.h>
#include <radarview/nradaritem.h>

#include <QGraphicsItem>
#include <QGraphicsLineItem>
#include <QGraphicsProxyWidget>
#include <QHeaderView>
#include <QMouseEvent>

#include <algorithm>

#include "commondefs.h"

#include "appwindow.h"

#include "datapack.h"
#include "datasourcedlg.h"

#include "guifilter.h"

#include "podtask.h"
#include "reflectorstask.h"
#include "sampletask.h"
#include "amplitudestask.h"
#include "ampscattertask.h"
#include "duplicatetask.h"
#include "falsepsrtask.h"
#include "closepointstask.h"
#include "ampfiltertask.h"
#include "acresolutiontask.h"
#include "scenarioresolutiontask.h"
#include "plotlabel.h"

namespace
{

const qint64 TrackPathMaximumGapMs = 30000;
const double TrackPathPositionToleranceMeters = 5000.0;
const double TrackPathMinimumPlausibleSpeedMps = 350.0;
const double TrackPathMaximumPlausibleSpeedMps = 1200.0;
const double TrackPathSpatialCellSize = 50000.0;
const int TrackPathChunkSegmentCount = 32;

uint rawTrackNumber(uint trackId)
{
    return trackId&0xffffu;
}

QPointF displayPosition(const NRadarPlot *plot,int altitudeMultiplier)
{
    if(altitudeMultiplier<=0)
        return plot->getXYCoord();

    return QPointF(plot->getADCoord().y(),
                   altitudeMultiplier*(plot->hasHeight() ? plot->getHeight() : -100.0));
}

bool hasConflictingAircraftAddress(const NRadarTrackPlot *previous,
                                   const NRadarTrackPlot *current)
{
    const QVariant previousValue=previous->getOption(NRadarPlot::AircraftAddress);
    const QVariant currentValue=current->getOption(NRadarPlot::AircraftAddress);
    if(previousValue.isNull() || currentValue.isNull())
        return false;

    const uint previousAddress=previousValue.toUInt();
    const uint currentAddress=currentValue.toUInt();
    return previousAddress && currentAddress && previousAddress!=currentAddress;
}

bool isContinuousTrack(const NRadarTrackPlot *previous,
                       const NRadarTrackPlot *current)
{
    const qint64 gapMs=previous->getTime().msecsTo(current->getTime());
    if(gapMs<0 || gapMs>TrackPathMaximumGapMs)
        return false;

    if((previous->getSource()==NRadarPlot::ADSB)!=(current->getSource()==NRadarPlot::ADSB))
        return false;

    if(hasConflictingAircraftAddress(previous,current))
        return false;

    const double reportedSpeedMps=qMax(previous->getSpeed(),current->getSpeed())/3.6;
    const double plausibleSpeedMps=qBound(TrackPathMinimumPlausibleSpeedMps,
                                          reportedSpeedMps*3.0,
                                          TrackPathMaximumPlausibleSpeedMps);
    const double allowedDistance=TrackPathPositionToleranceMeters+
            plausibleSpeedMps*(gapMs/1000.0);
    return QLineF(previous->getXYCoord(),current->getXYCoord()).length()<=allowedDistance;
}

quint32 trackStyleHash(quint8 radarId,uint trackId)
{
    quint32 value=trackId^(quint32(radarId)*0x9e3779b9u);
    value^=value>>16;
    value*=0x7feb352du;
    value^=value>>15;
    value*=0x846ca68bu;
    return value^(value>>16);
}

QColor trackColor(quint8 radarId,uint trackId,quint64 trackInstance)
{
    static const QRgb palette[]={
        0xFFE69F00, 0xFF56B4E9, 0xFF009E73, 0xFFF0E442,
        0xFF0072B2, 0xFFD55E00, 0xFFCC79A7, 0xFF00C2FF,
        0xFF8BE04E, 0xFFFF6F91, 0xFFB388FF, 0xFFFFB74D
    };
    const int colorCount=sizeof(palette)/sizeof(palette[0]);
    const int colorIndex=(trackStyleHash(radarId,trackId)+trackInstance)%colorCount;
    return QColor::fromRgba(palette[colorIndex]);
}

Qt::PenStyle monochromeTrackStyle(quint8 radarId,uint trackId,quint64 trackInstance)
{
    static const Qt::PenStyle styles[]={
        Qt::SolidLine, Qt::DashLine, Qt::DotLine,
        Qt::DashDotLine, Qt::DashDotDotLine
    };
    const int styleCount=sizeof(styles)/sizeof(styles[0]);
    const int styleIndex=(trackStyleHash(radarId,trackId)+trackInstance)%styleCount;
    return styles[styleIndex];
}

quint64 trackPathCellKey(int x,int y)
{
    return (quint64(quint32(x))<<32)|quint32(y);
}

QColor colorWithOpacity(QColor color,double opacity)
{
    color.setAlpha(qRound(color.alpha()*opacity));
    return color;
}

}

class TrackPathsOverlayItem:public QGraphicsItem
{
public:
    TrackPathsOverlayItem(const QRectF& bounds,QGraphicsItem *parent):
        QGraphicsItem(parent),
        m_bounds(bounds),
        m_blackWhiteMode(false),
        m_highlightActive(false),
        m_highlightedTrackInstance(0),
        m_paintGeneration(0)
    {
        setAcceptedMouseButtons(Qt::NoButton);
        setAcceptHoverEvents(false);
        setFlag(QGraphicsItem::ItemIsSelectable,false);
        setFlag(QGraphicsItem::ItemStacksBehindParent,true);
        setFlag(QGraphicsItem::ItemUsesExtendedStyleOption,true);
    }

    QRectF boundingRect() const
    {
        return m_bounds;
    }

    void setBounds(const QRectF& bounds)
    {
        if(m_bounds==bounds) return;
        prepareGeometryChange();
        m_bounds=bounds;
    }

    void clearPaths()
    {
        m_chunks.clear();
        m_grid.clear();
        m_instances.clear();
        m_chunkPaintGeneration.clear();
        m_paintGeneration=0;
        update();
    }

    void addPolyline(const QPolygonF& polyline,
                     quint8 radarId,
                     uint trackId,
                     quint64 trackInstance)
    {
        if(polyline.size()<2) return;

        const QColor color=trackColor(radarId,trackId,trackInstance);
        const Qt::PenStyle monochromeStyle=
                monochromeTrackStyle(radarId,trackId,trackInstance);

        int first=0;
        while(first<polyline.size()-1)
        {
            const int last=qMin(first+TrackPathChunkSegmentCount,polyline.size()-1);

            Chunk chunk;
            chunk.points.reserve(last-first+1);
            for(int i=first;i<=last;i++)
                chunk.points<<polyline.at(i);
            chunk.bounds=chunk.points.boundingRect();
            // A horizontal or vertical polyline has an empty QRectF and would
            // otherwise be rejected by QRectF::intersects during culling.
            chunk.bounds.adjust(-0.5,-0.5,0.5,0.5);
            chunk.trackInstance=trackInstance;
            chunk.color=color;
            chunk.monochromeStyle=monochromeStyle;

            const int chunkIndex=m_chunks.size();
            m_chunks<<chunk;
            m_chunkPaintGeneration<<0;
            indexChunk(chunkIndex);

            first=last;
        }

        m_instances.insert(trackInstance);
    }

    bool containsTrackInstance(quint64 trackInstance) const
    {
        return m_instances.contains(trackInstance);
    }

    void setDisplayStyle(bool blackWhiteMode,
                         bool highlightActive,
                         quint64 highlightedTrackInstance)
    {
        if(m_blackWhiteMode==blackWhiteMode &&
                m_highlightActive==highlightActive &&
                m_highlightedTrackInstance==highlightedTrackInstance)
            return;

        m_blackWhiteMode=blackWhiteMode;
        m_highlightActive=highlightActive;
        m_highlightedTrackInstance=highlightedTrackInstance;
        update();
    }

    void paint(QPainter *painter,
               const QStyleOptionGraphicsItem *option,
               QWidget *)
    {
        if(m_chunks.isEmpty()) return;

        QRectF exposed=option ? option->exposedRect : painter->clipBoundingRect();
        if(exposed.isEmpty())
            exposed=painter->clipBoundingRect();

        const double levelOfDetail=
                QStyleOptionGraphicsItem::levelOfDetailFromTransform(painter->worldTransform());
        const double margin=levelOfDetail>0.0 ? 6.0/levelOfDetail : 0.0;
        exposed.adjust(-margin,-margin,margin,margin);

        QVector<int> visibleChunks;
        findChunks(exposed,visibleChunks);
        if(visibleChunks.isEmpty()) return;

        std::sort(visibleChunks.begin(),visibleChunks.end());
        painter->setRenderHint(QPainter::Antialiasing,true);
        painter->setBrush(Qt::NoBrush);

        for(int pass=0;pass<2;pass++)
        {
            foreach(int chunkIndex,visibleChunks)
            {
                const Chunk& chunk=m_chunks.at(chunkIndex);
                const bool highlighted=m_highlightActive &&
                        chunk.trackInstance==m_highlightedTrackInstance;
                const bool dimmed=m_highlightActive && !highlighted;
                const double opacity=dimmed ? 0.22 : 1.0;

                QPen pen;
                if(pass==0)
                {
                    const QColor outline=m_blackWhiteMode ? QColor(Qt::white) : QColor(0,0,0,150);
                    pen=QPen(colorWithOpacity(outline,opacity),
                             highlighted ? 5.0 : 3.5,
                             Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin);
                }
                else
                {
                    const QColor line=m_blackWhiteMode ? QColor(Qt::black) : chunk.color;
                    pen=QPen(colorWithOpacity(line,opacity),
                             highlighted ? 3.0 : 1.5,
                             m_blackWhiteMode ? chunk.monochromeStyle : Qt::SolidLine,
                             Qt::RoundCap,Qt::RoundJoin);
                }
                pen.setCosmetic(true);
                painter->setPen(pen);
                painter->drawPolyline(chunk.points);
            }
        }
    }

private:
    struct Chunk
    {
        QPolygonF points;
        QRectF bounds;
        quint64 trackInstance;
        QColor color;
        Qt::PenStyle monochromeStyle;
    };

    void indexChunk(int chunkIndex)
    {
        const QRectF& bounds=m_chunks.at(chunkIndex).bounds;
        const int left=qFloor(bounds.left()/TrackPathSpatialCellSize);
        const int right=qFloor(bounds.right()/TrackPathSpatialCellSize);
        const int top=qFloor(bounds.top()/TrackPathSpatialCellSize);
        const int bottom=qFloor(bounds.bottom()/TrackPathSpatialCellSize);

        for(int x=left;x<=right;x++)
            for(int y=top;y<=bottom;y++)
                m_grid[trackPathCellKey(x,y)]<<chunkIndex;
    }

    void findChunks(const QRectF& exposed,QVector<int>& result)
    {
        m_paintGeneration++;
        if(!m_paintGeneration)
        {
            m_chunkPaintGeneration.fill(0);
            m_paintGeneration=1;
        }

        const int left=qFloor(exposed.left()/TrackPathSpatialCellSize);
        const int right=qFloor(exposed.right()/TrackPathSpatialCellSize);
        const int top=qFloor(exposed.top()/TrackPathSpatialCellSize);
        const int bottom=qFloor(exposed.bottom()/TrackPathSpatialCellSize);

        for(int x=left;x<=right;x++)
            for(int y=top;y<=bottom;y++)
            {
                QHash<quint64,QVector<int> >::const_iterator it=
                        m_grid.constFind(trackPathCellKey(x,y));
                if(it==m_grid.constEnd()) continue;

                foreach(int chunkIndex,it.value())
                {
                    if(m_chunkPaintGeneration[chunkIndex]==m_paintGeneration)
                        continue;
                    m_chunkPaintGeneration[chunkIndex]=m_paintGeneration;

                    if(m_chunks.at(chunkIndex).bounds.intersects(exposed))
                        result<<chunkIndex;
                }
            }
    }

private:
    QRectF m_bounds;
    QVector<Chunk> m_chunks;
    QHash<quint64,QVector<int> > m_grid;
    QSet<quint64> m_instances;
    QVector<quint32> m_chunkPaintGeneration;
    bool m_blackWhiteMode;
    bool m_highlightActive;
    quint64 m_highlightedTrackInstance;
    quint32 m_paintGeneration;
};

class PlotItem:public NRadarItem
{
public:
	PlotItem(const QPointF& pos,
             const NRadarPlot *plot,
             int type,
             const QColor *notAssociatedColor,
             const bool *blackWhiteMode) :
        NRadarItem(NRadarItem::Point),
        m_notAssociatedColor(notAssociatedColor),
        m_blackWhiteMode(blackWhiteMode)
	{
		this->type=type;
		this->plot=plot;
		posGeo = pos;

		switchToGeoPos();
		setFlag(QGraphicsItem::ItemIsSelectable,true);
	}

	QRect boundingRect() const
	{ return QRect(-4,-4,10,10); }

	void paint(QPainter *painter,const NRadarItemOption *opt)
	{
        if(opt->scale>150)
		{
			painter->setPen(Qt::NoPen);
			painter->setBrush(color1(opt));
			int r=(opt->scale>500 ? 1:2);
			painter->drawEllipse(QPoint(0,0),r,r);
		}
		else
		{
			painter->drawEllipse(boundingRect());
		}
	}

	QColor color1(const NRadarItemOption *option) const
	{
        const bool isBlackWhite = (m_blackWhiteMode && *m_blackWhiteMode);
        if(option->state&QStyle::State_Selected)
            return isBlackWhite ? Qt::red : Qt::white;

        if(fixedColor.isValid())
            return fixedColor;

        if (plot->getSource() == 2)
        {
            if (plot->getPlotAssociation() == NRadarPlot::NotAssociated)
                return isBlackWhite ? Qt::black : (m_notAssociatedColor ? *m_notAssociatedColor : Qt::gray);

            if (plot->getSSRType()==NRadarPlot::ModeS && (plot->getOption(NRadarPlot::AllCall)).toBool())
                return isBlackWhite ? Qt::black : Qt::cyan;

            if (plot->getSSRType() == NRadarPlot::ModeS)
                return isBlackWhite ? Qt::black : QColor(0xCAF0F8); //Qt::blue;

            return isBlackWhite ? Qt::black : Qt::yellow;
        }

        switch(type)
			{
            case 1: return isBlackWhite ? Qt::black : Qt::red;
            //case 2: return Qt::yellow;
            case 3: return isBlackWhite ? Qt::black : QColor(0,162,232);

            case 12: return isBlackWhite ? Qt::black : QColor(255,128,0);
            case 13: return isBlackWhite ? Qt::black : QColor(0,255,255);
            case 14: return isBlackWhite ? Qt::black : Qt::green;
			}
        return isBlackWhite ? Qt::black : Qt::white;
	}

	void switchToGeoPos(int alt_mul=0)
	{
		setPos(alt_mul<=0 ? posGeo : displayPosition(plot,alt_mul));
	}

//protected:
	int type;
	const NRadarPlot *plot;
	QPointF posGeo;
	QColor fixedColor;
    const QColor *m_notAssociatedColor;
    const bool *m_blackWhiteMode;
};

AppWindow::AppWindow():QMainWindow(0),
    m_notAssociated(Qt::gray),
    m_blackWhiteMode(false),
    m_trackOverlay(0),
    m_adsbTrackOverlay(0),
    m_draggingPopupIndex(-1),
    m_altitudeMultiplier(0),
    m_hasHighlightedTrack(false),
    m_highlightedTrackInstance(0)
{
	setupUi(this);

	radarScene=new NRadarScene(NRadarMap::createMap(60,30,650000));
	radarView->setScene(radarScene);
	radarView->setScaleLimits(1);
	radarScene->getMap()->initTiles();
    radarView->viewport()->installEventFilter(this);

	//radarView->set3DAllowed(true);

	barWorking=new QProgressBar(this);
	barWorking->setRange(0,100);
	statusBar()->addWidget(barWorking);
	barWorking->setVisible(false);

	tasks<<new SampleTask(this);
	tasks<<new PoDTask(this);
	tasks<<new ReflectorsTask(this);
	tasks<<new AmplitudesTask(this);
    tasks<<new ampScatterTask(this);
    tasks<<new DuplicateTask(this);
    tasks<<new FalsePSRTask(this);
    tasks<<new CloseTrackPointsTask(this);
    tasks<<new AmpFilterTask(this);
    tasks<<new ACResolutionTask(this);
    tasks<<new ScenarioResolutionTask(this);

	initGUI();
	initActions();

	treeDetails->setColumnCount(2);
	treeDetails->setRootIsDecorated(false);

	QHeaderView *header=treeDetails->header();
	header->setStretchLastSection(false);
	header->setSectionResizeMode(0,QHeaderView::Stretch);
	header->setSectionResizeMode(1,QHeaderView::ResizeToContents);
	header->setVisible(false);

	connect(radarScene,SIGNAL(itemSelectionChanged(NRadarItem*)),this,SLOT(onPlotSelected(NRadarItem*)));

    connect(blackWhite_chk, &QCheckBox::clicked, this, [this](bool checked){
        m_blackWhiteMode = checked;

        if(QAction *tilesAct = radarView->getControlAction(NRadarView::ActTilesOn))
        {
            tilesAct->setChecked(!checked);
            tilesAct->activate(QAction::Trigger);
        }

        updateTrackPathStyles();
        radarView->resetCachedContent();
        radarView->viewport()->update();
    });

    connect(connectTracks_chk,&QCheckBox::toggled,this,[this](bool checked){
        setTrackPathsVisible(checked && cmbType->currentIndex()!=1);
        radarView->viewport()->update();
    });

    connect(predictedTracks_chk,&QCheckBox::toggled,this,[this](bool){
        applyFilter();
    });

    shortTracksLimit_edit->setValidator(new QIntValidator(0,999999999,
                                                           shortTracksLimit_edit));
    connect(shortTracks_chk,&QCheckBox::toggled,this,[this](bool checked){
        shortTracksLimit_edit->setEnabled(checked);
        applyFilter();
    });
    connect(shortTracksLimit_edit,&QLineEdit::editingFinished,this,[this](){
        if(shortTracks_chk->isChecked())
            applyFilter();
    });

	dataPack=new DataPack();

	if(!importData(false)) return;

	QTimer::singleShot(10,this,SLOT(applyFilter()));
}

AppWindow::~AppWindow()
{
    closeAllPlotPopups();
	clearTrackPaths();
	delete dataPack;

	qDeleteAll(tasks);
}

void AppWindow::initGUI()
{
	filters<<new ComboBoxFilter(cmbSource,btnSourceClear,this,SLOT(applyFilter()));
	filters<<new ComboBoxFilter(cmbType,btnTypeClear,this,SLOT(applyFilterToLayers()));

	filters<<new ComboBoxFilter(cmbModeS,btnModeSClear,this,SLOT(applyFilter()));

	filters<<new ComboBoxFilter(cmbModeA,btnModeAClear,this,SLOT(applyFilter()));
	filters<<new ComboBoxFilter(cmbAircraftID,btnAircraftIDClear,this,SLOT(applyFilter()));
	filters<<new ComboBoxFilter(cmbAddress,btnAddressClear,this,SLOT(applyFilter()));
	filters<<new ComboBoxFilter(cmbTrackNo,btnTrackNoClear,this,SLOT(applyFilter()));

	filters<<new DateTimeFilter(dateBegin,btnBeginClear,this,SLOT(applyFilter()));
	filters<<new DateTimeFilter(dateEnd,btnEndClear,this,SLOT(applyFilter()));

	PolygonFilter *pf=new PolygonFilter(btnArea,btnAreaClear,this,SLOT(applyFilter()));
	connect(pf,SIGNAL(choosePolygon()),radarView->getControlAction(NRadarView::ActSelectRegion),SLOT(trigger()));
	connect(radarView,SIGNAL(areaSelected(const QPolygon&)),pf,SLOT(set(const QPolygon&)));
	filters<<pf;

	areaFilter=pf;
}

void AppWindow::initActions()
{
	connect(actImport,SIGNAL(triggered()),this,SLOT(importData()));
	connect(actExit,SIGNAL(triggered()),qApp,SLOT(quit()));
	connect(actVersion,SIGNAL(triggered()),this,SLOT(showAbout()));
	connect(actLoadMap,SIGNAL(triggered()),this,SLOT(loadMap()));

	QMenu *tasksMenu=new QMenu(this);
	int count=0;
	foreach(AnalyserTask *t,tasks)
	{
		QAction *act=tasksMenu->addAction(t->getName(true));
		if(t->getType()==AnalyserTask::TwoStages)
			act->setCheckable(true);
		act->setData(count++);
	}
	connect(tasksMenu,SIGNAL(triggered(QAction*)),this,SLOT(executeTask(QAction*)));
	actionTasks->setMenu(tasksMenu);

	QButtonGroup *grp = new QButtonGroup(this);
	grp->addButton(radioViewNormal,0);
	grp->addButton(radioViewRA,1);
	grp->addButton(radioViewRAx10,2);

	connect(grp,SIGNAL(buttonClicked(int)),this,SLOT(updateViewMode(int)));
}

void AppWindow::loadMap()
{
	QString path = QFileDialog::getOpenFileName(this,tr("Select maps file"),NRPL_MK_PATH("etc/maps"),"Maps (*.xmap)");
	if(path.isNull()) return;

	radarView->getRadarScene()->getMap()->addLayers(path);
}

bool AppWindow::importData(bool confirm)
{
	if(property("valid").toBool())
		if(confirm && QMessageBox::warning(this,tr("Data import"),tr("This will clear all previously imported data.\nIs it OK to continue?"),
									   QMessageBox::Yes,QMessageBox::No)==QMessageBox::No)
		return false;

	setProperty("valid",false);

	clearTrackPaths();
	m_hasHighlightedTrack=false;
	radarScene->clear();
	layers.clear();
	dataPack->clear();

	layers[1]=radarScene->addLayer("psr",1);
	layers[2]=radarScene->addLayer("ssr",2);
	layers[3]=radarScene->addLayer("adsb",3);
	layers[11]=radarScene->addLayer("track",4);

	foreach(GuiFilter* f,filters)
		f->clear();
	initFilters(dataPack); //что бы все очистить

	if(!radioViewNormal->isChecked())
		radioViewNormal->animateClick(0);

	qApp->processEvents();

	DataSourceDlg dlg(dataPack,radarScene->getMap(),this);
	if(!dlg.exec())
		return false;

	QPointF pt=dataPack->getCenter();
	radarScene->getMap()->setCenterPoint(pt.x(),pt.y());

	initFilters(dataPack);
	foreach(GuiFilter* f,filters)
		f->clear();

	qDebug()<<"Imported data size: "<<dataPack->getData().size();

	setProperty("valid",true);

	return true;
}

void AppWindow::updateViewMode(int m)
{
	if(m>0)
	{
		setProperty("tiles-on",radarScene->getMap()->isTilesOn());
		radarScene->getMap()->setTiles(false);
	}
	else
		radarScene->getMap()->setTiles(property("tiles-on").toBool());

	int alt_mul = m==0 ? 0 : (m==1 ? 1:10);
	m_altitudeMultiplier=alt_mul;
	double r=650000.0;
	if(m==0)
		radarScene->setSceneRect(-r,-r,r*2,r*2);
	else
		radarScene->setSceneRect(-5000,-500*alt_mul,r,20500*alt_mul);

	radarView->getDecoration()->setPolarGrid(m==0);

	QList<NRadarItem*> items=
			radarScene->layerByName("psr")->childItems() +
			radarScene->layerByName("ssr")->childItems() +
			radarScene->layerByName("adsb")->childItems() +
			radarScene->layerByName("track")->childItems();

	foreach(NRadarItem* item,items)
	{
		PlotItem *pl = dynamic_cast<PlotItem*>(item);
		if(!pl) continue;
		pl->switchToGeoPos(alt_mul);
	}

	rebuildTrackPaths();

	radarView->showFullMap();
}

void AppWindow::initFilters(const DataPack *data)
{
	QMap<QString,bool> modeAList,addressList,idList,trackList;
	QMap<uint,bool> tmp1,tmp2,tmp3;
	bool hasPlotsWithoutModeA=false;

	foreach(NRadarAbstractPlot* ap,data->getData())
	{
		if(ap->getType()!=NRadarAbstractPlot::TypePlot && ap->getType()!=NRadarAbstractPlot::TypeTrack)
			continue;

		NRadarPlot* plot=static_cast<NRadarPlot*>(ap);
		if(plot->getSource()==NRadarPlot::PSR) continue;

		if(plot->hasBoardNumber())
			tmp1[plot->getBoardNumber()]=true;
		else
			hasPlotsWithoutModeA=true;

		uint address=plot->getOption(NRadarPlot::AircraftAddress).toUInt();
		if(address) tmp2[address]=true;

		if(plot->getType()==NRadarAbstractPlot::TypePlot)
			foreach(uint tid,plot->getAssociatedTrackIds())
				tmp3[rawTrackNumber(tid)]=true;
		else if(plot->getSource()!=NRadarPlot::ADSB)
			tmp3[rawTrackNumber(
					static_cast<NRadarTrackPlot*>(ap)->getTrackId())]=true;

		QString id=plot->getOption(NRadarPlot::AircraftId).toString().toUpper();
		if(id.length()) idList[id]=true;
	}

	if(hasPlotsWithoutModeA)
		modeAList[tr("N/A")]=true;
	foreach(uint u,tmp1.keys())
		modeAList[QString("%1").arg(u,4,10,QChar('0'))]=true;

	foreach(uint u,tmp2.keys())
		addressList[QString("%1").arg(u,6,16,QChar('0'))]=true;

	foreach(uint u,tmp3.keys())
		trackList[QString::number(u)]=true;

	fillComboBox(cmbModeA,modeAList);
	for(int i=1;i<cmbModeA->count();i++)
	{
		bool ok=false;
		const uint modeA=cmbModeA->itemText(i).toUInt(&ok);
		if(ok)
			cmbModeA->setItemData(i,modeA);
	}
	fillComboBox(cmbAircraftID,idList);
	fillComboBox(cmbAddress,addressList);
	fillComboBox(cmbTrackNo,trackList);

	QDateTime tm1=data->getBeginDate(),tm2=data->getEndDate();
	dateBegin->setDateTimeRange(tm1,tm2);
	dateBegin->setDateTime(tm1);
	dateBegin->setProperty("default-datetime",tm1);
	dateEnd->setDateTimeRange(tm1,tm2.addSecs(1));
	dateEnd->setDateTime(tm2);
	dateEnd->setProperty("default-datetime",tm2);
}

void AppWindow::fillComboBox(QComboBox *cmb,const QMap<QString,bool>& list)
{
	cmb->clear();
	cmb->addItem(tr("Any"),0);

	if(!list.size())
		cmb->setEnabled(false);
	else foreach(QString name,list.keys())
		cmb->addItem(name);
}

void AppWindow::applyFilterToLayers()
{ applyFilter(false); }

void AppWindow::applyFilter(bool fullFilter)
{
	if(!property("valid").toBool()) return;

	int src=cmbSource->currentIndex();
	int type=cmbType->currentIndex();
	const bool limitTrackPoints=shortTracks_chk->isChecked();
	const bool showPredictedTrackPoints=predictedTracks_chk->isChecked();

	if(limitTrackPoints)
	{
		// This mode explicitly shows tracks only. Source and type selections
		// still decide which track layers can be displayed.
		layers[1]->setVisible(false);
		layers[2]->setVisible(false);
		layers[3]->setVisible((src==0 || src==3) && type!=1);
		layers[11]->setVisible(src!=3 && type!=1);
	}
	else
	{
		layers[1]->setVisible(src<=1 && type<=1);
		layers[2]->setVisible((src==0 || src==2) && type<=1);
		layers[3]->setVisible(src==0 || src==3);
		layers[11]->setVisible(src!=3 && type!=1);
	}
	setTrackPathsVisible(connectTracks_chk->isChecked() && type!=1);

	if(!fullFilter) return;

	const bool filterModeA=cmbModeA->currentIndex()!=0;
	const QVariant selectedModeA=cmbModeA->itemData(cmbModeA->currentIndex());
	const bool filterMissingModeA=filterModeA && !selectedModeA.isValid();
	const uint modeA=selectedModeA.toUInt();
	uint address=(cmbAddress->currentIndex()==0 ? 0 : cmbAddress->currentText().toUInt(0,16));
	QString aircraftId=(cmbAircraftID->currentIndex() ? cmbAircraftID->currentText() : "");
	uint trackNo=(cmbTrackNo->currentIndex()==0 ? 0 : cmbTrackNo->currentText().toUInt());
	uint modeS=cmbModeS->currentIndex();
	const uint maximumTrackPoints=shortTracksLimit_edit->text().toUInt();
	const QDateTime begin=dateBegin->dateTime();
	const QDateTime end=dateEnd->dateTime();
	const QPolygon& area=qobject_cast<PolygonFilter*>(areaFilter)->getPolygon();

	rebuildTrackInstances();

	auto isFilteredOut=[&](NRadarAbstractPlot *data)->bool
	{
		NRadarPlot *plot=static_cast<NRadarPlot*>(data);

		if(!showPredictedTrackPoints &&
				data->getType()==NRadarAbstractPlot::TypeTrack &&
				static_cast<NRadarTrackPlot*>(data)->getTrackPlotType()==
				NRadarTrackPlot::PredictedPoint)
			return true;

		if(data->getType()==NRadarAbstractPlot::TypeTrack &&
				(type!=1 || limitTrackPoints) && src)
		{
			const NRadarPlot::NPlotSourceType source=plot->getSource();
			bool sourceMatches=false;
			if(src==1)
				sourceMatches=source==NRadarPlot::PSR || source==NRadarPlot::Combined;
			else if(src==2)
				sourceMatches=source==NRadarPlot::SSR || source==NRadarPlot::Combined;
			else if(src==3)
				sourceMatches=source==NRadarPlot::ADSB;
			if(!sourceMatches) return true;
		}

		if(modeS && ((modeS==1 && plot->getSSRType()==NRadarPlot::ModeS) ||
				(modeS==2 && plot->getSSRType()!=NRadarPlot::ModeS)))
			return true;

		if(filterModeA)
		{
			if(filterMissingModeA)
			{
				if(plot->hasBoardNumber()) return true;
			}
			else if(!plot->hasBoardNumber() || plot->getBoardNumber()!=modeA)
				return true;
		}

		if(trackNo)
		{
			if(data->getType()==NRadarAbstractPlot::TypePlot)
			{
				bool matchingTrack=false;
				foreach(uint associatedTrackId,plot->getAssociatedTrackIds())
					if(rawTrackNumber(associatedTrackId)==trackNo)
					{
						matchingTrack=true;
						break;
					}
				if(!matchingTrack) return true;
			}
			else if(rawTrackNumber(
					static_cast<NRadarTrackPlot*>(data)->getTrackId())!=trackNo)
				return true;
		}

		if(address && plot->getOption(NRadarPlot::AircraftAddress).toUInt()!=address)
			return true;

		if(aircraftId.length() &&
				plot->getOption(NRadarPlot::AircraftId).toString()!=aircraftId)
			return true;

		const QDateTime time=plot->getTime();
		if(time<begin || time>end)
			return true;

		return !area.isEmpty() && !area.containsPoint(plot->getXYCoord(),Qt::OddEvenFill);
	};

	QHash<quint64,uint> visibleTrackPointCounts;
	if(limitTrackPoints)
	{
		foreach(NRadarAbstractPlot *data,dataPack->getData())
		{
			if(data->getType()!=NRadarAbstractPlot::TypeTrack || isFilteredOut(data))
				continue;

			const NRadarTrackPlot *track=static_cast<const NRadarTrackPlot*>(data);
			if(track->getTrackPlotType()==NRadarTrackPlot::EndPoint)
				continue;
			visibleTrackPointCounts[m_trackInstances.value(track)]++;
		}
	}

	barWorking->setValue(0);
	barWorking->setVisible(true);
	lastWorkingUpdate.start();
	qApp->processEvents();

	radarView->setUpdatesEnabled(false);

//    QTime bench;
//    bench.start();

	qint64 count=0;//,spi=0;
	foreach(NRadarAbstractPlot* data,dataPack->getData())
	{
		count++;
		if(count%10000==0 && lastWorkingUpdate.elapsed()>1000)
		{
			int progress=count*100/dataPack->getData().size();
			barWorking->setValue(progress);
			qApp->processEvents();
			lastWorkingUpdate.restart();
		}

		if(data->getType()!=NRadarAbstractPlot::TypePlot && data->getType()!=NRadarAbstractPlot::TypeTrack)
			continue;

		NRadarPlot* plot=static_cast<NRadarPlot*>(data);
		PlotItem *pi=static_cast<PlotItem*>(data->getUserData());

		bool filterOut=isFilteredOut(data);
		if(!filterOut && limitTrackPoints)
		{
			if(data->getType()!=NRadarAbstractPlot::TypeTrack)
				filterOut=true;
			else
			{
				const NRadarTrackPlot *track=static_cast<const NRadarTrackPlot*>(data);
				const quint64 trackInstance=m_trackInstances.value(track);
				filterOut=visibleTrackPointCounts.value(trackInstance)>maximumTrackPoints;
			}
		}

		if(filterOut && pi)
		{
			delete pi;
			pi=0;
		}

		if(!filterOut && !pi)
		{
			int id=0;
			switch(plot->getSource())
			{
			case NRadarPlot::PSR: id=1; break;
			case NRadarPlot::SSR: id=2; break;
			case NRadarPlot::ADSB: id=3; break;
			default:;
			}

			if(id!=3 && plot->getType()==NRadarAbstractPlot::TypeTrack)
				id=11;

			if(id)
			{
				pi=new PlotItem(plot->getXYCoord(),
                                plot,
                                id==11 ? id+plot->getSource() : id,
                                &m_notAssociated,
                                &m_blackWhiteMode);
				radarScene->addItem(pi,layers[id]);
			}
		}
		plot->setUserData(pi);
	}

	rebuildTrackPaths();

//    qDebug()<<bench.elapsed()<<"ms for"<<dataPack->getData().size()<<"items";
//    if(bench.elapsed())
//        qDebug()<<"==="<<((qint64)dataPack->getData().size()*1000 / bench.elapsed())<<"items/s";

//    qDebug()<<"SPI"<<spi;
	barWorking->setVisible(false);
	radarView->setUpdatesEnabled(true);
}

void AppWindow::clearTrackPaths()
{
    delete m_trackOverlay;
    delete m_adsbTrackOverlay;
    m_trackOverlay=0;
    m_adsbTrackOverlay=0;
    m_trackInstances.clear();
}

void AppWindow::rebuildTrackInstances()
{
    m_trackInstances.clear();

    QMap<quint64,QList<const NRadarTrackPlot*> > tracks;
    foreach(NRadarAbstractPlot *data,dataPack->getData())
    {
        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        const NRadarTrackPlot *track=static_cast<const NRadarTrackPlot*>(data);
        const quint64 key=(quint64(track->getRadarId())<<32)|quint64(track->getTrackId());
        tracks[key]<<track;
    }

    quint64 nextTrackInstance=1;
    QMapIterator<quint64,QList<const NRadarTrackPlot*> > trackIt(tracks);
    while(trackIt.hasNext())
    {
        trackIt.next();
        QList<const NRadarTrackPlot*> samples=trackIt.value();
        std::stable_sort(samples.begin(),samples.end(),
                         [](const NRadarTrackPlot *left,const NRadarTrackPlot *right)
        {
            return left->getTime()<right->getTime();
        });

        const NRadarTrackPlot *previousTrack=0;
        quint64 trackInstance=0;
        foreach(const NRadarTrackPlot *track,samples)
        {
            if(!trackInstance ||
                    (previousTrack && !isContinuousTrack(previousTrack,track)))
                trackInstance=nextTrackInstance++;

            m_trackInstances.insert(track,trackInstance);
            previousTrack=track;

            if(track->getTrackPlotType()==NRadarTrackPlot::EndPoint)
            {
                trackInstance=0;
                previousTrack=0;
            }
        }
    }
}

void AppWindow::ensureTrackPathOverlays()
{
    const QRectF bounds=radarScene->sceneRect();
    if(!m_trackOverlay)
    {
        NRadarSceneLayer *layer=layers.value(11,0);
        QGraphicsItem *parent=layer ? layer->associatedGraphicsItem() : 0;
        if(parent)
            m_trackOverlay=new TrackPathsOverlayItem(bounds,parent);
    }
    else
        m_trackOverlay->setBounds(bounds);

    if(!m_adsbTrackOverlay)
    {
        NRadarSceneLayer *layer=layers.value(3,0);
        QGraphicsItem *parent=layer ? layer->associatedGraphicsItem() : 0;
        if(parent)
            m_adsbTrackOverlay=new TrackPathsOverlayItem(bounds,parent);
    }
    else
        m_adsbTrackOverlay->setBounds(bounds);
}

void AppWindow::addTrackPolyline(const QPolygonF& polyline,
                                 quint8 radarId,
                                 uint trackId,
                                 quint64 trackInstance,
                                 bool adsb)
{
    TrackPathsOverlayItem *overlay=adsb ? m_adsbTrackOverlay : m_trackOverlay;
    if(overlay)
        overlay->addPolyline(polyline,radarId,trackId,trackInstance);
}

void AppWindow::setTrackPathsVisible(bool visible)
{
    if(m_trackOverlay)
        m_trackOverlay->setVisible(visible);
    if(m_adsbTrackOverlay)
        m_adsbTrackOverlay->setVisible(visible);
}

void AppWindow::rebuildTrackPaths()
{
    if(!property("valid").toBool()) return;

    ensureTrackPathOverlays();
    if(m_trackOverlay)
        m_trackOverlay->clearPaths();
    if(m_adsbTrackOverlay)
        m_adsbTrackOverlay->clearPaths();

    if(m_trackInstances.isEmpty())
        rebuildTrackInstances();

    QMap<quint64,QList<const NRadarTrackPlot*> > tracks;
    foreach(NRadarAbstractPlot *data,dataPack->getData())
    {
        if(data->getType()!=NRadarAbstractPlot::TypeTrack)
            continue;

        const NRadarTrackPlot *track=static_cast<const NRadarTrackPlot*>(data);
        const quint64 key=(quint64(track->getRadarId())<<32)|quint64(track->getTrackId());
        tracks[key]<<track;
    }

    QMapIterator<quint64,QList<const NRadarTrackPlot*> > trackIt(tracks);
    while(trackIt.hasNext())
    {
        trackIt.next();
        QList<const NRadarTrackPlot*> samples=trackIt.value();
        std::stable_sort(samples.begin(),samples.end(),
                         [](const NRadarTrackPlot *left,const NRadarTrackPlot *right)
        {
            return left->getTime()<right->getTime();
        });

        QPolygonF polyline;
        bool pathIsAdsb=false;
        quint64 trackInstance=0;

        foreach(const NRadarTrackPlot *track,samples)
        {
            const bool adsb=track->getSource()==NRadarPlot::ADSB;
            const quint64 sampleTrackInstance=m_trackInstances.value(track);

            if(sampleTrackInstance!=trackInstance)
            {
                addTrackPolyline(polyline,track->getRadarId(),track->getTrackId(),
                                 trackInstance,pathIsAdsb);
                polyline.clear();
                trackInstance=sampleTrackInstance;
                pathIsAdsb=adsb;
            }

            PlotItem *point=static_cast<PlotItem*>(track->getUserData());
            const bool visible=point && point->isVisible();
            if(visible)
                polyline<<displayPosition(track,m_altitudeMultiplier);
            else
            {
                addTrackPolyline(polyline,track->getRadarId(),track->getTrackId(),
                                 trackInstance,pathIsAdsb);
                polyline.clear();
            }

            if(track->getTrackPlotType()==NRadarTrackPlot::EndPoint)
            {
                addTrackPolyline(polyline,track->getRadarId(),track->getTrackId(),
                                 trackInstance,pathIsAdsb);
                polyline.clear();
                trackInstance=0;
            }
        }

        if(polyline.size()>1)
        {
            const NRadarTrackPlot *track=samples.last();
            addTrackPolyline(polyline,track->getRadarId(),track->getTrackId(),
                             trackInstance,pathIsAdsb);
        }
    }

    setTrackPathsVisible(connectTracks_chk->isChecked() && cmbType->currentIndex()!=1);
    updateTrackPathStyles();
}

void AppWindow::updateTrackPathStyles()
{
    const bool highlightedTrackIsVisible=m_hasHighlightedTrack &&
            ((m_trackOverlay && m_trackOverlay->containsTrackInstance(m_highlightedTrackInstance)) ||
             (m_adsbTrackOverlay && m_adsbTrackOverlay->containsTrackInstance(m_highlightedTrackInstance)));

    if(m_trackOverlay)
        m_trackOverlay->setDisplayStyle(m_blackWhiteMode,highlightedTrackIsVisible,
                                        m_highlightedTrackInstance);
    if(m_adsbTrackOverlay)
        m_adsbTrackOverlay->setDisplayStyle(m_blackWhiteMode,highlightedTrackIsVisible,
                                            m_highlightedTrackInstance);
}

void AppWindow::setHighlightedTrack(const NRadarTrackPlot *track)
{
    const quint64 trackInstance=track ? m_trackInstances.value(track,0) : 0;
    const bool hasTrack=trackInstance!=0;

    if(m_hasHighlightedTrack==hasTrack &&
            (!hasTrack || m_highlightedTrackInstance==trackInstance))
        return;

    m_hasHighlightedTrack=hasTrack;
    m_highlightedTrackInstance=trackInstance;
    updateTrackPathStyles();
}

QTreeWidgetItem* AppWindow::addItemToDetails(QTreeWidgetItem *parent,const QString& name,const QString& value)
{
	QTreeWidgetItem *item=new QTreeWidgetItem(parent);
	item->setText(0,name);

	if(!value.isNull()) item->setText(1,value);
	else item->setFirstColumnSpanned(true);

	return item;
}

//копипаста из RadarClient-а
void AppWindow::onPlotSelected(NRadarItem* radarItem)
{
    if(!radarItem || !radarItem->getLayer())
    {
        setHighlightedTrack(0);
        return;
    }

    if (QGuiApplication::mouseButtons() & Qt::RightButton)
    {
        showPlotPopup(radarItem,radarItem->scenePos());
    }

	const NRadarPlot* plot=static_cast<PlotItem*>(radarItem)->plot;
	if(!plot) return;

	QTreeWidget* tree=treeDetails;
	tree->clear();

	const NRadarTrackPlot *tplot=0;
	if(plot->getType()==NRadarAbstractPlot::TypeTrack)
		tplot=static_cast<const NRadarTrackPlot*>(plot);

    setHighlightedTrack(tplot);

	QTreeWidgetItem *root=tree->invisibleRootItem(),*item;

	int count=0;
	QString str;

	addItemToDetails(root,tr("Date"),plot->getTime().date().toString("dd/MM/yyyy"));
	addItemToDetails(root,tr("Time"),plot->getTime().time().toString("HH:mm:ss.zzz"));
	if(plot->getType()==NRadarAbstractPlot::TypeTrack)
	{
		switch(tplot->getTrackPlotType())
		{
		case NRadarTrackPlot::PredictedPoint: str=tr("Predicted"); break;
		case NRadarTrackPlot::EndPoint: str=tr("End"); break;
		case NRadarTrackPlot::NormalPoint:
			switch(plot->getSource())
			{
			case NRadarPlot::PSR: str=tr("PSR only"); break;
			case NRadarPlot::SSR: str=tr("SSR only"); break;
			case NRadarPlot::Combined: str=tr("Combined"); break;
			case NRadarPlot::ADSB: str=tr("ADS-B"); break;
			default: str="ERROR"; break;
			}
			break;
		}
		str=tr("Track - ")+str;
	}
	else
	{
		addItemToDetails(root,tr("Source"),plot->getSource()==NRadarPlot::PSR ? "PSR" : "SSR");
		count++;

		switch(plot->getPlotAssociation())
		{
		case NRadarPlot::NotAssociated: str=tr("Standalone"); break;
		case NRadarPlot::AssociatedWithPlot: str=tr("Associated"); break;
		case NRadarPlot::AssociatedWithTrack: str=tr("Assoc. to track"); break;
		}
		str=tr("Plot - ")+str;
	}
	item=addItemToDetails(root,tr("Type"),str);
	if(tplot && tplot->getTrackPlotType()!=NRadarTrackPlot::NormalPoint)
		item->setForeground(1,Qt::red);

	count+=2;

	if(tplot)
	{
		item=addItemToDetails(root,tr("Tracking data"));
		count++;

		if(plot->getSource()!=NRadarPlot::ADSB)
			addItemToDetails(item,tr("Track No"),
					QString::number(rawTrackNumber(tplot->getTrackId())));

		addItemToDetails(item,::trackFieldName(Field_Speed),NUnitsConverter::speed1kStr(tplot->getSpeed(),0));
		addItemToDetails(item,::trackFieldName(Field_Heading),NUnitsConverter::angleStr(tplot->getHeading()));
		addItemToDetails(item,::trackFieldName(Field_VerticalSpeed),NUnitsConverter::speedStr(tplot->getVerticalSpeed()));
		count+=4;
	}

	if(plot->getSource()!=NRadarPlot::PSR || (tplot && (plot->hasBoardNumber() || plot->hasHeight())))
	{
		QTreeWidgetItem *tmp;
		item=addItemToDetails(root,plot->getSource()==NRadarPlot::ADSB ? tr("ADS-B basic data") : tr("SSR basic data"));

		count++;

		if(plot->getSSRType()!=NRadarPlot::NoSSRType)
		{
			addItemToDetails(item,tr("Type"),plot->getSSRType()==NRadarPlot::ModeS ? tr("Mode S") :
									(plot->getSSRType()==NRadarPlot::ChannelRBS ? tr("RBS") : tr("UVD")));
			count++;
		}

		if(plot->hasBoardNumber())
			str=QString("%1").arg(plot->getBoardNumber(),
								  plot->getSSRType()==NRadarPlot::ChannelUVD ? 5:4,
								  10,QChar('0'));
		else str=tr("No squawk");
		if(plot->hasBoardNumber() || plot->getSource()!=NRadarPlot::ADSB)
		{
			tmp=addItemToDetails(item,::trackFieldName(Field_BoardNumber),str);
			count++;
			if(tplot && tplot->getPredictionFlags()&NRadarTrackPlot::PredictedBoardNumber)
				tmp->setForeground(1,Qt::red);
		}

		if(plot->getSSRType()==NRadarPlot::ModeS || plot->getSource()==NRadarPlot::ADSB)
		{
			QVariant v=plot->getOption(NRadarPlot::AircraftId);
			str=(v.isNull() ? tr("No identification"):v.toString());
			tmp=addItemToDetails(item,::trackFieldName(Field_AircraftId),str);
			count++;
			if(tplot && tplot->getPredictionFlags()&NRadarTrackPlot::PredictedAircraftId)
				tmp->setForeground(1,Qt::red);

			v=plot->getOption(NRadarPlot::AircraftAddress);
			str=(v.isNull() ? tr("No address") : QString::number(v.toUInt(),16).toUpper().rightJustified(6,'0'));
			tmp=addItemToDetails(item,::trackFieldName(Field_AircraftAddress),str);
			count++;
			if(v.isNull()) tmp->setForeground(1,Qt::red);
		}

		tmp=addItemToDetails(item,::trackFieldName(Field_Height),plot->hasHeight() ?
				NUnitsConverter::lengthStr(plot->getHeight()) :
				tr("No altitude"));
		count++;
		if(/*!noSSR && */tplot && tplot->getPredictionFlags()&NRadarTrackPlot::PredictedHeight)
			tmp->setForeground(1,Qt::red);

		if(plot->getSSRType()!=NRadarPlot::NoSSRType || plot->getSource()==NRadarPlot::ADSB)
		{
			addItemToDetails(item,::trackFieldName(Field_SPI),plot->hasSPI()?tr("Yes"):tr("No"));
			addItemToDetails(item,::trackFieldName(Field_Alert),plot->hasSOS()?tr("Yes"):tr("No"));
			count+=2;
		}
	}

	if(plot->hasOptions())
	{
		item=addItemToDetails(root,plot->getSource()==NRadarPlot::ADSB ? tr("ADS-B extended data"):tr("Mode S extended data"));

		const NRadarPlot::OptionMap& map=plot->getOptions();
		NRadarPlot::OptionMapIterator it(map);
		while(it.hasNext())
		{
			it.next();

			QString v,n=tr("Unnamed field");
			switch(it.key())
			{
			case NRadarPlot::Quality_NIC:
				n=::trackFieldName(Field_QualityPosition);
				v=QString("%1 / %2").arg(it.value().toInt()).arg(map.value(NRadarPlot::Quality_NACp).toInt());
				break;
			case NRadarPlot::Quality_NACv: n=::trackFieldName(Field_QualityVelocity); break;
			case NRadarPlot::Quality_SIL: n=::trackFieldName(Field_QualitySIL); break;

			case NRadarPlot::MOPSVersion:
				n=::trackFieldName(Field_MOPSVersion);
				switch(it.value().toInt())
				{
				case 0:	 v="DO-260"; break;
				case 1:  v="DO-260A"; break;
				case 2:  v="DO-260B"; break;
				default: v="unknown ["+it.value().toString()+"]"; break;
				}
				break;

			case NRadarPlot::EmitterCategory: n=::trackFieldName(Field_EmitterCategory); break;

			case NRadarPlot::OnGround:
				n=::trackFieldName(Field_OnGround);
				v=(it.value().toBool()?tr("Yes"):tr("No"));
				break;

			case NRadarPlot::ModeSServicesCap:
				n=::trackFieldName(Field_ModeSServicesCap);
				v=(it.value().toBool()?tr("Yes"):tr("No"));
				break;
			case NRadarPlot::AircraftIdCap:
				n=::trackFieldName(Field_AircraftIdCap);
				v=(it.value().toBool()?tr("Yes"):tr("No"));
				break;

			case NRadarPlot::MCPSelectedAltitude:
				n=::trackFieldName(Field_MCPSelectedAltitude);
				v=NUnitsConverter::lengthStr(it.value().toDouble());
				break;
			case NRadarPlot::FMSSelectedAltitude:
				n=::trackFieldName(Field_FMSSelectedAltitude);
				v=NUnitsConverter::lengthStr(it.value().toDouble());
				break;

			case NRadarPlot::TargetAltitudeSource:
				n=::trackFieldName(Field_TargetAltitudeSource);
				switch(it.value().toInt())
				{
				case 0: v="Aircraft alt."; break;
				case 1: v="FCU/MCP alt."; break;
				case 2: v="FMS alt."; break;
				default: v="Unknown"; break;
				}
				break;

			case NRadarPlot::BarometricPressureSetting:
				n=::trackFieldName(Field_BarometricPressureSetting);
				v=QString::number(it.value().toInt())+" mb";
				break;

			case NRadarPlot::ApproachMode:
				n=::trackFieldName(Field_ApproachMode);
				v=(it.value().toBool()?tr("Yes"):tr("No"));
				break;

			case NRadarPlot::AltHoldMode:
				n=::trackFieldName(Field_AltHoldMode);
				v=(it.value().toBool()?tr("Yes"):tr("No"));
				break;

			case NRadarPlot::VNAVMode:
				n=::trackFieldName(Field_VNAVMode);
				v=(it.value().toBool()?tr("Yes"):tr("No"));
				break;

			case NRadarPlot::RollAngle:
				n=::trackFieldName(Field_RollAngle);
				v=NUnitsConverter::angleStr(it.value().toDouble(),1);
				break;

			case NRadarPlot::TrueTrackAngle:
				n=::trackFieldName(Field_TrueTrackAngle);
				v=NUnitsConverter::angleStr(it.value().toDouble());
				break;

			case NRadarPlot::GroundSpeed:
				n=::trackFieldName(Field_GroundSpeed);
				v=NUnitsConverter::speed1kStr(it.value().toDouble());
				break;

			case NRadarPlot::TrackAngleRate:
				n=::trackFieldName(Field_TrackAngleRate);
				v=QString("%1 %2/%3").arg(it.value().toDouble(),0,'f',1).arg(QChar(0xB0)).arg(tr("s"));
				break;

			case NRadarPlot::TrueAirspeed:
				n=::trackFieldName(Field_TrueAirspeed);
				v=NUnitsConverter::speed1kStr(it.value().toDouble());
				break;

			case NRadarPlot::MagneticHeading:
				n=::trackFieldName(Field_MagneticHeading);
				v=NUnitsConverter::angleStr(it.value().toDouble());
				break;

			case NRadarPlot::IndicatedAirspeed:
				n=::trackFieldName(Field_IndicatedAirspeed);
				v=NUnitsConverter::speed1kStr(it.value().toDouble());
				break;

			case NRadarPlot::Mach:
				n=::trackFieldName(Field_Mach);
				v=QString::number(it.value().toDouble(),'f',3);
				break;

			case NRadarPlot::BarometricAltitudeRate:
				n=::trackFieldName(Field_BarometricAltitudeRate);
				v=NUnitsConverter::speedStr(it.value().toDouble());
				break;

			case NRadarPlot::IntertialVerticalVelocity:
				n=::trackFieldName(Field_IntertialVerticalVelocity);
				v=NUnitsConverter::speedStr(it.value().toDouble());
				break;

			default: continue;
			}
			if(v.isNull()) v=it.value().toString();

			addItemToDetails(item,n,v);
			count++;
		}
	}

	item=addItemToDetails(root,tr("Position"));
	count++;
	if(tplot && tplot->getTrackPlotType()==NRadarTrackPlot::PredictedPoint)
		item->setForeground(0,Qt::red);

	QPointF pt=plot->getADCoord();
	addItemToDetails(item,tr("Azimuth"),NUnitsConverter::angleStr(pt.x(),1));
	addItemToDetails(item,tr("Distance"),NUnitsConverter::length1kStr(pt.y()/1000.0,3));
	addItemToDetails(item,tr("X"),NUnitsConverter::length1kStr(plot->getXYCoord().x()/1000.0,3));
	addItemToDetails(item,tr("Y"),NUnitsConverter::length1kStr(plot->getXYCoord().y()/1000.0,3));
	count+=4;

	if(plot->hasHeight())
	{
		addItemToDetails(item,tr("Elevation angle"),NUnitsConverter::angleStr(atan(plot->getHeight()/pt.y())*180.0/M_PI,1));
		count++;
	}

	int ampl=plot->getOption(NRadarPlot::RAW_SSRAmplitude).toInt();
	if(ampl>0) addItemToDetails(item,tr("Amplitude"),QString("%1 dBm [%2]").arg(qRound(20*log10(ampl/18.0)-102)).arg((ampl)));

	tree->expandAll();
}

/////////////

void AppWindow::setTaskProgress(int value, bool on)
{
	int v=barWorking->value();
	bool o=barWorking->isVisible();

	barWorking->setValue(value);
	barWorking->setVisible(on);

	if(v!=value || o!=on)
		qApp->processEvents();
}

const QList<NRadarAbstractPlot*>& AppWindow::getAllData() const
{
	return dataPack->getData();
}

const NRadarMap* AppWindow::getActiveMap() const
{
	return radarScene->getMap();
}

NRadarScene* AppWindow::getRadarScene() const
{
	return radarScene;
}

IAnalyser::SourceSelection AppWindow::getSelectedSource() const
{
    return static_cast<IAnalyser::SourceSelection>(cmbSource->currentIndex());
}

IAnalyser::ModeSSelection AppWindow::getSelectedModeS() const
{
    return static_cast<IAnalyser::ModeSSelection>(cmbModeS->currentIndex());
}

QDateTime AppWindow::getSelectedBeginTime() const
{
    return dateBegin->dateTime();
}

QDateTime AppWindow::getSelectedEndTime() const
{
    return dateEnd->dateTime();
}

bool AppWindow::getShowPredictedTrackPoints() const
{
    return predictedTracks_chk->isChecked();
}

quint64 AppWindow::getTrackInstanceId(const NRadarTrackPlot *track) const
{
    return m_trackInstances.value(track,0);
}

bool AppWindow::eventFilter(QObject *obj,QEvent *event)
{
    if(obj==radarView->viewport())
    {
        if(event->type()==QEvent::MouseButtonPress)
        {
            QMouseEvent *mouseEvent=static_cast<QMouseEvent*>(event);
            int popupIndex=findPopupAtPos(mouseEvent->pos());

            if(popupIndex>=0)
            {
                if(mouseEvent->button()==Qt::RightButton)
                {
                    closePlotPopup(popupIndex);
                    return true;
                }
                if(mouseEvent->button()==Qt::LeftButton)
                {
                    QPointF scenePos=radarView->mapToScene(mouseEvent->pos());
                    m_draggingPopupIndex=popupIndex;
                    m_popupDragOffset=scenePos-m_plotPopups[popupIndex].proxy->pos();
                    return true;
                }
            }
        }
        else if(event->type()==QEvent::MouseMove && m_draggingPopupIndex>=0)
        {
            QMouseEvent *mouseEvent=static_cast<QMouseEvent*>(event);
            QPointF scenePos=radarView->mapToScene(mouseEvent->pos());
            m_plotPopups[m_draggingPopupIndex].proxy->setPos(scenePos-m_popupDragOffset);
            return true;
        }
        else if(event->type()==QEvent::MouseButtonRelease && m_draggingPopupIndex>=0)
        {
            m_draggingPopupIndex=-1;
            return true;
        }
    }

    return QMainWindow::eventFilter(obj,event);
}

void AppWindow::showPlotPopup(NRadarItem *radarItem,const QPointF& scenePos)
{
    if(!radarItem) return;

    PlotLabel *label=new PlotLabel(treeDetails);

    PlotPopupData popupData;
    popupData.proxy=new QGraphicsProxyWidget();
    popupData.proxy->setWidget(label);
    popupData.proxy->setFlag(QGraphicsItem::ItemIgnoresTransformations,true);
    popupData.proxy->setZValue(10000.0+m_plotPopups.size());
    radarScene->addItem(popupData.proxy);

    connect(popupData.proxy,&QGraphicsObject::xChanged,this,&AppWindow::updatePlotPopupLines);
    connect(popupData.proxy,&QGraphicsObject::yChanged,this,&AppWindow::updatePlotPopupLines);

    QPen pen(Qt::yellow,0);
    pen.setCosmetic(true);
    popupData.line=radarScene->addLine(QLineF(),pen);
    popupData.line->setZValue(9999.0);

    popupData.radarItem=radarItem;
    populatePlotPopup(label, radarItem);

    popupData.proxy->setPos(scenePos+QPointF(20,20));

    m_plotPopups.append(popupData);
    updatePlotPopupLines();
}

void AppWindow::populatePlotPopup(PlotLabel *label, NRadarItem *radarItem)
{
    if (!label || !radarItem)
        return;

    PlotItem *plotItem = dynamic_cast<PlotItem *>(radarItem);
    if (!plotItem || !plotItem->plot)
        return;

    const NRadarPlot *plot = plotItem->plot;

    QString source;
    switch (plot->getSource())
    {
    case NRadarPlot::PSR:      source = "PSR";      break;
    case NRadarPlot::Combined: source = "Combined"; break;
    case NRadarPlot::ADSB:     source = "ADS-B";    break;
    case NRadarPlot::SSR:
        if (plot->getSSRType() == NRadarPlot::ChannelRBS)
            source = "A/C";
        else if (plot->getSSRType() == NRadarPlot::ModeS)
            source = "Mode S";
        else
            source = "Unknown SSR";
        break;
    default:
        source = "Unknown";
        break;
    }

    QString type;
    if (plot->getType() == NRadarAbstractPlot::TypeTrack)
    {
        const auto *tplot = static_cast<const NRadarTrackPlot *>(plot);

        switch (tplot->getTrackPlotType())
        {
        case NRadarTrackPlot::PredictedPoint: type = "Predicted"; break;
        case NRadarTrackPlot::EndPoint:       type = "End";       break;
        case NRadarTrackPlot::NormalPoint:    break;
        }
        source = "Track, " + source;
    }
    else
    {
        if (plot->getSSRType() == NRadarPlot::ModeS)
        {
            if (plot->getOption(NRadarPlot::AllCall).toBool())
                type = "All-call, ";
            else
                type = "Roll-call, ";
        }

        switch (plot->getPlotAssociation())
        {
        case NRadarPlot::NotAssociated:       type += "Not assoc.";       break;
        case NRadarPlot::AssociatedWithPlot:  type += "Assoc.";           break;
        case NRadarPlot::AssociatedWithTrack: type += "Assoc. to track";  break;
        }
        source = "Plot, " + source;
    }

    QString squawk;
    if (plot->getSource() != NRadarPlot::PSR)
    {
        if (plot->hasBoardNumber())
        {
            int digits = (plot->getSSRType() == NRadarPlot::ChannelUVD) ? 5 : 4;
            squawk = QString("%1").arg(plot->getBoardNumber(), digits, 10, QChar('0'));
        }
        else
        {
            squawk = "No squawk";
        }

        uint address = plot->getOption(NRadarPlot::AircraftAddress).toUInt();
        if (address)
            squawk += " / " + QString("%1").arg(address, 6, 16, QChar('0'));
        else
            squawk += " / No address";
    }

    QPointF pt = plot->getADCoord();

    QVector<QString> entries;
    entries << plot->getTime().date().toString("dd/MM/yyyy") + " / " + plot->getTime().time().toString("HH:mm:ss.zzz");
    entries << source;
    if (!type.isEmpty())
        entries << type;
    if (!squawk.isEmpty())
        entries << squawk;
    entries << NUnitsConverter::angleStr(pt.x(), 1) + " / " + NUnitsConverter::length1kStr(pt.y() / 1000.0, 3);
    if (plot->hasHeight())
        entries << tr("Alt: %1").arg(NUnitsConverter::lengthStr(plot->getHeight()));

    label->setEntries(entries);
}

void AppWindow::updatePlotPopupLines()
{
    for(const auto &popup : m_plotPopups)
    {
        if(!popup.proxy || !popup.line || !popup.radarItem) continue;

        QPointF start=popup.proxy->sceneBoundingRect().topLeft();
        QPointF end=popup.radarItem->scenePos();
        popup.line->setLine(QLineF(start,end));
    }
}

void AppWindow::closePlotPopup(int index)
{
    if(index<0 || index>=m_plotPopups.size()) return;

    PlotPopupData &popup=m_plotPopups[index];

    delete popup.line;
    delete popup.proxy;

    m_plotPopups.removeAt(index);

    if(m_draggingPopupIndex==index)
        m_draggingPopupIndex=-1;
    else if(m_draggingPopupIndex>index)
        m_draggingPopupIndex--;
}

void AppWindow::closeAllPlotPopups()
{
    for(int i=m_plotPopups.size()-1; i>=0; --i)
        closePlotPopup(i);
}

int AppWindow::findPopupAtPos(const QPoint& viewPos)
{
    QList<QGraphicsItem*> itemsAtPos=radarView->items(viewPos);

    for(int i=m_plotPopups.size()-1; i>=0; --i)
    {
        if(itemsAtPos.contains(m_plotPopups[i].proxy))
            return i;
    }
    return -1;
}

//set plot color
void AppWindow::setPlotColor(const NRadarAbstractPlot* plot,const QColor& color)
{
	if(!plot->getUserData()) return;

	((PlotItem*)(plot->getUserData()))->fixedColor=color;
}

/////////////

void AppWindow::executeTask(QAction *act)
{
	int id=act->data().toUInt();
	if(id>=tasks.size()) return;

	const bool firstStage=act->isChecked();
	if(!tasks[id]->execute(firstStage))
	{
		if(tasks[id]->getType()==AnalyserTask::TwoStages)
			act->setChecked(!firstStage);
		return;
	}
	rebuildTrackPaths();
	if(tasks[id]->getType()==AnalyserTask::TwoStages)
		act->setText(tasks[id]->getName(!firstStage));
}
