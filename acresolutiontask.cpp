#include <libradardata/nradarplot.h>

#include <QAbstractItemView>
#include <QBrush>
#include <QCheckBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPointer>
#include <QTableWidget>
#include <QVariant>
#include <QVBoxLayout>
#include <QWidget>

#include "acresolutiontask.h"

namespace
{

const int ProbabilityRole=Qt::UserRole;
const int ManuallyExcludedRole=Qt::UserRole+1;
const int ThresholdDisabledRole=Qt::UserRole+2;

bool isModeRowExcluded(const QTableWidgetItem *codeItem)
{
    return codeItem->data(ManuallyExcludedRole).toBool() ||
            codeItem->data(ThresholdDisabledRole).toBool();
}

double detectionProbability(quint64 detectedPlots,quint64 expectedPlots)
{
    return expectedPlots ?
            qMin(100.0,100.0*detectedPlots/expectedPlots) : 0.0;
}

void setDetectionProbability(QTableWidgetItem *item,double probability)
{
    item->setText(QString::number(probability,'f',2));
    item->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
    item->setData(ProbabilityRole,probability);

    if(probability<30.0)
        item->setBackground(QBrush(Qt::red));
    else if(probability<96.0)
        item->setBackground(QBrush(Qt::yellow));
    else
        item->setBackground(QBrush(Qt::green));
}

void updateAverageProbability(QTableWidget *table,int modeRowCount)
{
    double probabilitySum=0.0;
    int includedCount=0;
    for(int row=0;row<modeRowCount;row++)
    {
        QTableWidgetItem *codeItem=table->item(row,0);
        QTableWidgetItem *pdItem=table->item(row,2);
        if(!codeItem || !pdItem || isModeRowExcluded(codeItem))
            continue;

        const QVariant probability=pdItem->data(ProbabilityRole);
        if(!probability.isValid())
            continue;

        probabilitySum+=probability.toDouble();
        includedCount++;
    }

    QTableWidgetItem *averagePdItem=table->item(modeRowCount,2);
    if(!averagePdItem)
        return;

    averagePdItem->setText(QString());
    averagePdItem->setBackground(QBrush());
    if(includedCount)
        setDetectionProbability(averagePdItem,probabilitySum/includedCount);
}

void updateDetectionProbabilities(QTableWidget *table,int modeRowCount,
                                  quint64 expectedPlots)
{
    for(int row=0;row<modeRowCount;row++)
    {
        QTableWidgetItem *codeItem=table->item(row,0);
        QTableWidgetItem *countItem=table->item(row,1);
        QTableWidgetItem *pdItem=table->item(row,2);
        if(!codeItem || !countItem || !pdItem)
            continue;

        if(expectedPlots)
        {
            const double probability=detectionProbability(
                    countItem->text().toULongLong(),expectedPlots);
            setDetectionProbability(pdItem,probability);
        }
        else
        {
            pdItem->setText(QString());
            pdItem->setData(ProbabilityRole,QVariant());
            pdItem->setBackground(QBrush());
        }

        if(isModeRowExcluded(codeItem))
            pdItem->setBackground(QBrush(Qt::lightGray));
    }

    updateAverageProbability(table,modeRowCount);
}

void updateModeRowAppearance(QTableWidget *table,int row)
{
    QTableWidgetItem *codeItem=table->item(row,0);
    if(!codeItem)
        return;

    const bool thresholdDisabled=
            codeItem->data(ThresholdDisabledRole).toBool();
    const bool excluded=isModeRowExcluded(codeItem);
    const QBrush rowBackground=excluded ? QBrush(Qt::lightGray) : QBrush();
    for(int column=0;column<table->columnCount();column++)
    {
        QTableWidgetItem *item=table->item(row,column);
        if(item)
        {
            Qt::ItemFlags flags=item->flags();
            if(thresholdDisabled)
                flags&=~(Qt::ItemIsEnabled|Qt::ItemIsSelectable);
            else
                flags|=Qt::ItemIsEnabled|Qt::ItemIsSelectable;
            item->setFlags(flags);
            item->setBackground(rowBackground);
        }
    }

    if(!excluded)
    {
        QTableWidgetItem *pdItem=table->item(row,2);
        if(pdItem)
        {
            const QVariant probability=pdItem->data(ProbabilityRole);
            if(probability.isValid())
                setDetectionProbability(pdItem,probability.toDouble());
        }
    }
}

void setModeRowManuallyExcluded(QTableWidget *table,int row,bool excluded)
{
    QTableWidgetItem *codeItem=table->item(row,0);
    if(!codeItem)
        return;

    codeItem->setData(ManuallyExcludedRole,excluded);
    updateModeRowAppearance(table,row);
}

void updateThresholdDisabledRows(QTableWidget *table,int modeRowCount,
                                 bool enabled,quint64 minimumPlots)
{
    for(int row=0;row<modeRowCount;row++)
    {
        QTableWidgetItem *codeItem=table->item(row,0);
        QTableWidgetItem *countItem=table->item(row,1);
        if(!codeItem || !countItem)
            continue;

        const bool disabled=enabled &&
                countItem->text().toULongLong()<minimumPlots;
        codeItem->setData(ThresholdDisabledRole,disabled);
        updateModeRowAppearance(table,row);
    }

    table->clearSelection();
    updateAverageProbability(table,modeRowCount);
}

}

