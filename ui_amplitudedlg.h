/********************************************************************************
** Form generated from reading UI file 'amplitudedlg.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AMPLITUDEDLG_H
#define UI_AMPLITUDEDLG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QVBoxLayout>

QT_BEGIN_NAMESPACE

class Ui_AmplitudeDlg
{
public:
    QVBoxLayout *verticalLayout;
    QLabel *label_4;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QSpinBox *spinIncrease;
    QLabel *label_3;
    QSpacerItem *horizontalSpacer;
    QHBoxLayout *horizontalLayout_2;
    QLabel *label_2;
    QSpinBox *spinLevel;
    QSpacerItem *horizontalSpacer_3;
    QHBoxLayout *horizontalLayout_3;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *btnOk;
    QSpacerItem *verticalSpacer;

    void setupUi(QDialog *AmplitudeDlg)
    {
        if (AmplitudeDlg->objectName().isEmpty())
            AmplitudeDlg->setObjectName(QString::fromUtf8("AmplitudeDlg"));
        AmplitudeDlg->resize(361, 118);
        verticalLayout = new QVBoxLayout(AmplitudeDlg);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(6, 6, 6, 6);
        label_4 = new QLabel(AmplitudeDlg);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        QFont font;
        font.setBold(true);
        font.setWeight(75);
        label_4->setFont(font);

        verticalLayout->addWidget(label_4);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label = new QLabel(AmplitudeDlg);
        label->setObjectName(QString::fromUtf8("label"));

        horizontalLayout->addWidget(label);

        spinIncrease = new QSpinBox(AmplitudeDlg);
        spinIncrease->setObjectName(QString::fromUtf8("spinIncrease"));
        spinIncrease->setMinimumSize(QSize(50, 0));
        spinIncrease->setMaximum(100);

        horizontalLayout->addWidget(spinIncrease);

        label_3 = new QLabel(AmplitudeDlg);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        horizontalLayout->addWidget(label_3);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout->addLayout(horizontalLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        label_2 = new QLabel(AmplitudeDlg);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        horizontalLayout_2->addWidget(label_2);

        spinLevel = new QSpinBox(AmplitudeDlg);
        spinLevel->setObjectName(QString::fromUtf8("spinLevel"));
        spinLevel->setMinimumSize(QSize(50, 0));
        spinLevel->setMaximum(5000);

        horizontalLayout_2->addWidget(spinLevel);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_3);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalSpacer_2 = new QSpacerItem(158, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_2);

        btnOk = new QPushButton(AmplitudeDlg);
        btnOk->setObjectName(QString::fromUtf8("btnOk"));

        horizontalLayout_3->addWidget(btnOk);


        verticalLayout->addLayout(horizontalLayout_3);

        verticalSpacer = new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(AmplitudeDlg);
        QObject::connect(btnOk, SIGNAL(clicked()), AmplitudeDlg, SLOT(accept()));

        QMetaObject::connectSlotsByName(AmplitudeDlg);
    } // setupUi

    void retranslateUi(QDialog *AmplitudeDlg)
    {
        AmplitudeDlg->setWindowTitle(QCoreApplication::translate("AmplitudeDlg", "SSR amplitudes options", nullptr));
        label_4->setText(QCoreApplication::translate("AmplitudeDlg", "Options:", nullptr));
        label->setText(QCoreApplication::translate("AmplitudeDlg", "- Increase amplitude value by", nullptr));
        spinIncrease->setSuffix(QCoreApplication::translate("AmplitudeDlg", "%", nullptr));
        label_3->setText(QCoreApplication::translate("AmplitudeDlg", "per 100 km", nullptr));
        label_2->setText(QCoreApplication::translate("AmplitudeDlg", "- Hide plots with amp below", nullptr));
        btnOk->setText(QCoreApplication::translate("AmplitudeDlg", "Start!", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AmplitudeDlg: public Ui_AmplitudeDlg {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AMPLITUDEDLG_H
