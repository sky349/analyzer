#ifndef GUIFILTER_H
#define GUIFILTER_H

#include <QtWidgets>

class GuiFilter:public QObject
{
Q_OBJECT

public:
    GuiFilter(QWidget *wdgInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot);

public slots:
    virtual void set();
    virtual void clear(bool enableInput=true);

signals:
    void update();

protected:
    QWidget *wdgInput;
    QAbstractButton *btnClear;
};

class ComboBoxFilter:public GuiFilter
{
Q_OBJECT

public:
    ComboBoxFilter(QComboBox *cmbInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot);

public slots:
    void set(int index);
    void clear(bool enableInput);
};

class DateTimeFilter:public GuiFilter
{
Q_OBJECT

public:
    DateTimeFilter(QDateTimeEdit *cmbInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot);

public slots:
    void set();
    void clear(bool enableInput);
};

class PolygonFilter:public GuiFilter
{
Q_OBJECT

public:
    PolygonFilter(QAbstractButton *btnInput,QAbstractButton *btnClear,QObject *updateObj,const char* updateSlot);

    const QPolygon& getPolygon() const;

public slots:
    void set(const QPolygon& poly);
    void clear(bool);

signals:
    void choosePolygon();

protected:
    QPolygon polygon;
};



#endif // GUIFILTER_H
