/****************************************************************************
** Meta object code from reading C++ file 'reflectorstask.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.2)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../reflectorstask.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'reflectorstask.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.2. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_ReflectorsTask_t {
    QByteArrayData data[3];
    char stringdata0[32];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ReflectorsTask_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ReflectorsTask_t qt_meta_stringdata_ReflectorsTask = {
    {
QT_MOC_LITERAL(0, 0, 14), // "ReflectorsTask"
QT_MOC_LITERAL(1, 15, 15), // "removeReflector"
QT_MOC_LITERAL(2, 31, 0) // ""

    },
    "ReflectorsTask\0removeReflector\0"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ReflectorsTask[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       1,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       0,       // signalCount

 // slots: name, argc, parameters, tag, flags
       1,    0,   19,    2, 0x09 /* Protected */,

 // slots: parameters
    QMetaType::Void,

       0        // eod
};

void ReflectorsTask::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ReflectorsTask *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->removeReflector(); break;
        default: ;
        }
    }
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject ReflectorsTask::staticMetaObject = { {
    QMetaObject::SuperData::link<AnalyserTask::staticMetaObject>(),
    qt_meta_stringdata_ReflectorsTask.data,
    qt_meta_data_ReflectorsTask,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ReflectorsTask::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ReflectorsTask::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ReflectorsTask.stringdata0))
        return static_cast<void*>(this);
    return AnalyserTask::qt_metacast(_clname);
}

int ReflectorsTask::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = AnalyserTask::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 1)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 1;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 1)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 1;
    }
    return _id;
}
struct qt_meta_stringdata_ReflectorItem_t {
    QByteArrayData data[4];
    char stringdata0[34];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_ReflectorItem_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_ReflectorItem_t qt_meta_stringdata_ReflectorItem = {
    {
QT_MOC_LITERAL(0, 0, 13), // "ReflectorItem"
QT_MOC_LITERAL(1, 14, 8), // "removeMe"
QT_MOC_LITERAL(2, 23, 0), // ""
QT_MOC_LITERAL(3, 24, 9) // "hideLabel"

    },
    "ReflectorItem\0removeMe\0\0hideLabel"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_ReflectorItem[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
       2,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    0,   24,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       3,    0,   25,    2, 0x09 /* Protected */,

 // signals: parameters
    QMetaType::Void,

 // slots: parameters
    QMetaType::Void,

       0        // eod
};

void ReflectorItem::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<ReflectorItem *>(_o);
        Q_UNUSED(_t)
        switch (_id) {
        case 0: _t->removeMe(); break;
        case 1: _t->hideLabel(); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (ReflectorItem::*)();
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&ReflectorItem::removeMe)) {
                *result = 0;
                return;
            }
        }
    }
    Q_UNUSED(_a);
}

QT_INIT_METAOBJECT const QMetaObject ReflectorItem::staticMetaObject = { {
    QMetaObject::SuperData::link<NRadarObjectItem::staticMetaObject>(),
    qt_meta_stringdata_ReflectorItem.data,
    qt_meta_data_ReflectorItem,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *ReflectorItem::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *ReflectorItem::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_ReflectorItem.stringdata0))
        return static_cast<void*>(this);
    return NRadarObjectItem::qt_metacast(_clname);
}

int ReflectorItem::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = NRadarObjectItem::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 2)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 2;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 2)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 2;
    }
    return _id;
}

// SIGNAL 0
void ReflectorItem::removeMe()
{
    QMetaObject::activate(this, &staticMetaObject, 0, nullptr);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
