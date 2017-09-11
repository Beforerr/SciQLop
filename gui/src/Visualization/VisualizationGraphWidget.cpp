#include "Visualization/VisualizationGraphWidget.h"
#include "Visualization/IVisualizationWidgetVisitor.h"
#include "Visualization/VisualizationDefs.h"
#include "Visualization/VisualizationGraphHelper.h"
#include "Visualization/VisualizationGraphRenderingDelegate.h"
#include "ui_VisualizationGraphWidget.h"

#include <Data/ArrayData.h>
#include <Data/IDataSeries.h>
#include <Settings/SqpSettingsDefs.h>
#include <SqpApplication.h>
#include <Variable/Variable.h>
#include <Variable/VariableController.h>

#include <unordered_map>

Q_LOGGING_CATEGORY(LOG_VisualizationGraphWidget, "VisualizationGraphWidget")

namespace {

/// Key pressed to enable zoom on horizontal axis
const auto HORIZONTAL_ZOOM_MODIFIER = Qt::NoModifier;

/// Key pressed to enable zoom on vertical axis
const auto VERTICAL_ZOOM_MODIFIER = Qt::ControlModifier;

} // namespace

struct VisualizationGraphWidget::VisualizationGraphWidgetPrivate {

    explicit VisualizationGraphWidgetPrivate(const QString &name)
            : m_Name{name},
              m_DoAcquisition{true},
              m_IsCalibration{false},
              m_RenderingDelegate{nullptr}
    {
    }

    QString m_Name;
    // 1 variable -> n qcpplot
    std::map<std::shared_ptr<Variable>, PlottablesMap> m_VariableToPlotMultiMap;
    bool m_DoAcquisition;
    bool m_IsCalibration;
    QCPItemTracer *m_TextTracer;
    /// Delegate used to attach rendering features to the plot
    std::unique_ptr<VisualizationGraphRenderingDelegate> m_RenderingDelegate;
};

VisualizationGraphWidget::VisualizationGraphWidget(const QString &name, QWidget *parent)
        : QWidget{parent},
          ui{new Ui::VisualizationGraphWidget},
          impl{spimpl::make_unique_impl<VisualizationGraphWidgetPrivate>(name)}
{
    ui->setupUi(this);

    // 'Close' options : widget is deleted when closed
    setAttribute(Qt::WA_DeleteOnClose);

    // Set qcpplot properties :
    // - Drag (on x-axis) and zoom are enabled
    // - Mouse wheel on qcpplot is intercepted to determine the zoom orientation
    ui->widget->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectItems);
    ui->widget->axisRect()->setRangeDrag(Qt::Horizontal);

    // The delegate must be initialized after the ui as it uses the plot
    impl->m_RenderingDelegate = std::make_unique<VisualizationGraphRenderingDelegate>(*this);

    connect(ui->widget, &QCustomPlot::mousePress, this, &VisualizationGraphWidget::onMousePress);
    connect(ui->widget, &QCustomPlot::mouseRelease, this,
            &VisualizationGraphWidget::onMouseRelease);
    connect(ui->widget, &QCustomPlot::mouseMove, this, &VisualizationGraphWidget::onMouseMove);
    connect(ui->widget, &QCustomPlot::mouseWheel, this, &VisualizationGraphWidget::onMouseWheel);
    connect(ui->widget->xAxis, static_cast<void (QCPAxis::*)(const QCPRange &, const QCPRange &)>(
                                   &QCPAxis::rangeChanged),
            this, &VisualizationGraphWidget::onRangeChanged, Qt::DirectConnection);

    // Activates menu when right clicking on the graph
    ui->widget->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(ui->widget, &QCustomPlot::customContextMenuRequested, this,
            &VisualizationGraphWidget::onGraphMenuRequested);

    connect(this, &VisualizationGraphWidget::requestDataLoading, &sqpApp->variableController(),
            &VariableController::onRequestDataLoading);

    connect(&sqpApp->variableController(), &VariableController::updateVarDisplaying, this,
            &VisualizationGraphWidget::onUpdateVarDisplaying);
}


VisualizationGraphWidget::~VisualizationGraphWidget()
{
    delete ui;
}

void VisualizationGraphWidget::enableAcquisition(bool enable)
{
    impl->m_DoAcquisition = enable;
}

