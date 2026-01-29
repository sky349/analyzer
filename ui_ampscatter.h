/********************************************************************************
** Form generated from reading UI file 'ampscatter.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_AMPSCATTER_H
#define UI_AMPSCATTER_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_ampscatter
{
public:
    QGridLayout *gridLayout;
    QVBoxLayout *verticalLayout_2;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QLabel *label_3;
    QLineEdit *Z1range_edit;
    QLabel *label_4;
    QLineEdit *Z1att_edit;
    QLabel *label_5;
    QLineEdit *Z2range_edit;
    QLabel *label_6;
    QLineEdit *Z2att_edit;
    QFrame *line;
    QLabel *label;
    QLineEdit *FL_edit;
    QLabel *label_2;
    QLineEdit *Padding_edit;
    QFrame *line_2;
    QPushButton *pushButton;

    void setupUi(QWidget *ampscatter)
    {
        if (ampscatter->objectName().isEmpty())
            ampscatter->setObjectName(QString::fromUtf8("ampscatter"));
        ampscatter->resize(1542, 567);
        gridLayout = new QGridLayout(ampscatter);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        verticalLayout_2 = new QVBoxLayout();
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        label_3 = new QLabel(ampscatter);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        horizontalLayout->addWidget(label_3);

        Z1range_edit = new QLineEdit(ampscatter);
        Z1range_edit->setObjectName(QString::fromUtf8("Z1range_edit"));
        Z1range_edit->setMaximumSize(QSize(50, 16777215));

        horizontalLayout->addWidget(Z1range_edit);

        label_4 = new QLabel(ampscatter);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        horizontalLayout->addWidget(label_4);

        Z1att_edit = new QLineEdit(ampscatter);
        Z1att_edit->setObjectName(QString::fromUtf8("Z1att_edit"));
        Z1att_edit->setMaximumSize(QSize(50, 16777215));

        horizontalLayout->addWidget(Z1att_edit);

        label_5 = new QLabel(ampscatter);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        horizontalLayout->addWidget(label_5);

        Z2range_edit = new QLineEdit(ampscatter);
        Z2range_edit->setObjectName(QString::fromUtf8("Z2range_edit"));
        Z2range_edit->setMaximumSize(QSize(50, 16777215));

        horizontalLayout->addWidget(Z2range_edit);

        label_6 = new QLabel(ampscatter);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        horizontalLayout->addWidget(label_6);

        Z2att_edit = new QLineEdit(ampscatter);
        Z2att_edit->setObjectName(QString::fromUtf8("Z2att_edit"));
        Z2att_edit->setMaximumSize(QSize(50, 16777215));

        horizontalLayout->addWidget(Z2att_edit);

        line = new QFrame(ampscatter);
        line->setObjectName(QString::fromUtf8("line"));
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line);

        label = new QLabel(ampscatter);
        label->setObjectName(QString::fromUtf8("label"));

        horizontalLayout->addWidget(label);

        FL_edit = new QLineEdit(ampscatter);
        FL_edit->setObjectName(QString::fromUtf8("FL_edit"));
        QSizePolicy sizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(FL_edit->sizePolicy().hasHeightForWidth());
        FL_edit->setSizePolicy(sizePolicy);
        FL_edit->setMaximumSize(QSize(50, 16777215));

        horizontalLayout->addWidget(FL_edit);

        label_2 = new QLabel(ampscatter);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        horizontalLayout->addWidget(label_2);

        Padding_edit = new QLineEdit(ampscatter);
        Padding_edit->setObjectName(QString::fromUtf8("Padding_edit"));
        sizePolicy.setHeightForWidth(Padding_edit->sizePolicy().hasHeightForWidth());
        Padding_edit->setSizePolicy(sizePolicy);
        Padding_edit->setMaximumSize(QSize(50, 16777215));

        horizontalLayout->addWidget(Padding_edit);

        line_2 = new QFrame(ampscatter);
        line_2->setObjectName(QString::fromUtf8("line_2"));
        line_2->setFrameShape(QFrame::VLine);
        line_2->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_2);

        pushButton = new QPushButton(ampscatter);
        pushButton->setObjectName(QString::fromUtf8("pushButton"));

        horizontalLayout->addWidget(pushButton);


        verticalLayout_2->addLayout(horizontalLayout);


        gridLayout->addLayout(verticalLayout_2, 0, 0, 1, 1);


        retranslateUi(ampscatter);

        QMetaObject::connectSlotsByName(ampscatter);
    } // setupUi

    void retranslateUi(QWidget *ampscatter)
    {
        ampscatter->setWindowTitle(QCoreApplication::translate("ampscatter", "Reply Amplite vs Range", nullptr));
        label_3->setText(QCoreApplication::translate("ampscatter", "Z1 range:", nullptr));
        Z1range_edit->setInputMask(QString());
        Z1range_edit->setText(QString());
        label_4->setText(QCoreApplication::translate("ampscatter", "Z1 att:", nullptr));
        Z1att_edit->setText(QString());
        label_5->setText(QCoreApplication::translate("ampscatter", "Z2 range:", nullptr));
        Z2range_edit->setText(QString());
        label_6->setText(QCoreApplication::translate("ampscatter", "Z2 att:", nullptr));
        Z2att_edit->setText(QString());
        label->setText(QCoreApplication::translate("ampscatter", "FL value:", nullptr));
        FL_edit->setText(QString());
        label_2->setText(QCoreApplication::translate("ampscatter", "Padding:", nullptr));
        Padding_edit->setText(QString());
        pushButton->setText(QCoreApplication::translate("ampscatter", "Generate map", nullptr));
    } // retranslateUi

};

namespace Ui {
    class ampscatter: public Ui_ampscatter {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_AMPSCATTER_H
