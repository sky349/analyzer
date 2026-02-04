#include "plotlabel.h"

#include <QHeaderView>
#include <QPalette>

PlotLabel::PlotLabel(const QTreeWidget *styleSource, QWidget *parent)
    : QTreeWidget(parent)
{
    setColumnCount(2);
    setHeaderHidden(true);
    header()->setFixedHeight(0);
    setRootIsDecorated(false);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setIndentation(0);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setFrameShape(QFrame::Box);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);
    QPalette framePal = palette();
    framePal.setColor(QPalette::WindowText, Qt::black);
    setPalette(framePal);
    setViewportMargins(0, 0, 0, 0);

    QPalette pal = palette();
    pal.setColor(QPalette::Base, QColor(255, 255, 255, 220));
    setPalette(pal);

    applyTreeStyle(styleSource);
}

void PlotLabel::applyTreeStyle(const QTreeWidget *styleSource)
{
    if(!styleSource)
        return;

    setStyle(styleSource->style());
    setFont(styleSource->font());
    setStyleSheet(styleSource->styleSheet());
    setAlternatingRowColors(styleSource->alternatingRowColors());
}

void PlotLabel::setEntries(const QVector<QPair<QString, QString> > &entries)
{
    clear();

    for(int i = 0; i < entries.size(); ++i)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(this);
        item->setText(0, entries[i].first);
        item->setText(1, entries[i].second);
    }

    resizeColumnToContents(0);
    resizeColumnToContents(1);

    int height = 0;
    for(int i = 0; i < topLevelItemCount(); ++i)
        height += sizeHintForRow(i);
    height += frameWidth() * 2;
    setFixedHeight(height);

    int width = columnWidth(0) + columnWidth(1) + frameWidth() * 2;
    setFixedWidth(width);
}
