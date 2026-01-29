
#include "guifilter.h"

GuiFilter::GuiFilter(QWidget *wdgInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot):QObject(0)
{
    this->wdgInput=wdgInput;
    this->btnClear=btnClear;

    connect(btnClear,SIGNAL(clicked()),this,SLOT(clear()));
    connect(this,SIGNAL(update()),updateObj,updateSlot);
}

void GuiFilter::set()
{
    btnClear->setEnabled(true);
    wdgInput->setEnabled(false);

    emit update();
}

void GuiFilter::clear(bool enableInput)
{
    btnClear->setEnabled(false);
    wdgInput->setEnabled(enableInput);

    emit update();
}

///////////////////

ComboBoxFilter::ComboBoxFilter(QComboBox *cmbInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot):
    GuiFilter(cmbInput,btnClear,updateObj,updateSlot)
{
    connect(cmbInput,SIGNAL(currentIndexChanged(int)),this,SLOT(set(int)));
}

void ComboBoxFilter::set(int index)
{
    if(!index) return;
    GuiFilter::set();
}

void ComboBoxFilter::clear(bool)
{
    qobject_cast<QComboBox*>(wdgInput)->setCurrentIndex(0);

    GuiFilter::clear(qobject_cast<QComboBox*>(wdgInput)->count()>1);
}

////////////////////

DateTimeFilter::DateTimeFilter(QDateTimeEdit *dtInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot):
    GuiFilter(dtInput,btnClear,updateObj,updateSlot)
{
    connect(dtInput,SIGNAL(editingFinished()),this,SLOT(set()));
}

void DateTimeFilter::set()
{
    QDateTime dt=qobject_cast<QDateTimeEdit*>(wdgInput)->dateTime();
    dt.setTimeSpec(Qt::UTC);

    if(dt==wdgInput->property("default-datetime").toDateTime()) return;

    GuiFilter::set();
}

void DateTimeFilter::clear(bool)
{
    qobject_cast<QDateTimeEdit*>(wdgInput)->setDateTime(wdgInput->property("default-datetime").toDateTime());

    GuiFilter::clear();
}

/////////////////////

PolygonFilter::PolygonFilter(QAbstractButton *btnInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot):
    GuiFilter(btnInput,btnClear,updateObj,updateSlot)
{
    connect(btnInput,SIGNAL(clicked()),this,SIGNAL(choosePolygon()));
}

const QPolygon& PolygonFilter::getPolygon() const
{ return polygon; }

void PolygonFilter::set(const QPolygon& poly)
{
    polygon=poly;
    GuiFilter::set();
}

void PolygonFilter::clear(bool)
{
    polygon.clear();
    GuiFilter::clear();
}
