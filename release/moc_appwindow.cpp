/****************************************************************************
** Meta object code from reading C++ file 'appwindow.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../appwindow.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'appwindow.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_AppWindow_t {
    QByteArrayData data[23];
    char stringdata0[235];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_AppWindow_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_AppWindow_t qt_meta_stringdata_AppWindow = {
    {
QT_MOC_LITERAL(0, 0, 9), // "AppWindow"
QT_MOC_LITERAL(1, 10, 7), // "loadMap"
QT_MOC_LITERAL(2, 18, 0), // ""
QT_MOC_LITERAL(3, 19, 10), // "importData"
QT_MOC_LITERAL(4, 30, 7), // "confirm"
QT_MOC_LITERAL(5, 38, 11), // "initFilters"
QT_MOC_LITERAL(6, 50, 15), // "const DataPack*"
QT_MOC_LITERAL(7, 66, 4), // "data"
QT_MOC_LITERAL(8, 71, 11), // "applyFilter"
QT_MOC_LITERAL(9, 83, 10), // "fullFilter"
QT_MOC_LITERAL(10, 94, 19), // "applyFilterToLayers"
QT_MOC_LITERAL(11, 114, 14), // "updateViewMode"
QT_MOC_LITERAL(12, 129, 1), // "m"
QT_MOC_LITERAL(13, 131, 16), // "addItemToDetails"
QT_MOC_LITERAL(14, 148, 16), // "QTreeWidgetItem*"
QT_MOC_LITERAL(15, 165, 6), // "parent"
QT_MOC_LITERAL(16, 172, 4), // "name"
QT_MOC_LITERAL(17, 177, 5), // "value"
QT_MOC_LITERAL(18, 183, 14), // "onPlotSelected"
QT_MOC_LITERAL(19, 198, 11), // "NRadarItem*"
QT_MOC_LITERAL(20, 210, 11), // "executeTask"
QT_MOC_LITERAL(21, 222, 8), // "QAction*"
QT_MOC_LITERAL(22, 231, 3) // "act"

    },
    "AppWindow\0loadMap\0\0importData\0confirm\0"
    "initFilters\0const DataPack*\0data\0"
    "applyFilter\0fullFilter\0applyFilterToLayers\0"
    "updateViewMode\0m\0addItemToDetails\0"
    "QTreeWidgetItem*\0parent\0name\0value\0"
    "onPlotSelected\0NRadarItem*\0executeTask\0"
    "QAction*\0act"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_AppWindow[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      12,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   74,    2, 0x09 /* Protected */,
       3,    1,   75,    2, 0x09 /* Protected */,
       3,    0,   78,    2, 0x29 /* Protected | MethodCloned */,
       5,    1,   79,    2, 0x09 /* Protected */,
       8,    1,   82,    2, 0x09 /* Protected */,
       8,    0,   85,    2, 0x29 /* Protected | MethodCloned */,
      10,    0,   86,    2, 0x09 /* Protected */,
      11,    1,   87,    2, 0x09 /* Protected */,
      13,    3,   90,    2, 0x09 /* Protected */,
      13,    2,   97,    2, 0x29 /* Protected | MethodCloned */,
      18,    1,  102,    2, 0x09 /* Protected */,
      20,    1,  105,    2, 0x09 /* Protected */,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Bool, QMetaType::Bool,    4,
    QMetaType::Bool,
    QMetaType::Void, 0x80000000 | 6,    7,
    QMetaType::Void, QMetaType::Bool,    9,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Int,   12,
    0x80000000 | 14, 0x80000000 | 14, QMetaType::QString, QMetaType::QString,   15,   16,   17,
    0x80000000 | 14, 0x80000000 | 14, QMetaType::QString,   15,   16,
    QMetaType::Void, 0x80000000 | 19,    2,
    QMetaType::Void, 0x80000000 | 21,   22,

       0        // eod
};

void AppWindow::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<AppWindow *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->loadMap(); break;
        case 1: { bool _r = _t->importData((*reinterpret_cast< bool(*)>(_a[1])));
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 2: { bool _r = _t->importData();
            if (_a[0]) *reinterpret_cast< bool*>(_a[0]) = std::move(_r); }  break;
        case 3: _t->initFilters((*reinterpret_cast< const DataPack*(*)>(_a[1]))); break;
        case 4: _t->applyFilter((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 5: _t->applyFilter(); break;
        case 6: _t->applyFilterToLayers(); break;
        case 7: _t->updateViewMode((*reinterpret_cast< int(*)>(_a[1]))); break;
        case 8: { QTreeWidgetItem* _r = _t->addItemToDetails((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])),(*reinterpret_cast< const QString(*)>(_a[3])));
            if (_a[0]) *reinterpret_cast< QTreeWidgetItem**>(_a[0]) = std::move(_r); }  break;
        case 9: { QTreeWidgetItem* _r = _t->addItemToDetails((*reinterpret_cast< QTreeWidgetItem*(*)>(_a[1])),(*reinterpret_cast< const QString(*)>(_a[2])));
            if (_a[0]) *reinterpret_cast< QTreeWidgetItem**>(_a[0]) = std::move(_r); }  break;
        case 10: _t->onPlotSelected((*reinterpret_cast< NRadarItem*(*)>(_a[1]))); break;
        case 11: _t->executeTask((*reinterpret_cast< QAction*(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        switch (_id) {
        default: *reinterpret_cast<int*>(_a[0]) = -1; break;
        case 11:
            switch (*reinterpret_cast<int*>(_a[1])) {
            default: *reinterpret_cast<int*>(_a[0]) = -1; break;
            case 0:
                *reinterpret_cast<int*>(_a[0]) = qRegisterMetaType< QAction* >(); break;
            }
            break;
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject AppWindow::staticMetaObject = { {
    QMetaObject::SuperData::link<QMainWindow::staticMetaObject>(),
    qt_meta_stringdata_AppWindow.data,
    qt_meta_data_AppWindow,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *AppWindow::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *AppWindow::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_AppWindow.stringdata0))
        return static_cast<void*>(this);
    if (!strcmp(_clname, "IAnalyser"))
        return static_cast< IAnalyser*>(this);
    return QMainWindow::qt_metacast(_clname);
}

int AppWindow::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QMainWindow::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 12)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 12;
    }
    return _id;
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
