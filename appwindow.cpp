
#include <radardata/nradartrackplot.h>
#include <radarview/nunitsconverter.h>
#include <radarview/nradardecoration.h>
#include <radarview/nradaritem.h>

#include "commondefs.h"

#include "appwindow.h"

#include "datapack.h"
#include "datasourcedlg.h"

#include "guifilter.h"

#include "podtask.h"
#include "reflectorstask.h"
#include "sampletask.h"
#include "amplitudestask.h"
#include "falsepsrtask.h"
#include "ampscattertask.h"
#include "duplicatetask.h"
#include "ampfiltertask.h"

class PlotItem:public NRadarItem
{
public:
	PlotItem(const QPointF& pos, const NRadarPlot *plot,int type,const QColor *notAssociatedColor) :
        NRadarItem(NRadarItem::Point),
        m_notAssociatedColor(notAssociatedColor)
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
        if(option->state&QStyle::State_Selected)
            return Qt::red; //Qt::white;

        if(fixedColor.isValid())
            return fixedColor;

        if (plot->getSource() == 2)
        {
            if (plot->getPlotAssociation() == NRadarPlot::NotAssociated)
                return Qt::black; //(m_notAssociatedColor ? *m_notAssociatedColor : Qt::gray);

            if (plot->getSSRType()==NRadarPlot::ModeS && (plot->getOption(NRadarPlot::AllCall)).toBool())
                return Qt::black; //Qt::cyan;

            if (plot->getSSRType() == NRadarPlot::ModeS)
                return Qt::black; //Qt::blue;

            return Qt::black; //Qt::yellow;
        }

        switch(type)
			{
            case 1: return Qt::black; //Qt::red;
            //case 2: return Qt::yellow;
            case 3: return Qt::black; //QColor(0,162,232);

            case 12: return Qt::black; //QColor(255,128,0);
            case 13: return Qt::black; //QColor(0,255,255);
            case 14: return Qt::black; //Qt::green;
			}
        return Qt::black; //Qt::white;
	}

	void switchToGeoPos(int alt_mul=0)
	{
		QPointF pt;
		if(alt_mul<=0) pt = posGeo;
		else pt = QPointF(plot->getADCoord().y(), alt_mul*(plot->hasHeight() ? plot->getHeight() : -100.0));
		setPos(pt);
	}

//protected:
	int type;
	const NRadarPlot *plot;
	QPointF posGeo;
	QColor fixedColor;
    const QColor *m_notAssociatedColor;
};

AppWindow::AppWindow():QMainWindow(0),
	m_notAssociated(Qt::gray)
{
	setupUi(this);

	radarScene=new NRadarScene(NRadarMap::createMap(60,30,650000));
	radarView->setScene(radarScene);
	radarView->setScaleLimits(1);
	radarScene->getMap()->initTiles();

	//radarView->set3DAllowed(true);

	barWorking=new QProgressBar(this);
	barWorking->setRange(0,100);
	statusBar()->addWidget(barWorking);
	barWorking->setVisible(false);

	tasks<<new SampleTask(this);
	tasks<<new PoDTask(this);
	tasks<<new ReflectorsTask(this);
	tasks<<new AmplitudesTask(this);
	tasks<<new FalsePSRTask(this);
    tasks<<new ampScatterTask(this);
    tasks<<new DuplicateTask(this);
    tasks<<new AmpFilterTask(this);

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

	dataPack=new DataPack();

	if(!importData(false)) return;

	QTimer::singleShot(10,this,SLOT(applyFilter()));
}

AppWindow::~AppWindow()
{
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

	radarScene->clear();
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

    QMessageBox::information(this, "Warning", "Dont forget to switch on the Satellite view to have the white background color!");

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

	radarView->showFullMap();
}