void VisualizationGraphWidget::addVariable(std::shared_ptr<Variable> variable, SqpRange range)
{
    // Uses delegate to create the qcpplot components according to the variable
    auto createdPlottables = VisualizationGraphHelper::create(variable, *ui->widget);
    impl->m_VariableToPlotMultiMap.insert({variable, std::move(createdPlottables)});

    // Set axes properties according to the units of the data series
    /// @todo : for the moment, no control is performed on the axes: the units and the tickers
    /// are fixed for the default x-axis and y-axis of the plot, and according to the new graph
    auto xAxisUnit = Unit{};
    auto valuesUnit = Unit{};

    if (auto dataSeries = variable->dataSeries()) {
        dataSeries->lockRead();
        xAxisUnit = dataSeries->xAxisUnit();
        valuesUnit = dataSeries->valuesUnit();
        dataSeries->unlock();
    }
    impl->m_RenderingDelegate->setAxesProperties(xAxisUnit, valuesUnit);

    connect(variable.get(), SIGNAL(updated()), this, SLOT(onDataCacheVariableUpdated()));

    auto varRange = variable->range();

    this->enableAcquisition(false);
    this->setGraphRange(range);
    this->enableAcquisition(true);

    emit requestDataLoading(QVector<std::shared_ptr<Variable> >() << variable, range, varRange,
                            false);

    emit variableAdded(variable);
}

void VisualizationGraphWidget::removeVariable(std::shared_ptr<Variable> variable) noexcept
{
    // Each component associated to the variable :
    // - is removed from qcpplot (which deletes it)
    // - is no longer referenced in the map
    auto variableIt = impl->m_VariableToPlotMultiMap.find(variable);
    if (variableIt != impl->m_VariableToPlotMultiMap.cend()) {
        auto &plottablesMap = variableIt->second;

        for (auto plottableIt = plottablesMap.cbegin(), plottableEnd = plottablesMap.cend();
             plottableIt != plottableEnd;) {
            ui->widget->removePlottable(plottableIt->second);
            plottableIt = plottablesMap.erase(plottableIt);
        }

        impl->m_VariableToPlotMultiMap.erase(variableIt);
    }

    // Updates graph
    ui->widget->replot();
}

void VisualizationGraphWidget::setRange(std::shared_ptr<Variable> variable, const SqpRange &range)
{
    // Note: in case of different axes that depends on variable, we could start with a code like
    // that:
    //    auto componentsIt = impl->m_VariableToPlotMultiMap.equal_range(variable);
    //    for (auto it = componentsIt.first; it != componentsIt.second;) {
    //    }
    ui->widget->xAxis->setRange(range.m_TStart, range.m_TEnd);
    ui->widget->replot();
}

void VisualizationGraphWidget::setYRange(const SqpRange &range)
{
    ui->widget->yAxis->setRange(range.m_TStart, range.m_TEnd);
}

SqpRange VisualizationGraphWidget::graphRange() const noexcept
{
    auto graphRange = ui->widget->xAxis->range();
    return SqpRange{graphRange.lower, graphRange.upper};
}

void VisualizationGraphWidget::setGraphRange(const SqpRange &range)
{
    qCDebug(LOG_VisualizationGraphWidget()) << tr("VisualizationGraphWidget::setGraphRange START");
    ui->widget->xAxis->setRange(range.m_TStart, range.m_TEnd);
    ui->widget->replot();
    qCDebug(LOG_VisualizationGraphWidget()) << tr("VisualizationGraphWidget::setGraphRange END");
}

void VisualizationGraphWidget::accept(IVisualizationWidgetVisitor *visitor)
{
    if (visitor) {
        visitor->visit(this);
    }
    else {
        qCCritical(LOG_VisualizationGraphWidget())
            << tr("Can't visit widget : the visitor is null");
    }
}

bool VisualizationGraphWidget::canDrop(const Variable &variable) const
{
    /// @todo : for the moment, a graph can always accomodate a variable
    Q_UNUSED(variable);
    return true;
}

bool VisualizationGraphWidget::contains(const Variable &variable) const
{
    // Finds the variable among the keys of the map
    auto variablePtr = &variable;
    auto findVariable
        = [variablePtr](const auto &entry) { return variablePtr == entry.first.get(); };

    auto end = impl->m_VariableToPlotMultiMap.cend();
    auto it = std::find_if(impl->m_VariableToPlotMultiMap.cbegin(), end, findVariable);
    return it != end;
}

QString VisualizationGraphWidget::name() const
{
    return impl->m_Name;
}

void VisualizationGraphWidget::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    impl->m_RenderingDelegate->showGraphOverlay(true);
}

void VisualizationGraphWidget::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    impl->m_RenderingDelegate->showGraphOverlay(false);
}

QCustomPlot &VisualizationGraphWidget::plot() noexcept
{
    return *ui->widget;
}