ACResolutionTask::ACResolutionTask(IAnalyser *analyser):AnalyserTask(analyser)
{
}

QString ACResolutionTask::getName(bool firstStage) const
{
    Q_UNUSED(firstStage);
    return tr("A/C resolution");
}

bool ACResolutionTask::execute(bool firstStage)
{
    Q_UNUSED(firstStage);

    QWidget *resultWidget=new QWidget;
    resultWidget->setAttribute(Qt::WA_DeleteOnClose);
    resultWidget->setWindowTitle(tr("A/C resolution"));

    QVBoxLayout *mainLayout=new QVBoxLayout(resultWidget);
    QHBoxLayout *expectedPlotsLayout=new QHBoxLayout;
    QLabel *expectedPlotsLabel=new QLabel(tr("Expected number of plots: "),resultWidget);
    QLineEdit *expectedPlotsEdit=new QLineEdit(QString::number(10),resultWidget);
    expectedPlotsEdit->setValidator(new QIntValidator(0,999999999,expectedPlotsEdit));
    expectedPlotsEdit->setAlignment(Qt::AlignRight);
    expectedPlotsEdit->setFixedWidth(80);
    expectedPlotsLabel->setAlignment(Qt::AlignRight|Qt::AlignVCenter);

    expectedPlotsLayout->addStretch();
    expectedPlotsLayout->addWidget(expectedPlotsLabel);
    expectedPlotsLayout->addWidget(expectedPlotsEdit);
    mainLayout->addLayout(expectedPlotsLayout);

    QHBoxLayout *disableSmallPlotsLayout=new QHBoxLayout;
    QCheckBox *disableSmallPlotsCheckBox=new QCheckBox(resultWidget);
    QLabel *disableSmallPlotsLabel=new QLabel(
            tr("Disable for plots less then:"),resultWidget);
    QLineEdit *disableSmallPlotsEdit=new QLineEdit(QString::number(5),resultWidget);
    disableSmallPlotsCheckBox->setChecked(false);
    disableSmallPlotsEdit->setValidator(
            new QIntValidator(0,999999999,disableSmallPlotsEdit));
    disableSmallPlotsEdit->setAlignment(Qt::AlignRight);
    disableSmallPlotsEdit->setFixedWidth(80);

    disableSmallPlotsLayout->addStretch();
    disableSmallPlotsLayout->addWidget(disableSmallPlotsCheckBox);
    disableSmallPlotsLayout->addWidget(disableSmallPlotsLabel);
    disableSmallPlotsLayout->addWidget(disableSmallPlotsEdit);
    mainLayout->addLayout(disableSmallPlotsLayout);

    QTableWidget *table=new QTableWidget(0,3,resultWidget);
    table->setHorizontalHeaderLabels(QStringList()
            << tr("Code A") << tr("Det. plots") << tr("Pd, %"));
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    mainLayout->addWidget(table);

    resultWidget->resize(500,400);
    resultWidget->show();

    QPointer<QTableWidget> guardedTable(table);
    QPointer<QLineEdit> guardedExpectedPlotsEdit(expectedPlotsEdit);
    QMap<uint,quint64> plotCounts;
    const QList<NRadarAbstractPlot*>& allData=analyser->getAllData();
    const int total=allData.size();

    analyser->setTaskProgress(0);
    for(int i=0;i<total;i++)
    {
        if(i && i%10000==0)
            analyser->setTaskProgress(int((qint64(i)*100)/total));

        NRadarAbstractPlot *data=allData.at(i);
        if(data->getType()!=NRadarAbstractPlot::TypePlot ||
                !analyser->isPlotVisible(data))
            continue;

        const NRadarPlot *plot=static_cast<const NRadarPlot*>(data);
        if(plot->getSource()!=NRadarPlot::SSR ||
                plot->getSSRType()==NRadarPlot::ModeS ||
                !plot->hasBoardNumber())
            continue;

        plotCounts[plot->getBoardNumber()]++;
    }
    analyser->setTaskProgress(0,false);

    if(guardedTable && guardedExpectedPlotsEdit)
    {
        quint64 expectedPlots=0;
        foreach(quint64 count,plotCounts)
            expectedPlots=qMax(expectedPlots,count);
        guardedExpectedPlotsEdit->setText(QString::number(expectedPlots));

        guardedTable->setRowCount(plotCounts.size()+1);
        int row=0;
        for(QMap<uint,quint64>::const_iterator it=plotCounts.constBegin();
                it!=plotCounts.constEnd();++it,++row)
        {
            QTableWidgetItem *codeItem=new QTableWidgetItem(
                    QString("%1").arg(it.key(),4,10,QChar('0')));
            QTableWidgetItem *countItem=new QTableWidgetItem(QString::number(it.value()));
            QTableWidgetItem *pdItem=new QTableWidgetItem;

            codeItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);
            countItem->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);

            const double probability=detectionProbability(
                    it.value(),expectedPlots);
            setDetectionProbability(pdItem,probability);

            guardedTable->setItem(row,0,codeItem);
            guardedTable->setItem(row,1,countItem);
            guardedTable->setItem(row,2,pdItem);
        }

        QTableWidgetItem *averageCodeItem=new QTableWidgetItem;
        QTableWidgetItem *averageCountItem=new QTableWidgetItem;
        QTableWidgetItem *averagePdItem=new QTableWidgetItem;

        guardedTable->setItem(row,0,averageCodeItem);
        guardedTable->setItem(row,1,averageCountItem);
        guardedTable->setItem(row,2,averagePdItem);

        const int modeRowCount=plotCounts.size();
        updateAverageProbability(guardedTable,modeRowCount);
        QTableWidget *resultTable=guardedTable.data();
        QLineEdit *expectedPlotsField=guardedExpectedPlotsEdit.data();
        connect(disableSmallPlotsCheckBox,&QCheckBox::toggled,resultTable,
                [resultTable,disableSmallPlotsEdit,modeRowCount](bool checked)
        {
            bool ok=false;
            const quint64 minimumPlots=
                    disableSmallPlotsEdit->text().toULongLong(&ok);
            updateThresholdDisabledRows(resultTable,modeRowCount,checked,
                    ok ? minimumPlots : 0);
        });
        connect(disableSmallPlotsEdit,&QLineEdit::textChanged,resultTable,
                [resultTable,disableSmallPlotsCheckBox,modeRowCount](
                const QString& text)
        {
            if(!disableSmallPlotsCheckBox->isChecked())
                return;

            bool ok=false;
            const quint64 minimumPlots=text.toULongLong(&ok);
            updateThresholdDisabledRows(resultTable,modeRowCount,true,
                    ok ? minimumPlots : 0);
        });
        connect(expectedPlotsField,&QLineEdit::textChanged,resultTable,
                [resultTable,modeRowCount](const QString& text)
        {
            bool ok=false;
            const quint64 expectedPlots=text.toULongLong(&ok);
            updateDetectionProbabilities(resultTable,modeRowCount,
                    ok ? expectedPlots : 0);
        });
        connect(resultTable,&QTableWidget::cellClicked,resultTable,
                [resultTable,modeRowCount](int clickedRow,int)
        {
            resultTable->clearSelection();
            if(clickedRow<0 || clickedRow>=modeRowCount)
                return;

            QTableWidgetItem *codeItem=resultTable->item(clickedRow,0);
            if(!codeItem)
                return;
            if(codeItem->data(ThresholdDisabledRole).toBool())
                return;

            const bool excluded=
                    !codeItem->data(ManuallyExcludedRole).toBool();
            setModeRowManuallyExcluded(resultTable,clickedRow,excluded);
            updateAverageProbability(resultTable,modeRowCount);
        });
    }

    emit finished(true);
    return true;
}