void AppWindow::initFilters(const DataPack *data)
{
	QMap<QString,bool> modeAList,addressList,idList,trackList;
	QMap<uint,bool> tmp1,tmp2,tmp3;

	foreach(NRadarAbstractPlot* ap,data->getData())
	{
		if(ap->getType()!=NRadarAbstractPlot::TypePlot && ap->getType()!=NRadarAbstractPlot::TypeTrack)
			continue;

		NRadarPlot* plot=static_cast<NRadarPlot*>(ap);
		if(plot->getSource()==NRadarPlot::PSR) continue;

		uint modeA=plot->getBoardNumber();
		if(modeA) tmp1[modeA]=true;

		uint address=plot->getOption(NRadarPlot::AircraftAddress).toUInt();
		if(address) tmp2[address]=true;

		if(plot->getType()==NRadarAbstractPlot::TypePlot)
			foreach(uint tid,plot->getAssociatedTrackIds())
				tmp3[tid]=true;
		else if(plot->getSource()!=NRadarPlot::ADSB)
			tmp3[static_cast<NRadarTrackPlot*>(ap)->getTrackId()]=true;

		QString id=plot->getOption(NRadarPlot::AircraftId).toString().toUpper();
		if(id.length()) idList[id]=true;
	}

	modeAList["N/A"]=true;
	foreach(uint u,tmp1.keys())
		modeAList[QString("%1").arg(u,4,10,QChar('0'))]=true;

	foreach(uint u,tmp2.keys())
		addressList[QString("%1").arg(u,6,16,QChar('0'))]=true;

	foreach(uint u,tmp3.keys())
		trackList[QString::number(u)]=true;

	fillComboBox(cmbModeA,modeAList);
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

	uint modeA=(cmbModeA->currentIndex()==0 ? 9999 : cmbModeA->currentText().toUInt());
	uint address=(cmbAddress->currentIndex()==0 ? 0 : cmbAddress->currentText().toUInt(0,16));
	QString aircraftId=(cmbAircraftID->currentIndex() ? cmbAircraftID->currentText() : "");
	uint trackNo=(cmbTrackNo->currentIndex()==0 ? 0 : cmbTrackNo->currentText().toUInt());

	uint begin=dateBegin->dateTime().toTime_t();
	uint end=dateEnd->dateTime().toTime_t();

	uint modeS=cmbModeS->currentIndex();

	layers[1]->setVisible(src<=1 && type<=1);
	layers[2]->setVisible((src==0 || src==2) && type<=1);
	layers[3]->setVisible(src==0 || src==3);
	layers[11]->setVisible(src!=3 && type!=1);

	if(!fullFilter) return;

	barWorking->setValue(0);
	barWorking->setVisible(true);
	lastWorkingUpdate.start();
	qApp->processEvents();

	radarView->setUpdatesEnabled(false);

	const QPolygon& area=qobject_cast<PolygonFilter*>(areaFilter)->getPolygon();

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
		NRadarTrackPlot* tplot=static_cast<NRadarTrackPlot*>(data);
		PlotItem *pi=static_cast<PlotItem*>(data->getUserData());

		bool filterOut=false;

		if(!filterOut && type!=1 && (src==1 || src==2) && data->getType()==NRadarAbstractPlot::TypeTrack)
		{
			NRadarPlot::NPlotSourceType s=plot->getSource();
			bool ok=false;
			if(src==1 && (s==NRadarPlot::PSR || s==NRadarPlot::Combined)) ok=true;
			if(src==2 && (s==NRadarPlot::SSR || s==NRadarPlot::Combined)) ok=true;
			if(!ok) filterOut=true;
		}

		if(!filterOut && modeS)
			filterOut = (modeS==1 && plot->getSSRType()==NRadarPlot::ModeS) || (modeS==2 && plot->getSSRType()!=NRadarPlot::ModeS);

		if(!filterOut && modeA!=9999)
			filterOut = (plot->getBoardNumber()!=modeA);

		if(!filterOut && trackNo)
		{
			if(data->getType()==NRadarAbstractPlot::TypePlot)
				filterOut = !plot->getAssociatedTrackIds().contains(trackNo);
			else
				filterOut = (tplot->getTrackId()!=trackNo);
		}

		if(!filterOut && address)
			filterOut = (plot->getOption(NRadarPlot::AircraftAddress).toUInt() != address);

		if(!filterOut && aircraftId.length())
			filterOut = (plot->getOption(NRadarPlot::AircraftId).toString() != aircraftId);

		if(!filterOut)
		{
			uint t=plot->getTime().toTime_t();
			filterOut = (t<begin || t>end);
		}

		if(!filterOut && !area.isEmpty() && !area.containsPoint(plot->getXYCoord(),Qt::OddEvenFill))
			filterOut=true;

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
				pi=new PlotItem(plot->getXYCoord(),plot,id==11 ? id+plot->getSource() : id,&m_notAssociated);
				radarScene->addItem(pi,layers[id]);
			}
		}
		plot->setUserData(pi);
	}

//    qDebug()<<bench.elapsed()<<"ms for"<<dataPack->getData().size()<<"items";
//    if(bench.elapsed())
//        qDebug()<<"==="<<((qint64)dataPack->getData().size()*1000 / bench.elapsed())<<"items/s";

//    qDebug()<<"SPI"<<spi;
	barWorking->setVisible(false);
	radarView->setUpdatesEnabled(true);
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
	if(!radarItem || !radarItem->getLayer()) return;

	const NRadarPlot* plot=static_cast<PlotItem*>(radarItem)->plot;
	if(!plot) return;

	QTreeWidget* tree=treeDetails;
	tree->clear();

	const NRadarTrackPlot *tplot=0;
	if(plot->getType()==NRadarAbstractPlot::TypeTrack)
		tplot=static_cast<const NRadarTrackPlot*>(plot);

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
			addItemToDetails(item,tr("Track No"),QString::number(tplot->getTrackId()));

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

	tasks[id]->execute(act->isChecked());
	if(tasks[id]->getType()==AnalyserTask::TwoStages)
		act->setText(tasks[id]->getName(!act->isChecked()));
}
