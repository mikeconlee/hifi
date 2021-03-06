//
//  SharedObject.h
//  metavoxels
//
//  Created by Andrzej Kapolka on 2/5/14.
//  Copyright (c) 2014 High Fidelity, Inc. All rights reserved.
//

#ifndef __interface__SharedObject__
#define __interface__SharedObject__

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QPointer>
#include <QSet>
#include <QWidget>
#include <QtDebug>

class QComboBox;

class SharedObject;

typedef QHash<int, QPointer<SharedObject> > WeakSharedObjectHash;

/// A QObject that may be shared over the network.
class SharedObject : public QObject {
    Q_OBJECT
    
public:

    /// Returns the weak hash under which all local shared objects are registered.
    static const WeakSharedObjectHash& getWeakHash() { return _weakHash; }

    Q_INVOKABLE SharedObject();

    /// Returns the unique local ID for this object.
    int getID() const { return _id; }

    /// Returns the unique remote ID for this object, or zero if this is a local object.
    int getRemoteID() const { return _remoteID; }
    
    void setRemoteID(int remoteID) { _remoteID = remoteID; }

    int getReferenceCount() const { return _referenceCount; }
    void incrementReferenceCount();
    void decrementReferenceCount();

    /// Creates a new clone of this object.
    /// \param withID if true, give the clone the same ID as this object
    virtual SharedObject* clone(bool withID = false) const;

    /// Tests this object for equality with another.    
    virtual bool equals(const SharedObject* other) const;

    // Dumps the contents of this object to the debug output.
    virtual void dump(QDebug debug = QDebug(QtDebugMsg)) const;

private:
    
    void setID(int id);
    
    int _id;
    int _remoteID;
    int _referenceCount;
    
    static int _lastID;
    static WeakSharedObjectHash _weakHash;
};

/// Removes the null references from the supplied hash.
void pruneWeakSharedObjectHash(WeakSharedObjectHash& hash);

/// A pointer to a shared object.
template<class T> class SharedObjectPointerTemplate {
public:
    
    SharedObjectPointerTemplate(T* data = NULL);
    SharedObjectPointerTemplate(const SharedObjectPointerTemplate<T>& other);
    ~SharedObjectPointerTemplate();

    T* data() const { return _data; }
    
    /// "Detaches" this object, making a new copy if its reference count is greater than one.
    bool detach();

    void swap(SharedObjectPointerTemplate<T>& other) { qSwap(_data, other._data); }

    void reset();

    bool operator!() const { return !_data; }
    operator T*() const { return _data; }
    T& operator*() const { return *_data; }
    T* operator->() const { return _data; }
    
    template<class X> SharedObjectPointerTemplate<X> staticCast() const;
    
    SharedObjectPointerTemplate<T>& operator=(T* data);
    SharedObjectPointerTemplate<T>& operator=(const SharedObjectPointerTemplate<T>& other);

    bool operator==(const SharedObjectPointerTemplate<T>& other) const { return _data == other._data; }
    bool operator!=(const SharedObjectPointerTemplate<T>& other) const { return _data != other._data; }
    
private:

    T* _data;
};

template<class T> inline SharedObjectPointerTemplate<T>::SharedObjectPointerTemplate(T* data) : _data(data) {
    if (_data) {
        _data->incrementReferenceCount();
    }
}

template<class T> inline SharedObjectPointerTemplate<T>::SharedObjectPointerTemplate(const SharedObjectPointerTemplate<T>& other) :
    _data(other._data) {
    
    if (_data) {
        _data->incrementReferenceCount();
    }
}

template<class T> inline SharedObjectPointerTemplate<T>::~SharedObjectPointerTemplate() {
    if (_data) {
        _data->decrementReferenceCount();
    }
}

template<class T> inline bool SharedObjectPointerTemplate<T>::detach() {
    if (_data && _data->getReferenceCount() > 1) {
        _data->decrementReferenceCount();
        (_data = _data->clone())->incrementReferenceCount();
        return true;
    }
    return false;
}

template<class T> inline void SharedObjectPointerTemplate<T>::reset() {
    if (_data) {
        _data->decrementReferenceCount();
    }
    _data = NULL;
}

template<class T> template<class X> inline SharedObjectPointerTemplate<X> SharedObjectPointerTemplate<T>::staticCast() const {
    return SharedObjectPointerTemplate<X>(static_cast<X*>(_data));
}

template<class T> inline SharedObjectPointerTemplate<T>& SharedObjectPointerTemplate<T>::operator=(T* data) {
    if (_data) {
        _data->decrementReferenceCount();
    }
    if ((_data = data)) {
        _data->incrementReferenceCount();
    }
    return *this;
}

template<class T> inline SharedObjectPointerTemplate<T>& SharedObjectPointerTemplate<T>::operator=(
        const SharedObjectPointerTemplate<T>& other) {
    if (_data) {
        _data->decrementReferenceCount();
    }
    if ((_data = other._data)) {
        _data->incrementReferenceCount();
    }
    return *this;
}

template<class T> uint qHash(const SharedObjectPointerTemplate<T>& pointer, uint seed = 0) {
    return qHash(pointer.data(), seed);
}

template<class T, class X> bool equals(const SharedObjectPointerTemplate<T>& first,
        const SharedObjectPointerTemplate<X>& second) {
    return first ? first->equals(second) : !second;
}

typedef SharedObjectPointerTemplate<SharedObject> SharedObjectPointer;

Q_DECLARE_METATYPE(SharedObjectPointer)

typedef QSet<SharedObjectPointer> SharedObjectSet;

Q_DECLARE_METATYPE(SharedObjectSet)

/// Allows editing shared object instances.
class SharedObjectEditor : public QWidget {
    Q_OBJECT
    Q_PROPERTY(SharedObjectPointer object READ getObject WRITE setObject USER true)

public:
    
    SharedObjectEditor(const QMetaObject* metaObject, bool nullable = true, QWidget* parent = NULL);

    const SharedObjectPointer& getObject() const { return _object; }

    /// "Detaches" the object pointer, copying it if anyone else is holding a reference.
    void detachObject();

public slots:

    void setObject(const SharedObjectPointer& object);

private slots:

    void updateType();
    void propertyChanged();
    void updateProperty();

private:
    
    QComboBox* _type;
    SharedObjectPointer _object;
};

#endif /* defined(__interface__SharedObject__) */
