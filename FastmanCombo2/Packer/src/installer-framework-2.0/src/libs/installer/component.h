/**************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: http://www.qt.io/licensing/
**
** This file is part of the Qt Installer Framework.
**
** $QT_BEGIN_LICENSE:LGPL21$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see http://www.qt.io/terms-conditions. For further
** information use the contact form at http://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 2.1 or version 3 as published by the Free
** Software Foundation and appearing in the file LICENSE.LGPLv21 and
** LICENSE.LGPLv3 included in the packaging of this file. Please review the
** following information to ensure the GNU Lesser General Public License
** requirements will be met: https://www.gnu.org/licenses/lgpl.html and
** http://www.gnu.org/licenses/old-licenses/lgpl-2.1.html.
**
** As a special exception, The Qt Company gives you certain additional
** rights. These rights are described in The Qt Company LGPL Exception
** version 1.1, included in the file LGPL_EXCEPTION.txt in this package.
**
** $QT_END_LICENSE$
**
**************************************************************************/

#ifndef COMPONENT_H
#define COMPONENT_H

#include "constants.h"
#include "component_p.h"
#include "qinstallerglobal.h"

#include <QtCore/QDir>
#include <QtCore/QMetaType>
#include <QtCore/QObject>
#include <QtCore/QUrl>

QT_FORWARD_DECLARE_CLASS(QDebug)

namespace KDUpdater {
    class Update;
    struct PackageInfo;
}

namespace QInstaller {

class PackageManagerCore;

class INSTALLER_EXPORT Component : public QObject, public ComponentModelHelper
{
    Q_OBJECT
    Q_DISABLE_COPY(Component)

    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QString displayName READ displayName)
    Q_PROPERTY(bool autoCreateOperations READ autoCreateOperations WRITE setAutoCreateOperations)
    Q_PROPERTY(QStringList archives READ archives)
    Q_PROPERTY(QStringList userInterfaces READ userInterfaces)
    Q_PROPERTY(QStringList dependencies READ dependencies)
    Q_PROPERTY(QStringList autoDependencies READ autoDependencies)
    Q_PROPERTY(bool fromOnlineRepository READ isFromOnlineRepository)
    Q_PROPERTY(QUrl repositoryUrl READ repositoryUrl)
    Q_PROPERTY(bool default READ isDefault)
    Q_PROPERTY(bool installed READ isInstalled)
    Q_PROPERTY(bool enabled READ isEnabled WRITE setEnabled)

public:
    explicit Component(PackageManagerCore *core);
    ~Component();

    struct SortingPriorityLessThan
    {
        bool operator() (const Component *lhs, const Component *rhs) const
        {
            return lhs->value(scSortingPriority).toInt() < rhs->value(scSortingPriority).toInt();
        }
    };

    struct SortingPriorityGreaterThan
    {
        bool operator() (const Component *lhs, const Component *rhs) const
        {
            return lhs->value(scSortingPriority).toInt() > rhs->value(scSortingPriority).toInt();
        }
    };

    void loadDataFromPackage(const Package &package);
    void loadDataFromPackage(const LocalPackage &package);

    QHash<QString, QString> variables() const;
    Q_INVOKABLE void setValue(const QString &key, const QString &value);
    Q_INVOKABLE QString value(const QString &key, const QString &defaultValue = QString()) const;

    QStringList archives() const;
    PackageManagerCore *packageManagerCore() const;

    Component *parentComponent() const;
    void appendComponent(Component *component);
    void removeComponent(Component *component);
    QList<Component*> descendantComponents() const;

    void loadComponentScript();

    //move this to private
    void loadComponentScript(const QString &fileName);
    void loadTranslations(const QDir &directory, const QStringList &qms);
    void loadUserInterfaces(const QDir &directory, const QStringList &uis);
    void loadLicenses(const QString &directory, const QHash<QString, QVariant> &hash);
    void markAsPerformedInstallation();

    QStringList userInterfaces() const;
    QHash<QString, QPair<QString, QString> > licenses() const;
    Q_INVOKABLE QWidget *userInterface(const QString &name) const;
    Q_INVOKABLE virtual void beginInstallation();
    Q_INVOKABLE virtual void createOperations();
    Q_INVOKABLE virtual void createOperationsForArchive(const QString &archive);
    Q_INVOKABLE virtual void createOperationsForPath(const QString &path);

