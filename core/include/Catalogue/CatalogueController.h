#ifndef SCIQLOP_CATALOGUECONTROLLER_H
#define SCIQLOP_CATALOGUECONTROLLER_H

#include "CoreGlobal.h"

#include <Data/SqpRange.h>

#include <QLoggingCategory>
#include <QObject>
#include <QUuid>

#include <Common/spimpl.h>

#include <memory>

class DBCatalogue;
class DBEvent;

Q_DECLARE_LOGGING_CATEGORY(LOG_CatalogueController)

class DataSourceItem;
class Variable;

/**
 * @brief The CatalogueController class aims to handle catalogues and event using the CatalogueAPI
 * library.
 */
class SCIQLOP_CORE_EXPORT CatalogueController : public QObject {
    Q_OBJECT
public:
    explicit CatalogueController(QObject *parent = 0);
    virtual ~CatalogueController();

    // DB
    QStringList getRepositories() const;
    void addDB(const QString &dbPath);
    void saveDB(const QString &destinationPath, const QString &repository);

    // Event
    bool createEvent(const QString &name);
    std::list<std::shared_ptr<DBEvent> > retrieveEvents(const QString &repository) const;
    std::list<std::shared_ptr<DBEvent> > retrieveAllEvents() const;
    void updateEvent(std::shared_ptr<DBEvent> event);
    void trashEvent(std::shared_ptr<DBEvent> event);
    void removeEvent(std::shared_ptr<DBEvent> event);
    void restore(QUuid eventId);
    void saveEvent(std::shared_ptr<DBEvent> event);

    // Catalogue
    bool createCatalogue(const QString &name, QVector<QUuid> eventList);
    void getCatalogues(const QString &repository) const;
    void removeEvent(QUuid catalogueId, const QString &repository);
    void saveCatalogue(std::shared_ptr<DBEvent> event);

public slots:
    /// Manage init/end of the controller
    void initialize();
    void finalize();

private:
    void waitForFinish();

    class CatalogueControllerPrivate;
    spimpl::unique_impl_ptr<CatalogueControllerPrivate> impl;
};

#endif // SCIQLOP_CATALOGUECONTROLLER_H
