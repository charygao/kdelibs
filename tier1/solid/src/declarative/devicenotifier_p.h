/*
 *   Copyright (C) 2013 Ivan Cukic <ivan.cukic(at)kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2,
 *   or (at your option) any later version, as published by the Free
 *   Software Foundation
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef SOLID_DECALARATIVE_DEVICE_NOTIFIER_P_H
#define SOLID_DECALARATIVE_DEVICE_NOTIFIER_P_H

#include "devicenotifier.h"

#include <solid/devicenotifier.h>
#include <solid/device.h>

class SolidDeviceNotifierPrivate: public QObject {
    Q_OBJECT

public:
    SolidDeviceNotifierPrivate(SolidDeviceNotifier * parent);

    void emitChange() const;

    void initialize();
    void reset();

    SolidDeviceNotifier * const q;
    Solid::DeviceNotifier * notifier;
    Solid::Predicate predicate;
    QString query;
    QStringList devices;

    bool initialized;

public Q_SLOTS:
    void addDevice(const QString & udi);
    void removeDevice(const QString & udi);
};

#endif
