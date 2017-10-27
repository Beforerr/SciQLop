#include "Visualization/VisualizationTabWidget.h"
#include "Visualization/IVisualizationWidgetVisitor.h"
#include "ui_VisualizationTabWidget.h"

#include "Visualization/VisualizationGraphWidget.h"
#include "Visualization/VisualizationZoneWidget.h"

#include "Variable/VariableController.h"

#include "Common/MimeTypesDef.h"

#include "DragDropHelper.h"
#include "SqpApplication.h"

Q_LOGGING_CATEGORY(LOG_VisualizationTabWidget, "VisualizationTabWidget")

namespace {

/// Generates a default name for a new zone, according to the number of zones already displayed in
/// the tab
QString defaultZoneName(const QLayout &layout)
{
    auto count = 0;
    for (auto i = 0; i < layout.count(); ++i) {
        if (dynamic_cast<VisualizationZoneWidget *>(layout.itemAt(i)->widget())) {
            count++;
        }
    }

    return QObject::tr("Zone %1").arg(count + 1);
}

/**
 * Applies a function to all zones of the tab represented by its layout
 * @param layout the layout that contains zones
 * @param fun the function to apply to each zone
 */
template <typename Fun>
void processZones(QLayout &layout, Fun fun)
{
    for (auto i = 0; i < layout.count(); ++i) {
        if (auto item = layout.itemAt(i)) {
            if (auto visualizationZoneWidget
                = dynamic_cast<VisualizationZoneWidget *>(item->widget())) {
                fun(*visualizationZoneWidget);
            }
        }
    }
}

} // namespace

struct VisualizationTabWidget::VisualizationTabWidgetPrivate {
    explicit VisualizationTabWidgetPrivate(const QString &name) : m_Name{name} {}

    QString m_Name;

    void dropGraph(int index, VisualizationTabWidget *tabWidget);
    void dropZone(int index, VisualizationTabWidget *tabWidget);
    void dropVariables(const QList<std::shared_ptr<Variable> > &variables, int index,
                       VisualizationTabWidget *tabWidget);
};

VisualizationTabWidget::VisualizationTabWidget(const QString &name, QWidget *parent)
        : QWidget{parent},
          ui{new Ui::VisualizationTabWidget},
          impl{spimpl::make_unique_impl<VisualizationTabWidgetPrivate>(name)}
{
    ui->setupUi(this);

    ui->dragDropContainer->setAcceptedMimeTypes(
        {MIME_TYPE_GRAPH, MIME_TYPE_ZONE, MIME_TYPE_VARIABLE_LIST});
    connect(ui->dragDropContainer, &VisualizationDragDropContainer::dropOccured, this,
            &VisualizationTabWidget::dropMimeData);
    ui->dragDropContainer->setAcceptMimeDataFunction([this](auto mimeData) {
        return sqpApp->dragDropHelper().checkMimeDataForVisualization(mimeData,
                                                                      ui->dragDropContainer);
    });
    sqpApp->dragDropHelper().addDragDropScrollArea(ui->scrollArea);

    // Widget is deleted when closed
    setAttribute(Qt::WA_DeleteOnClose);
}

VisualizationTabWidget::~VisualizationTabWidget()
{
    sqpApp->dragDropHelper().removeDragDropScrollArea(ui->scrollArea);
    delete ui;
}

void VisualizationTabWidget::addZone(VisualizationZoneWidget *zoneWidget)
{
    ui->dragDropContainer->addDragWidget(zoneWidget);
}

void VisualizationTabWidget::insertZone(int index, VisualizationZoneWidget *zoneWidget)
{
    ui->dragDropContainer->insertDragWidget(index, zoneWidget);
}

VisualizationZoneWidget *VisualizationTabWidget::createZone(std::shared_ptr<Variable> variable)
{
    return createZone({variable}, -1);
}

VisualizationZoneWidget *
VisualizationTabWidget::createZone(const QList<std::shared_ptr<Variable> > &variables, int index)
{
    auto zoneWidget = createEmptyZone(index);

    // Creates a new graph into the zone
    zoneWidget->createGraph(variables, index);

    return zoneWidget;
}

VisualizationZoneWidget *VisualizationTabWidget::createEmptyZone(int index)
{
    auto zoneWidget
        = new VisualizationZoneWidget{defaultZoneName(*ui->dragDropContainer->layout()), this};
    this->insertZone(index, zoneWidget);

    return zoneWidget;
}

void VisualizationTabWidget::accept(IVisualizationWidgetVisitor *visitor)
{
    if (visitor) {
        visitor->visitEnter(this);

        // Apply visitor to zone children: widgets different from zones are not visited (no action)
        processZones(tabLayout(), [visitor](VisualizationZoneWidget &zoneWidget) {
            zoneWidget.accept(visitor);
        });

        visitor->visitLeave(this);
    }
    else {
        qCCritical(LOG_VisualizationTabWidget()) << tr("Can't visit widget : the visitor is null");
    }
}

bool VisualizationTabWidget::canDrop(const Variable &variable) const
{
    // A tab can always accomodate a variable
    Q_UNUSED(variable);
    return true;
}