void VisualizationGraphWidget::onGraphMenuRequested(const QPoint &pos) noexcept
{
    QMenu graphMenu{};

    // Iterates on variables (unique keys)
    for (auto it = impl->m_VariableToPlotMultiMap.cbegin(),
              end = impl->m_VariableToPlotMultiMap.cend();
         it != end; it = impl->m_VariableToPlotMultiMap.upper_bound(it->first)) {
        // 'Remove variable' action
        graphMenu.addAction(tr("Remove variable %1").arg(it->first->name()),
                            [ this, var = it->first ]() { removeVariable(var); });
    }

    if (!graphMenu.isEmpty()) {
        graphMenu.exec(QCursor::pos());
    }
}

void VisualizationGraphWidget::onRangeChanged(const QCPRange &t1, const QCPRange &t2)
{
    qCDebug(LOG_VisualizationGraphWidget()) << tr("TORM: VisualizationGraphWidget::onRangeChanged")
                                            << QThread::currentThread()->objectName() << "DoAcqui"
                                            << impl->m_DoAcquisition;

    auto graphRange = SqpRange{t1.lower, t1.upper};
    auto oldGraphRange = SqpRange{t2.lower, t2.upper};

    if (impl->m_DoAcquisition) {
        QVector<std::shared_ptr<Variable> > variableUnderGraphVector;

        for (auto it = impl->m_VariableToPlotMultiMap.begin(),
                  end = impl->m_VariableToPlotMultiMap.end();
             it != end; it = impl->m_VariableToPlotMultiMap.upper_bound(it->first)) {
            variableUnderGraphVector.push_back(it->first);
        }
        emit requestDataLoading(std::move(variableUnderGraphVector), graphRange, oldGraphRange,
                                !impl->m_IsCalibration);

        if (!impl->m_IsCalibration) {
            qCDebug(LOG_VisualizationGraphWidget())
                << tr("TORM: VisualizationGraphWidget::Synchronize notify !!")
                << QThread::currentThread()->objectName() << graphRange << oldGraphRange;
            emit synchronize(graphRange, oldGraphRange);
        }
    }
}

void VisualizationGraphWidget::onMouseMove(QMouseEvent *event) noexcept
{
    // Handles plot rendering when mouse is moving
    impl->m_RenderingDelegate->onMouseMove(event);
}

void VisualizationGraphWidget::onMouseWheel(QWheelEvent *event) noexcept
{
    auto zoomOrientations = QFlags<Qt::Orientation>{};

    // Lambda that enables a zoom orientation if the key modifier related to this orientation
    // has
    // been pressed
    auto enableOrientation
        = [&zoomOrientations, event](const auto &orientation, const auto &modifier) {
              auto orientationEnabled = event->modifiers().testFlag(modifier);
              zoomOrientations.setFlag(orientation, orientationEnabled);
          };
    enableOrientation(Qt::Vertical, VERTICAL_ZOOM_MODIFIER);
    enableOrientation(Qt::Horizontal, HORIZONTAL_ZOOM_MODIFIER);

    ui->widget->axisRect()->setRangeZoom(zoomOrientations);
}

void VisualizationGraphWidget::onMousePress(QMouseEvent *event) noexcept
{
    impl->m_IsCalibration = event->modifiers().testFlag(Qt::ControlModifier);
}

void VisualizationGraphWidget::onMouseRelease(QMouseEvent *event) noexcept
{
    impl->m_IsCalibration = false;
}

void VisualizationGraphWidget::onDataCacheVariableUpdated()
{
    auto graphRange = ui->widget->xAxis->range();
    auto dateTime = SqpRange{graphRange.lower, graphRange.upper};

    for (auto &variableEntry : impl->m_VariableToPlotMultiMap) {
        auto variable = variableEntry.first;
        qCDebug(LOG_VisualizationGraphWidget())
            << "TORM: VisualizationGraphWidget::onDataCacheVariableUpdated S" << variable->range();
        qCDebug(LOG_VisualizationGraphWidget())
            << "TORM: VisualizationGraphWidget::onDataCacheVariableUpdated E" << dateTime;
        if (dateTime.contains(variable->range()) || dateTime.intersect(variable->range())) {
            VisualizationGraphHelper::updateData(variableEntry.second, variable->dataSeries(),
                                                 variable->range());
        }
    }
}

void VisualizationGraphWidget::onUpdateVarDisplaying(std::shared_ptr<Variable> variable,
                                                     const SqpRange &range)
{
    auto it = impl->m_VariableToPlotMultiMap.find(variable);
    if (it != impl->m_VariableToPlotMultiMap.end()) {
        VisualizationGraphHelper::updateData(it->second, variable->dataSeries(), range);
    }
}
