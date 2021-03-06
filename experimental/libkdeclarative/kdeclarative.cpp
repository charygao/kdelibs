/*
 *   Copyright 2011 Marco Martin <mart@kde.org>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Library General Public License as
 *   published by the Free Software Foundation; either version 2, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details
 *
 *   You should have received a copy of the GNU Library General Public
 *   License along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include "kdeclarative.h"
#include "bindings/i18n_p.h"
#include "private/kdeclarative_p.h"
#include "private/engineaccess_p.h"
#include "private/kiconprovider_p.h"

#include <QtDeclarative/QDeclarativeComponent>
#include <QtDeclarative/QDeclarativeContext>
#include <QtDeclarative/QDeclarativeEngine>
#include <QtDeclarative/QDeclarativeExpression>
#include <QtDeclarative/qdeclarativedebug.h>
#include <QtScript/QScriptEngine>
#include <QtScript/QScriptValueIterator>
#include <QtCore/QWeakPointer>

#include <kcmdlineargs.h>
#include <kdebug.h>
#include <kglobal.h>
#include <kstandarddirs.h>
#include <kurl.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

void registerNonGuiMetaTypes(QScriptEngine *engine);
QScriptValue constructIconClass(QScriptEngine *engine);
QScriptValue constructKUrlClass(QScriptEngine *engine);

KDeclarativePrivate::KDeclarativePrivate()
    : initialized(false)
{
}

KDeclarative::KDeclarative()
    : d(new KDeclarativePrivate)
{
}

KDeclarative::~KDeclarative()
{
    delete d;
}


void KDeclarative::setDeclarativeEngine(QDeclarativeEngine *engine)
{
    if (d->declarativeEngine.data() == engine) {
        return;
    }
    d->initialized = false;
    d->declarativeEngine = engine;
}

QDeclarativeEngine *KDeclarative::declarativeEngine() const
{
    return d->declarativeEngine.data();
}

void KDeclarative::initialize()
{
    //Glorious hack:steal the engine
    //create the access object
    EngineAccess *engineAccess = new EngineAccess(this);
    d->declarativeEngine.data()->rootContext()->setContextProperty("__engineAccess", engineAccess);

    //make engineaccess set our d->scriptengine
    QDeclarativeExpression *expr = new QDeclarativeExpression(d->declarativeEngine.data()->rootContext(), d->declarativeEngine.data()->rootContext()->contextObject(), "__engineAccess.setEngine(this)");
    expr->evaluate();
    delete expr;

    //we don't need engineaccess anymore
    d->declarativeEngine.data()->rootContext()->setContextProperty("__engineAccess", 0);
    engineAccess->deleteLater();

    //fail?
    if (!d->scriptEngine) {
        kWarning() << "Failed to get the script engine";
        return;
    }

    //change the old globalobject with a new read/write copy
    QScriptValue originalGlobalObject = d->scriptEngine.data()->globalObject();

    QScriptValue newGlobalObject = d->scriptEngine.data()->newObject();

    QString eval = QLatin1String("eval");
    QString version = QLatin1String("version");

    {
        QScriptValueIterator iter(originalGlobalObject);
        QVector<QString> names;
        QVector<QScriptValue> values;
        QVector<QScriptValue::PropertyFlags> flags;
        while (iter.hasNext()) {
            iter.next();

            QString name = iter.name();

            if (name == version) {
                continue;
            }

            if (name != eval) {
                names.append(name);
                values.append(iter.value());
                flags.append(iter.flags() | QScriptValue::Undeletable);
            }
            newGlobalObject.setProperty(iter.scriptName(), iter.value());

           // m_illegalNames.insert(name);
        }

    }

    d->scriptEngine.data()->setGlobalObject(newGlobalObject);

    d->initialized = true;
}

void KDeclarative::setupBindings()
{
    QScriptEngine *engine = d->scriptEngine.data();
    if (!engine) {
        return;
    }

    /*tell the engine to search for import in the kde4 plugin dirs.
    addImportPath adds the path at the beginning, so to honour user's
    paths we need to traverse the list in reverse order*/

    const QStringList importPathList = KGlobal::dirs()->findDirs("module", "imports");
    QStringListIterator importPathIterator(importPathList);
    importPathIterator.toBack();
    while (importPathIterator.hasPrevious()) {
        d->declarativeEngine.data()->addImportPath(importPathIterator.previous());
    }

    const QString target = componentsTarget();
    if (target != defaultComponentsTarget()) {
        const QStringList paths = KGlobal::dirs()->findDirs("module", "platformimports/" % target);
        QStringListIterator it(paths);
        it.toBack();
        while (it.hasPrevious()) {
            d->declarativeEngine.data()->addImportPath(it.previous());
        }
    }

    QScriptValue global = engine->globalObject();

    //KConfig and KJob
    registerNonGuiMetaTypes(d->scriptEngine.data());

    // Stuff from Qt
    global.setProperty("QIcon", constructIconClass(engine));

    // Add stuff from KDE libs
    bindI18N(engine);
    qScriptRegisterSequenceMetaType<KUrl::List>(engine);
    global.setProperty("Url", constructKUrlClass(engine));

    // setup ImageProvider for KDE icons
    d->declarativeEngine.data()->addImageProvider(QString("icon"), new KIconProvider);
}

QScriptEngine *KDeclarative::scriptEngine() const
{
    return d->scriptEngine.data();
}

void KDeclarative::setupQmlJsDebugger()
{
    if (KCmdLineArgs::parsedArgs("qt")->isSet("qmljsdebugger")) {
        QDeclarativeDebuggingEnabler enabler;
    }
}

QString KDeclarative::defaultComponentsTarget()
{
    return QLatin1String("desktop");
}

QString KDeclarative::componentsTarget()
{
    const QStringList platform = runtimePlatform();
    if (platform.isEmpty()) {
        return defaultComponentsTarget();
    }

    return platform.last();
}

QStringList KDeclarative::runtimePlatform()
{
    static QStringList *runtimePlatform = 0;
    if (!runtimePlatform) {
        const QString env = getenv("PLASMA_PLATFORM");
        runtimePlatform = new QStringList(env.split(":", QString::SkipEmptyParts));
        if (runtimePlatform->isEmpty()) {
            KConfigGroup cg(KGlobal::config(), "General");
            *runtimePlatform = cg.readEntry("runtimePlatform", *runtimePlatform);
        }
    }

    return *runtimePlatform;
}