bool VisualizationTabWidget::contains(const Variable &variable) const
{
    Q_UNUSED(variable);
    return false;
}

QString VisualizationTabWidget::name() const
{
    return impl->m_Name;
}

void VisualizationTabWidget::closeEvent(QCloseEvent *event)
{
    // Closes zones in the tab
    processZones(tabLayout(), [](VisualizationZoneWidget &zoneWidget) { zoneWidget.close(); });

    QWidget::closeEvent(event);
}

QLayout &VisualizationTabWidget::tabLayout() const noexcept
{
    return *ui->dragDropContainer->layout();
}

void VisualizationTabWidget::dropMimeData(int index, const QMimeData *mimeData)
{
    if (mimeData->hasFormat(MIME_TYPE_GRAPH)) {
        impl->dropGraph(index, this);
    }
    else if (mimeData->hasFormat(MIME_TYPE_ZONE)) {
        impl->dropZone(index, this);
    }
    else if (mimeData->hasFormat(MIME_TYPE_VARIABLE_LIST)) {
        auto variables = sqpApp->variableController().variablesForMimeData(
            mimeData->data(MIME_TYPE_VARIABLE_LIST));
        impl->dropVariables(variables, index, this);
    }
    else {
        qCWarning(LOG_VisualizationZoneWidget())
            << tr("VisualizationTabWidget::dropMimeData, unknown MIME data received.");
    }
}

void VisualizationTabWidget::VisualizationTabWidgetPrivate::dropGraph(
    int index, VisualizationTabWidget *tabWidget)
{
    auto &helper = sqpApp->dragDropHelper();

    auto graphWidget = qobject_cast<VisualizationGraphWidget *>(helper.getCurrentDragWidget());
    if (!graphWidget) {
        qCWarning(LOG_VisualizationZoneWidget())
            << tr("VisualizationTabWidget::dropGraph, drop aborted, the dropped graph is not "
                  "found or invalid.");
        Q_ASSERT(false);
        return;
    }

    auto parentDragDropContainer
        = qobject_cast<VisualizationDragDropContainer *>(graphWidget->parentWidget());
    if (!parentDragDropContainer) {
        qCWarning(LOG_VisualizationZoneWidget())
            << tr("VisualizationTabWidget::dropGraph, drop aborted, the parent container of "
                  "the dropped graph is not found.");
        Q_ASSERT(false);
        return;
    }

    auto nbGraph = parentDragDropContainer->countDragWidget();

    const auto &variables = graphWidget->variables();

    if (!variables.isEmpty()) {
        // Abort the requests for the variables (if any)
        // Commented, because it's not sure if it's needed or not
        // for (const auto& var : variables)
        //{
        //    sqpApp->variableController().onAbortProgressRequested(var);
        //}

        if (nbGraph == 1) {
            // This is the only graph in the previous zone, close the zone
            graphWidget->parentZoneWidget()->close();
        }
        else {
            // Close the graph
            graphWidget->close();
        }

        tabWidget->createZone(variables, index);
    }
    else {
        // The graph is empty, create an empty zone and move the graph inside

        auto parentZoneWidget = graphWidget->parentZoneWidget();

        parentDragDropContainer->layout()->removeWidget(graphWidget);

        auto zoneWidget = tabWidget->createEmptyZone(index);
        zoneWidget->addGraph(graphWidget);

        // Close the old zone if it was the only graph inside
        if (nbGraph == 1) {
            parentZoneWidget->close();
        }
    }
}

void VisualizationTabWidget::VisualizationTabWidgetPrivate::dropZone(
    int index, VisualizationTabWidget *tabWidget)
{
    auto &helper = sqpApp->dragDropHelper();

    auto zoneWidget = qobject_cast<VisualizationZoneWidget *>(helper.getCurrentDragWidget());
    if (!zoneWidget) {
        qCWarning(LOG_VisualizationZoneWidget())
            << tr("VisualizationTabWidget::dropZone, drop aborted, the dropped zone is not "
                  "found or invalid.");
        Q_ASSERT(false);
        return;
    }

    auto parentDragDropContainer
        = qobject_cast<VisualizationDragDropContainer *>(zoneWidget->parentWidget());
    if (!parentDragDropContainer) {
        qCWarning(LOG_VisualizationZoneWidget())
            << tr("VisualizationTabWidget::dropZone, drop aborted, the parent container of "
                  "the dropped zone is not found.");
        Q_ASSERT(false);
        return;
    }

    // Simple move of the zone, no variable operation associated
    parentDragDropContainer->layout()->removeWidget(zoneWidget);
    tabWidget->ui->dragDropContainer->insertDragWidget(index, zoneWidget);
}

void VisualizationTabWidget::VisualizationTabWidgetPrivate::dropVariables(
    const QList<std::shared_ptr<Variable> > &variables, int index,
    VisualizationTabWidget *tabWidget)
{
    // Note: we are sure that there is a single and compatible variable here
    // because the AcceptMimeDataFunction, set on the drop container, makes the check before the
    // drop can occur.
    tabWidget->createZone(variables, index);
}
