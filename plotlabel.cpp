#include "plotlabel.h"

#include <QHeaderView>
#include <QPalette>

PlotLabel::PlotLabel(const QTreeWidget *styleSource, QWidget *parent)
    : QTreeWidget(parent)
{
    setColumnCount(1);
    setHeaderHidden(true);
    setRootIsDecorated(false);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setContextMenuPolicy(Qt::NoContextMenu);
    setIndentation(0);
    setSizeAdjustPolicy(QAbstractScrollArea::AdjustToContents);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    header()->setStretchLastSection(false);
    header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    setFrameShape(QFrame::Box);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);
    setViewportMargins(0, 0, 0, 0);

    QPalette pal = palette();
    pal.setColor(QPalette::WindowText, Qt::black);
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

void PlotLabel::setEntries(const QVector<QString> &entries)
{
    clear();

    for(const auto &entry : entries)
    {
        QTreeWidgetItem *item = new QTreeWidgetItem(this);
        item->setText(0, entry);
    }

    doItemsLayout();

    int height = frameWidth() * 2;
    for(int i = 0; i < topLevelItemCount(); ++i)
        height += sizeHintForRow(i);
    setFixedHeight(height);

    int width = sizeHintForColumn(0) + frameWidth() * 2;
    setFixedWidth(width);
}
