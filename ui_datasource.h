/********************************************************************************
** Form generated from reading UI file 'datasource.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_DATASOURCE_H
#define UI_DATASOURCE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDateTimeEdit>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_DataSource
{
public:
    QVBoxLayout *verticalLayout_2;
    QLabel *label_4;
    QFrame *line;
    QRadioButton *radioFolder;
    QWidget *wdgFolder;
    QVBoxLayout *verticalLayout;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLineEdit *edFolder;
    QToolButton *btnBrowseFolder;
    QHBoxLayout *horizontalLayout_3;
    QLabel *label_2;
    QDateTimeEdit *dateFrom;
    QLabel *label_3;
    QDateTimeEdit *dateTo;
    QRadioButton *radioFile;
    QWidget *wdgFile;
    QHBoxLayout *horizontalLayout_4;
    QLineEdit *edFile;
    QToolButton *btnBrowseFile;
    QFrame *line_2;
    QHBoxLayout *horizontalLayout_6;
    QLabel *label_6;
    QLineEdit *edCenter;
    QSpacerItem *verticalSpacer;
    QStackedWidget *stackActions;
    QWidget *page;
    QHBoxLayout *horizontalLayout_2;
    QSpacerItem *horizontalSpacer;
    QPushButton *btnImport;
    QPushButton *btnClose;
    QWidget *page_2;
    QHBoxLayout *horizontalLayout_5;
    QProgressBar *progressImport;
    QPushButton *btnAbort;

    void setupUi(QDialog *DataSource)
    {
        if (DataSource->objectName().isEmpty())
            DataSource->setObjectName(QString::fromUtf8("DataSource"));
        DataSource->resize(365, 254);
        verticalLayout_2 = new QVBoxLayout(DataSource);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        label_4 = new QLabel(DataSource);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        QFont font;
        font.setPointSize(11);
        label_4->setFont(font);

        verticalLayout_2->addWidget(label_4);

        line = new QFrame(DataSource);
        line->setObjectName(QString::fromUtf8("line"));
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Sunken);

        verticalLayout_2->addWidget(line);

        radioFolder = new QRadioButton(DataSource);
        radioFolder->setObjectName(QString::fromUtf8("radioFolder"));
        radioFolder->setChecked(true);

        verticalLayout_2->addWidget(radioFolder);

        wdgFolder = new QWidget(DataSource);
        wdgFolder->setObjectName(QString::fromUtf8("wdgFolder"));
        verticalLayout = new QVBoxLayout(wdgFolder);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(20, 0, 0, 0);
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label = new QLabel(wdgFolder);
        label->setObjectName(QString::fromUtf8("label"));

        horizontalLayout->addWidget(label);

        edFolder = new QLineEdit(wdgFolder);
        edFolder->setObjectName(QString::fromUtf8("edFolder"));
        edFolder->setReadOnly(true);

        horizontalLayout->addWidget(edFolder);

        btnBrowseFolder = new QToolButton(wdgFolder);
        btnBrowseFolder->setObjectName(QString::fromUtf8("btnBrowseFolder"));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(btnBrowseFolder->sizePolicy().hasHeightForWidth());
        btnBrowseFolder->setSizePolicy(sizePolicy);

        horizontalLayout->addWidget(btnBrowseFolder);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        label_2 = new QLabel(wdgFolder);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Preferred);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(label_2->sizePolicy().hasHeightForWidth());
        label_2->setSizePolicy(sizePolicy1);

        horizontalLayout_3->addWidget(label_2);

        dateFrom = new QDateTimeEdit(wdgFolder);
        dateFrom->setObjectName(QString::fromUtf8("dateFrom"));
        dateFrom->setEnabled(false);
        dateFrom->setCalendarPopup(true);
        dateFrom->setTimeSpec(Qt::UTC);

        horizontalLayout_3->addWidget(dateFrom);

        label_3 = new QLabel(wdgFolder);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        sizePolicy1.setHeightForWidth(label_3->sizePolicy().hasHeightForWidth());
        label_3->setSizePolicy(sizePolicy1);

        horizontalLayout_3->addWidget(label_3);

        dateTo = new QDateTimeEdit(wdgFolder);
        dateTo->setObjectName(QString::fromUtf8("dateTo"));
        dateTo->setEnabled(false);
        dateTo->setCalendarPopup(true);
        dateTo->setTimeSpec(Qt::UTC);

        horizontalLayout_3->addWidget(dateTo);


        verticalLayout->addLayout(horizontalLayout_3);


        verticalLayout_2->addWidget(wdgFolder);

        radioFile = new QRadioButton(DataSource);
        radioFile->setObjectName(QString::fromUtf8("radioFile"));
        radioFile->setEnabled(true);

        verticalLayout_2->addWidget(radioFile);

        wdgFile = new QWidget(DataSource);
        wdgFile->setObjectName(QString::fromUtf8("wdgFile"));
        wdgFile->setEnabled(true);
        horizontalLayout_4 = new QHBoxLayout(wdgFile);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(20, 0, 0, 0);
        edFile = new QLineEdit(wdgFile);
        edFile->setObjectName(QString::fromUtf8("edFile"));
        edFile->setEnabled(true);
        edFile->setReadOnly(true);

        horizontalLayout_4->addWidget(edFile);

        btnBrowseFile = new QToolButton(wdgFile);
        btnBrowseFile->setObjectName(QString::fromUtf8("btnBrowseFile"));
        btnBrowseFile->setEnabled(true);
        sizePolicy.setHeightForWidth(btnBrowseFile->sizePolicy().hasHeightForWidth());
        btnBrowseFile->setSizePolicy(sizePolicy);

        horizontalLayout_4->addWidget(btnBrowseFile);


        verticalLayout_2->addWidget(wdgFile);

        line_2 = new QFrame(DataSource);
        line_2->setObjectName(QString::fromUtf8("line_2"));
        line_2->setFrameShape(QFrame::HLine);
        line_2->setFrameShadow(QFrame::Sunken);

        verticalLayout_2->addWidget(line_2);

        horizontalLayout_6 = new QHBoxLayout();
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        label_6 = new QLabel(DataSource);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        horizontalLayout_6->addWidget(label_6);

        edCenter = new QLineEdit(DataSource);
        edCenter->setObjectName(QString::fromUtf8("edCenter"));

        horizontalLayout_6->addWidget(edCenter);


        verticalLayout_2->addLayout(horizontalLayout_6);

        verticalSpacer = new QSpacerItem(20, 14, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);

        stackActions = new QStackedWidget(DataSource);
        stackActions->setObjectName(QString::fromUtf8("stackActions"));
        page = new QWidget();
        page->setObjectName(QString::fromUtf8("page"));
        horizontalLayout_2 = new QHBoxLayout(page);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        btnImport = new QPushButton(page);
        btnImport->setObjectName(QString::fromUtf8("btnImport"));

        horizontalLayout_2->addWidget(btnImport);

        btnClose = new QPushButton(page);
        btnClose->setObjectName(QString::fromUtf8("btnClose"));

        horizontalLayout_2->addWidget(btnClose);

        stackActions->addWidget(page);
        page_2 = new QWidget();
        page_2->setObjectName(QString::fromUtf8("page_2"));
        horizontalLayout_5 = new QHBoxLayout(page_2);
        horizontalLayout_5->setObjectName(QString::fromUtf8("horizontalLayout_5"));
        horizontalLayout_5->setContentsMargins(0, 0, 0, 0);
        progressImport = new QProgressBar(page_2);
        progressImport->setObjectName(QString::fromUtf8("progressImport"));
        progressImport->setValue(0);

        horizontalLayout_5->addWidget(progressImport);

        btnAbort = new QPushButton(page_2);
        btnAbort->setObjectName(QString::fromUtf8("btnAbort"));
        QPalette palette;
        QBrush brush(QColor(255, 192, 192, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Button, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Button, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush);
        btnAbort->setPalette(palette);

        horizontalLayout_5->addWidget(btnAbort);

        stackActions->addWidget(page_2);

        verticalLayout_2->addWidget(stackActions);

        radioFolder->raise();
        radioFile->raise();
        label_4->raise();
        line->raise();
        wdgFolder->raise();
        wdgFile->raise();
        stackActions->raise();
        line_2->raise();

        retranslateUi(DataSource);
        QObject::connect(radioFolder, SIGNAL(toggled(bool)), wdgFolder, SLOT(setEnabled(bool)));
        QObject::connect(radioFile, SIGNAL(toggled(bool)), wdgFile, SLOT(setEnabled(bool)));
        QObject::connect(btnClose, SIGNAL(clicked()), DataSource, SLOT(reject()));

        stackActions->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(DataSource);
    } // setupUi

    void retranslateUi(QDialog *DataSource)
    {
        DataSource->setWindowTitle(QCoreApplication::translate("DataSource", "Analyser - Data import", nullptr));
        label_4->setText(QCoreApplication::translate("DataSource", "Analyser data import", nullptr));
        radioFolder->setText(QCoreApplication::translate("DataSource", "Choose a RDB folder:", nullptr));
        label->setText(QCoreApplication::translate("DataSource", "RDB files folder:", nullptr));
        edFolder->setPlaceholderText(QCoreApplication::translate("DataSource", "select a folder...", nullptr));
        btnBrowseFolder->setText(QCoreApplication::translate("DataSource", "...", nullptr));
        label_2->setText(QCoreApplication::translate("DataSource", "From", nullptr));
        dateFrom->setDisplayFormat(QCoreApplication::translate("DataSource", "d/M/yy HH:mm", nullptr));
        label_3->setText(QCoreApplication::translate("DataSource", "to", nullptr));
        dateTo->setDisplayFormat(QCoreApplication::translate("DataSource", "d/M/yy HH:mm", nullptr));
        radioFile->setText(QCoreApplication::translate("DataSource", "Choose a separate RDB or ASTERIX binary file:", nullptr));
        edFile->setPlaceholderText(QCoreApplication::translate("DataSource", "select a file...", nullptr));
        btnBrowseFile->setText(QCoreApplication::translate("DataSource", "...", nullptr));
        label_6->setText(QCoreApplication::translate("DataSource", "Radar position:", nullptr));
        edCenter->setInputMask(QCoreApplication::translate("DataSource", "00\\d00'00.0A 000\\d00'00.0A", nullptr));
        edCenter->setText(QCoreApplication::translate("DataSource", "60d10'15.0N 024d56'15.0E", nullptr));
        btnImport->setText(QCoreApplication::translate("DataSource", "Import!", nullptr));
        btnClose->setText(QCoreApplication::translate("DataSource", "Close and exit", nullptr));
        btnAbort->setText(QCoreApplication::translate("DataSource", "Stop now", nullptr));
    } // retranslateUi

};

namespace Ui {
    class DataSource: public Ui_DataSource {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_DATASOURCE_H