    QList<QPair<QString, bool> > pathsForUninstallation() const;
    Q_INVOKABLE void registerPathForUninstallation(const QString &path, bool wipe = false);

    OperationList operations() const;

    void addOperation(Operation *operation);
    Q_INVOKABLE bool addOperation(const QString &operation, const QString &parameter1 = QString(),
        const QString &parameter2 = QString(), const QString &parameter3 = QString(),
        const QString &parameter4 = QString(), const QString &parameter5 = QString(),
        const QString &parameter6 = QString(), const QString &parameter7 = QString(),
        const QString &parameter8 = QString(), const QString &parameter9 = QString(),
        const QString &parameter10 = QString());
    Q_INVOKABLE bool addOperation(const QString &operation, const QStringList &parameters);

    void addElevatedOperation(Operation *operation);
    Q_INVOKABLE bool addElevatedOperation(const QString &operation,
        const QString &parameter1 = QString(), const QString &parameter2 = QString(),
        const QString &parameter3 = QString(), const QString &parameter4 = QString(),
        const QString &parameter5 = QString(), const QString &parameter6 = QString(),
        const QString &parameter7 = QString(), const QString &parameter8 = QString(),
        const QString &parameter9 = QString(), const QString &parameter10 = QString());
    Q_INVOKABLE bool addElevatedOperation(const QString &operation, const QStringList &parameters);

    QStringList downloadableArchives() const;
    Q_INVOKABLE void addDownloadableArchive(const QString &path);
    Q_INVOKABLE void removeDownloadableArchive(const QString &path);

    QStringList stopProcessForUpdateRequests() const;
    Q_INVOKABLE void addStopProcessForUpdateRequest(const QString &process);
    Q_INVOKABLE void removeStopProcessForUpdateRequest(const QString &process);
    Q_INVOKABLE void setStopProcessForUpdateRequest(const QString &process, bool requested);

    QString name() const;
    QString displayName() const;
    quint64 updateUncompressedSize();

    QUrl repositoryUrl() const;
    void setRepositoryUrl(const QUrl &url);

    Q_INVOKABLE void addDependency(const QString &newDependency);
    QStringList dependencies() const;
    QStringList autoDependencies() const;

    void languageChanged();
    QString localTempPath() const;

    bool autoCreateOperations() const;
    bool operationsCreatedSuccessfully() const;

    Q_INVOKABLE bool isDefault() const;
    Q_INVOKABLE bool isAutoDependOn(const QSet<QString> &componentsToInstall) const;

    Q_INVOKABLE void setInstalled();
    Q_INVOKABLE bool isInstalled() const;
    Q_INVOKABLE bool installationRequested() const;
    bool isSelectedForInstallation() const;

    Q_INVOKABLE void setUninstalled();
    Q_INVOKABLE bool isUninstalled() const;
    Q_INVOKABLE bool uninstallationRequested() const;

    Q_INVOKABLE bool isFromOnlineRepository() const;

    Q_INVOKABLE void setUpdateAvailable(bool isUpdateAvailable);
    Q_INVOKABLE bool updateRequested();

    Q_INVOKABLE bool componentChangeRequested();


    bool isVirtual() const;
    bool isSelected() const;
    bool forcedInstallation() const;

    void setValidatorCallbackName(const QString &name);

    bool validatePage();

public Q_SLOTS:
    void setAutoCreateOperations(bool autoCreateOperations);

Q_SIGNALS:
    void loaded();
    void virtualStateChanged();
    void valueChanged(const QString &key, const QString &value);

private Q_SLOTS:
    void updateModelData(const QString &key, const QString &value);

private:
    void setLocalTempPath(const QString &tempPath);

    Operation *createOperation(const QString &operationName, const QString &parameter1 = QString(),
        const QString &parameter2 = QString(), const QString &parameter3 = QString(),
        const QString &parameter4 = QString(), const QString &parameter5 = QString(),
        const QString &parameter6 = QString(), const QString &parameter7 = QString(),
        const QString &parameter8 = QString(), const QString &parameter9 = QString(),
        const QString &parameter10 = QString());
    Operation *createOperation(const QString &operationName, const QStringList &parameters);

private:
    QString validatorCallbackName;
    ComponentPrivate *d;
};

QDebug operator<<(QDebug dbg, Component *component);

}   // namespace QInstaller

Q_DECLARE_METATYPE(QInstaller::Component*)

#endif // COMPONENT_H