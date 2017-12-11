#ifndef SCIQLOP_CATALOGUEEVENTSTABLEMODEL_H
#define SCIQLOP_CATALOGUEEVENTSTABLEMODEL_H

#include <Common/spimpl.h>
#include <QAbstractTableModel>

#include <DBEvent.h>

class CatalogueEventsTableModel : public QAbstractTableModel {
public:
    CatalogueEventsTableModel(QObject *parent = nullptr);

    void setEvents(const QVector<DBEvent> &events);
    DBEvent getEvent(int row) const;


    // Model
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    Qt::DropActions supportedDragActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;


private:
    class CatalogueEventsTableModelPrivate;
    spimpl::unique_impl_ptr<CatalogueEventsTableModelPrivate> impl;
};

#endif // SCIQLOP_CATALOGUEEVENTSTABLEMODEL_H
