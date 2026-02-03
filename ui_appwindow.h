/********************************************************************************
** Form generated from reading UI file 'appwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_APPWINDOW_H
#define UI_APPWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QAction>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QDateTimeEdit>
#include <QtWidgets/QDockWidget>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QMenu>
#include <QtWidgets/QMenuBar>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QTreeWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include <radarview/nradarview.h>

QT_BEGIN_NAMESPACE

class Ui_AppWindow
{
public:
    QAction *actExit;
    QAction *actVersion;
    QAction *actImport;
    QAction *actPOD_SSR;
    QAction *actionTasks;
    QAction *actLoadMap;
    QWidget *centralWidget;
    QVBoxLayout *verticalLayout_5;
    NRadarView *radarView;
    QMenuBar *menuBar;
    QMenu *menuActions;
    QMenu *menuAbout;
    QDockWidget *dockFilters;
    QWidget *dockWidgetContents_2;
    QVBoxLayout *verticalLayout_4;
    QHBoxLayout *horizontalLayout;
    QVBoxLayout *verticalLayout;
    QLabel *label_2;
    QLabel *label_3;
    QLabel *label_10;
    QLabel *label_4;
    QLabel *label_7;
    QLabel *label_8;
    QLabel *label_5;
    QLabel *label_6;
    QLabel *label;
    QLabel *label_9;
    QVBoxLayout *verticalLayout_3;
    QComboBox *cmbSource;
    QComboBox *cmbType;
    QComboBox *cmbModeS;
    QToolButton *btnArea;
    QDateTimeEdit *dateBegin;
    QDateTimeEdit *dateEnd;
    QComboBox *cmbModeA;
    QComboBox *cmbAircraftID;
    QComboBox *cmbAddress;
    QComboBox *cmbTrackNo;
    QVBoxLayout *verticalLayout_2;
    QToolButton *btnSourceClear;
    QToolButton *btnTypeClear;
    QToolButton *btnModeSClear;
    QToolButton *btnAreaClear;
    QToolButton *btnBeginClear;
    QToolButton *btnEndClear;
    QToolButton *btnModeAClear;
    QToolButton *btnAircraftIDClear;
    QToolButton *btnAddressClear;
    QToolButton *btnTrackNoClear;
    QDockWidget *dockDetails;
    QWidget *dockWidgetContents_3;
    QVBoxLayout *verticalLayout_6;
    QTreeWidget *treeDetails;
    QCheckBox *blackWhite_chk;
    QDockWidget *dockView;
    QWidget *dockWidgetContents;
    QHBoxLayout *horizontalLayout_2;
    QRadioButton *radioViewNormal;
    QRadioButton *radioViewRA;
    QRadioButton *radioViewRAx10;

    void setupUi(QMainWindow *AppWindow)
    {
        if (AppWindow->objectName().isEmpty())
            AppWindow->setObjectName(QString::fromUtf8("AppWindow"));
        AppWindow->resize(822, 700);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(AppWindow->sizePolicy().hasHeightForWidth());
        AppWindow->setSizePolicy(sizePolicy);
        actExit = new QAction(AppWindow);
        actExit->setObjectName(QString::fromUtf8("actExit"));
        actVersion = new QAction(AppWindow);
        actVersion->setObjectName(QString::fromUtf8("actVersion"));
        actImport = new QAction(AppWindow);
        actImport->setObjectName(QString::fromUtf8("actImport"));
        actPOD_SSR = new QAction(AppWindow);
        actPOD_SSR->setObjectName(QString::fromUtf8("actPOD_SSR"));
        actionTasks = new QAction(AppWindow);
        actionTasks->setObjectName(QString::fromUtf8("actionTasks"));
        actLoadMap = new QAction(AppWindow);
        actLoadMap->setObjectName(QString::fromUtf8("actLoadMap"));
        centralWidget = new QWidget(AppWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        verticalLayout_5 = new QVBoxLayout(centralWidget);
        verticalLayout_5->setSpacing(6);
        verticalLayout_5->setContentsMargins(11, 11, 11, 11);
        verticalLayout_5->setObjectName(QString::fromUtf8("verticalLayout_5"));
        verticalLayout_5->setContentsMargins(0, 0, 0, 0);
        radarView = new NRadarView(centralWidget);
        radarView->setObjectName(QString::fromUtf8("radarView"));

        verticalLayout_5->addWidget(radarView);

        AppWindow->setCentralWidget(centralWidget);
        menuBar = new QMenuBar(AppWindow);
        menuBar->setObjectName(QString::fromUtf8("menuBar"));
        menuBar->setGeometry(QRect(0, 0, 822, 17));
        menuActions = new QMenu(menuBar);
        menuActions->setObjectName(QString::fromUtf8("menuActions"));
        menuAbout = new QMenu(menuBar);
        menuAbout->setObjectName(QString::fromUtf8("menuAbout"));
        AppWindow->setMenuBar(menuBar);
        dockFilters = new QDockWidget(AppWindow);
        dockFilters->setObjectName(QString::fromUtf8("dockFilters"));
        dockFilters->setFeatures(QDockWidget::DockWidgetMovable);
        dockFilters->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
        dockWidgetContents_2 = new QWidget();
        dockWidgetContents_2->setObjectName(QString::fromUtf8("dockWidgetContents_2"));
        QSizePolicy sizePolicy1(QSizePolicy::Preferred, QSizePolicy::Maximum);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(dockWidgetContents_2->sizePolicy().hasHeightForWidth());
        dockWidgetContents_2->setSizePolicy(sizePolicy1);
        verticalLayout_4 = new QVBoxLayout(dockWidgetContents_2);
        verticalLayout_4->setSpacing(6);
        verticalLayout_4->setContentsMargins(11, 11, 11, 11);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(0, 0, 0, 0);
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(6);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        verticalLayout = new QVBoxLayout();
        verticalLayout->setSpacing(6);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        label_2 = new QLabel(dockWidgetContents_2);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label_2);

        label_3 = new QLabel(dockWidgetContents_2);
        label_3->setObjectName(QString::fromUtf8("label_3"));
        label_3->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label_3);

        label_10 = new QLabel(dockWidgetContents_2);
        label_10->setObjectName(QString::fromUtf8("label_10"));

        verticalLayout->addWidget(label_10);

        label_4 = new QLabel(dockWidgetContents_2);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label_4);

        label_7 = new QLabel(dockWidgetContents_2);
        label_7->setObjectName(QString::fromUtf8("label_7"));
        label_7->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label_7);

        label_8 = new QLabel(dockWidgetContents_2);
        label_8->setObjectName(QString::fromUtf8("label_8"));
        label_8->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label_8);

        label_5 = new QLabel(dockWidgetContents_2);
        label_5->setObjectName(QString::fromUtf8("label_5"));
        label_5->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label_5);

        label_6 = new QLabel(dockWidgetContents_2);
        label_6->setObjectName(QString::fromUtf8("label_6"));
        label_6->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label_6);

        label = new QLabel(dockWidgetContents_2);
        label->setObjectName(QString::fromUtf8("label"));
        label->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        verticalLayout->addWidget(label);

        label_9 = new QLabel(dockWidgetContents_2);
        label_9->setObjectName(QString::fromUtf8("label_9"));

        verticalLayout->addWidget(label_9);


        horizontalLayout->addLayout(verticalLayout);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setSpacing(6);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        cmbSource = new QComboBox(dockWidgetContents_2);
        cmbSource->addItem(QString());
        cmbSource->addItem(QString());
        cmbSource->addItem(QString());
        cmbSource->addItem(QString());
        cmbSource->setObjectName(QString::fromUtf8("cmbSource"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(cmbSource->sizePolicy().hasHeightForWidth());
        cmbSource->setSizePolicy(sizePolicy2);

        verticalLayout_3->addWidget(cmbSource);

        cmbType = new QComboBox(dockWidgetContents_2);
        cmbType->addItem(QString());
        cmbType->addItem(QString());
        cmbType->addItem(QString());
        cmbType->setObjectName(QString::fromUtf8("cmbType"));
        sizePolicy2.setHeightForWidth(cmbType->sizePolicy().hasHeightForWidth());
        cmbType->setSizePolicy(sizePolicy2);

        verticalLayout_3->addWidget(cmbType);

        cmbModeS = new QComboBox(dockWidgetContents_2);
        cmbModeS->addItem(QString());
        cmbModeS->addItem(QString());
        cmbModeS->addItem(QString());
        cmbModeS->setObjectName(QString::fromUtf8("cmbModeS"));

        verticalLayout_3->addWidget(cmbModeS);

        btnArea = new QToolButton(dockWidgetContents_2);
        btnArea->setObjectName(QString::fromUtf8("btnArea"));
        sizePolicy2.setHeightForWidth(btnArea->sizePolicy().hasHeightForWidth());
        btnArea->setSizePolicy(sizePolicy2);

        verticalLayout_3->addWidget(btnArea);

        dateBegin = new QDateTimeEdit(dockWidgetContents_2);
        dateBegin->setObjectName(QString::fromUtf8("dateBegin"));
        sizePolicy2.setHeightForWidth(dateBegin->sizePolicy().hasHeightForWidth());
        dateBegin->setSizePolicy(sizePolicy2);
        dateBegin->setCalendarPopup(true);
        dateBegin->setTimeSpec(Qt::UTC);

        verticalLayout_3->addWidget(dateBegin);

        dateEnd = new QDateTimeEdit(dockWidgetContents_2);
        dateEnd->setObjectName(QString::fromUtf8("dateEnd"));
        sizePolicy2.setHeightForWidth(dateEnd->sizePolicy().hasHeightForWidth());
        dateEnd->setSizePolicy(sizePolicy2);
        dateEnd->setCalendarPopup(true);
        dateEnd->setTimeSpec(Qt::UTC);

        verticalLayout_3->addWidget(dateEnd);

        cmbModeA = new QComboBox(dockWidgetContents_2);
        cmbModeA->setObjectName(QString::fromUtf8("cmbModeA"));
        sizePolicy2.setHeightForWidth(cmbModeA->sizePolicy().hasHeightForWidth());
        cmbModeA->setSizePolicy(sizePolicy2);

        verticalLayout_3->addWidget(cmbModeA);

        cmbAircraftID = new QComboBox(dockWidgetContents_2);
        cmbAircraftID->setObjectName(QString::fromUtf8("cmbAircraftID"));
        sizePolicy2.setHeightForWidth(cmbAircraftID->sizePolicy().hasHeightForWidth());
        cmbAircraftID->setSizePolicy(sizePolicy2);

        verticalLayout_3->addWidget(cmbAircraftID);

        cmbAddress = new QComboBox(dockWidgetContents_2);
        cmbAddress->setObjectName(QString::fromUtf8("cmbAddress"));
        sizePolicy2.setHeightForWidth(cmbAddress->sizePolicy().hasHeightForWidth());
        cmbAddress->setSizePolicy(sizePolicy2);

        verticalLayout_3->addWidget(cmbAddress);

        cmbTrackNo = new QComboBox(dockWidgetContents_2);
        cmbTrackNo->setObjectName(QString::fromUtf8("cmbTrackNo"));

        verticalLayout_3->addWidget(cmbTrackNo);


        horizontalLayout->addLayout(verticalLayout_3);

        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        btnSourceClear = new QToolButton(dockWidgetContents_2);
        btnSourceClear->setObjectName(QString::fromUtf8("btnSourceClear"));
        btnSourceClear->setEnabled(false);

        verticalLayout_2->addWidget(btnSourceClear);

        btnTypeClear = new QToolButton(dockWidgetContents_2);
        btnTypeClear->setObjectName(QString::fromUtf8("btnTypeClear"));
        btnTypeClear->setEnabled(false);

        verticalLayout_2->addWidget(btnTypeClear);

        btnModeSClear = new QToolButton(dockWidgetContents_2);
        btnModeSClear->setObjectName(QString::fromUtf8("btnModeSClear"));
        btnModeSClear->setEnabled(false);

        verticalLayout_2->addWidget(btnModeSClear);

        btnAreaClear = new QToolButton(dockWidgetContents_2);
        btnAreaClear->setObjectName(QString::fromUtf8("btnAreaClear"));
        btnAreaClear->setEnabled(false);

        verticalLayout_2->addWidget(btnAreaClear);

        btnBeginClear = new QToolButton(dockWidgetContents_2);
        btnBeginClear->setObjectName(QString::fromUtf8("btnBeginClear"));
        btnBeginClear->setEnabled(false);

        verticalLayout_2->addWidget(btnBeginClear);

        btnEndClear = new QToolButton(dockWidgetContents_2);
        btnEndClear->setObjectName(QString::fromUtf8("btnEndClear"));
        btnEndClear->setEnabled(false);

        verticalLayout_2->addWidget(btnEndClear);

        btnModeAClear = new QToolButton(dockWidgetContents_2);
        btnModeAClear->setObjectName(QString::fromUtf8("btnModeAClear"));
        btnModeAClear->setEnabled(false);

        verticalLayout_2->addWidget(btnModeAClear);

        btnAircraftIDClear = new QToolButton(dockWidgetContents_2);
        btnAircraftIDClear->setObjectName(QString::fromUtf8("btnAircraftIDClear"));
        btnAircraftIDClear->setEnabled(false);

        verticalLayout_2->addWidget(btnAircraftIDClear);

        btnAddressClear = new QToolButton(dockWidgetContents_2);
        btnAddressClear->setObjectName(QString::fromUtf8("btnAddressClear"));
        btnAddressClear->setEnabled(false);

        verticalLayout_2->addWidget(btnAddressClear);

        btnTrackNoClear = new QToolButton(dockWidgetContents_2);
        btnTrackNoClear->setObjectName(QString::fromUtf8("btnTrackNoClear"));
        btnTrackNoClear->setEnabled(false);

        verticalLayout_2->addWidget(btnTrackNoClear);


        horizontalLayout->addLayout(verticalLayout_2);


        verticalLayout_4->addLayout(horizontalLayout);

        dockFilters->setWidget(dockWidgetContents_2);
        AppWindow->addDockWidget(Qt::RightDockWidgetArea, dockFilters);
        dockDetails = new QDockWidget(AppWindow);
        dockDetails->setObjectName(QString::fromUtf8("dockDetails"));
        sizePolicy.setHeightForWidth(dockDetails->sizePolicy().hasHeightForWidth());
        dockDetails->setSizePolicy(sizePolicy);
        dockDetails->setFeatures(QDockWidget::DockWidgetMovable);
        dockDetails->setAllowedAreas(Qt::LeftDockWidgetArea|Qt::RightDockWidgetArea);
        dockWidgetContents_3 = new QWidget();
        dockWidgetContents_3->setObjectName(QString::fromUtf8("dockWidgetContents_3"));
        QSizePolicy sizePolicy3(QSizePolicy::Preferred, QSizePolicy::Expanding);
        sizePolicy3.setHorizontalStretch(0);
        sizePolicy3.setVerticalStretch(0);
        sizePolicy3.setHeightForWidth(dockWidgetContents_3->sizePolicy().hasHeightForWidth());
        dockWidgetContents_3->setSizePolicy(sizePolicy3);
        verticalLayout_6 = new QVBoxLayout(dockWidgetContents_3);
        verticalLayout_6->setSpacing(6);
        verticalLayout_6->setContentsMargins(11, 11, 11, 11);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        verticalLayout_6->setContentsMargins(0, 0, 0, 0);
        treeDetails = new QTreeWidget(dockWidgetContents_3);
        QTreeWidgetItem *__qtreewidgetitem = new QTreeWidgetItem();
        __qtreewidgetitem->setText(0, QString::fromUtf8("1"));
        treeDetails->setHeaderItem(__qtreewidgetitem);
        treeDetails->setObjectName(QString::fromUtf8("treeDetails"));
        treeDetails->setAlternatingRowColors(true);
        treeDetails->setAllColumnsShowFocus(true);
        treeDetails->header()->setVisible(false);

        verticalLayout_6->addWidget(treeDetails);

        blackWhite_chk = new QCheckBox(dockWidgetContents_3);
        blackWhite_chk->setObjectName(QString::fromUtf8("blackWhite_chk"));

        verticalLayout_6->addWidget(blackWhite_chk);

        dockDetails->setWidget(dockWidgetContents_3);
        AppWindow->addDockWidget(Qt::RightDockWidgetArea, dockDetails);
        dockView = new QDockWidget(AppWindow);
        dockView->setObjectName(QString::fromUtf8("dockView"));
        dockView->setFeatures(QDockWidget::DockWidgetMovable);
        dockWidgetContents = new QWidget();
        dockWidgetContents->setObjectName(QString::fromUtf8("dockWidgetContents"));
        sizePolicy1.setHeightForWidth(dockWidgetContents->sizePolicy().hasHeightForWidth());
        dockWidgetContents->setSizePolicy(sizePolicy1);
        horizontalLayout_2 = new QHBoxLayout(dockWidgetContents);
        horizontalLayout_2->setSpacing(6);
        horizontalLayout_2->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(6, 0, 6, 0);
        radioViewNormal = new QRadioButton(dockWidgetContents);
        radioViewNormal->setObjectName(QString::fromUtf8("radioViewNormal"));
        radioViewNormal->setChecked(true);

        horizontalLayout_2->addWidget(radioViewNormal);

        radioViewRA = new QRadioButton(dockWidgetContents);
        radioViewRA->setObjectName(QString::fromUtf8("radioViewRA"));

        horizontalLayout_2->addWidget(radioViewRA);

        radioViewRAx10 = new QRadioButton(dockWidgetContents);
        radioViewRAx10->setObjectName(QString::fromUtf8("radioViewRAx10"));

        horizontalLayout_2->addWidget(radioViewRAx10);

        dockView->setWidget(dockWidgetContents);
        AppWindow->addDockWidget(Qt::RightDockWidgetArea, dockView);

        menuBar->addAction(menuActions->menuAction());
        menuBar->addAction(menuAbout->menuAction());
        menuActions->addAction(actImport);
        menuActions->addSeparator();
        menuActions->addAction(actionTasks);
        menuActions->addAction(actLoadMap);
        menuActions->addSeparator();
        menuActions->addAction(actExit);
        menuAbout->addAction(actVersion);

        retranslateUi(AppWindow);

        QMetaObject::connectSlotsByName(AppWindow);
    } // setupUi

    void retranslateUi(QMainWindow *AppWindow)
    {
        AppWindow->setWindowTitle(QCoreApplication::translate("AppWindow", "Easat Analyser", nullptr));
        actExit->setText(QCoreApplication::translate("AppWindow", "Exit", nullptr));
        actVersion->setText(QCoreApplication::translate("AppWindow", "Version", nullptr));
        actImport->setText(QCoreApplication::translate("AppWindow", "New import", nullptr));
        actPOD_SSR->setText(QCoreApplication::translate("AppWindow", "Get SSR PoD...", nullptr));
        actionTasks->setText(QCoreApplication::translate("AppWindow", "Tasks", nullptr));
        actLoadMap->setText(QCoreApplication::translate("AppWindow", "Load map file...", nullptr));
        menuActions->setTitle(QCoreApplication::translate("AppWindow", "Actions", nullptr));
        menuAbout->setTitle(QCoreApplication::translate("AppWindow", "About", nullptr));
        dockFilters->setWindowTitle(QCoreApplication::translate("AppWindow", "Data filters", nullptr));
        label_2->setText(QCoreApplication::translate("AppWindow", "Source:", nullptr));
        label_3->setText(QCoreApplication::translate("AppWindow", "Type:", nullptr));
        label_10->setText(QCoreApplication::translate("AppWindow", "Mode-S:", nullptr));
        label_4->setText(QCoreApplication::translate("AppWindow", "Area:", nullptr));
        label_7->setText(QCoreApplication::translate("AppWindow", "Begin", nullptr));
        label_8->setText(QCoreApplication::translate("AppWindow", "End", nullptr));
        label_5->setText(QCoreApplication::translate("AppWindow", "Mode-A:", nullptr));
        label_6->setText(QCoreApplication::translate("AppWindow", "Aircr. ID:", nullptr));
        label->setText(QCoreApplication::translate("AppWindow", "Address:", nullptr));
        label_9->setText(QCoreApplication::translate("AppWindow", "Track #", nullptr));
        cmbSource->setItemText(0, QCoreApplication::translate("AppWindow", "Any", nullptr));
        cmbSource->setItemText(1, QCoreApplication::translate("AppWindow", "PSR", nullptr));
        cmbSource->setItemText(2, QCoreApplication::translate("AppWindow", "SSR", nullptr));
        cmbSource->setItemText(3, QCoreApplication::translate("AppWindow", "ADS-B", nullptr));

        cmbType->setItemText(0, QCoreApplication::translate("AppWindow", "Any", nullptr));
        cmbType->setItemText(1, QCoreApplication::translate("AppWindow", "Plots", nullptr));
        cmbType->setItemText(2, QCoreApplication::translate("AppWindow", "Tracks", nullptr));

        cmbModeS->setItemText(0, QCoreApplication::translate("AppWindow", "Any", nullptr));
        cmbModeS->setItemText(1, QCoreApplication::translate("AppWindow", "No", nullptr));
        cmbModeS->setItemText(2, QCoreApplication::translate("AppWindow", "Yes", nullptr));

        btnArea->setText(QCoreApplication::translate("AppWindow", "click to define...", nullptr));
        dateBegin->setDisplayFormat(QCoreApplication::translate("AppWindow", "d/M/yy HH:mm:ss", nullptr));
        dateEnd->setDisplayFormat(QCoreApplication::translate("AppWindow", "d/M/yy HH:mm:ss", nullptr));
        btnSourceClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnTypeClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnModeSClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnAreaClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnBeginClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnEndClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnModeAClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnAircraftIDClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnAddressClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        btnTrackNoClear->setText(QCoreApplication::translate("AppWindow", "X", nullptr));
        dockDetails->setWindowTitle(QCoreApplication::translate("AppWindow", "Selection details", nullptr));
        blackWhite_chk->setText(QCoreApplication::translate("AppWindow", "Black and white mode", nullptr));
        dockView->setWindowTitle(QCoreApplication::translate("AppWindow", "View mode", nullptr));
        radioViewNormal->setText(QCoreApplication::translate("AppWindow", "Geo", nullptr));
        radioViewRA->setText(QCoreApplication::translate("AppWindow", "Range / Alt", nullptr));
        radioViewRAx10->setText(QCoreApplication::translate("AppWindow", "Range / Alt x10", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AppWindow: public Ui_AppWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_APPWINDOW_H
