#include "CosinusProvider.h"
#include "MockDefs.h"

#include <Data/DataProviderParameters.h>
#include <Data/ScalarSeries.h>
#include <Data/VectorSeries.h>

#include <cmath>

#include <QFuture>
#include <QThread>
#include <QtConcurrent/QtConcurrent>

Q_LOGGING_CATEGORY(LOG_CosinusProvider, "CosinusProvider")

namespace {

/// Abstract cosinus type
struct ICosinusType {
    virtual ~ICosinusType() = default;
    /// @return the number of components generated for the type
    virtual int componentCount() const = 0;
    /// @return the data series created for the type
    virtual std::shared_ptr<IDataSeries> createDataSeries(std::vector<double> xAxisData,
                                                          std::vector<double> valuesData,
                                                          Unit xAxisUnit,
                                                          Unit valuesUnit) const = 0;
};

struct ScalarCosinus : public ICosinusType {
    int componentCount() const override { return 1; }

    std::shared_ptr<IDataSeries> createDataSeries(std::vector<double> xAxisData,
                                                  std::vector<double> valuesData, Unit xAxisUnit,
                                                  Unit valuesUnit) const override
    {
        return std::make_shared<ScalarSeries>(std::move(xAxisData), std::move(valuesData),
                                              xAxisUnit, valuesUnit);
    }
};
struct VectorCosinus : public ICosinusType {
    int componentCount() const override { return 3; }

    std::shared_ptr<IDataSeries> createDataSeries(std::vector<double> xAxisData,
                                                  std::vector<double> valuesData, Unit xAxisUnit,
                                                  Unit valuesUnit) const override
    {
        return std::make_shared<VectorSeries>(std::move(xAxisData), std::move(valuesData),
                                              xAxisUnit, valuesUnit);
    }
};

/// Converts string to cosinus type
/// @return the cosinus type if the string could be converted, nullptr otherwise
std::unique_ptr<ICosinusType> cosinusType(const QString &type) noexcept
{
    if (type.compare(QStringLiteral("scalar"), Qt::CaseInsensitive) == 0) {
        return std::make_unique<ScalarCosinus>();
    }
    else if (type.compare(QStringLiteral("vector"), Qt::CaseInsensitive) == 0) {
        return std::make_unique<VectorCosinus>();
    }
    else {
        return nullptr;
    }
}

} // namespace

std::shared_ptr<IDataProvider> CosinusProvider::clone() const
{
    // No copy is made in clone
    return std::make_shared<CosinusProvider>();
}

std::shared_ptr<IDataSeries> CosinusProvider::retrieveData(QUuid acqIdentifier,
                                                           const SqpRange &dataRangeRequested,
                                                           const QVariantHash &data)
{
    // TODO: Add Mutex
    auto dataIndex = 0;

    // Retrieves cosinus type
    auto typeVariant = data.value(COSINUS_TYPE_KEY, COSINUS_TYPE_DEFAULT_VALUE);
    if (!typeVariant.canConvert<QString>()) {
        qCCritical(LOG_CosinusProvider()) << tr("Can't retrieve data: invalid type");
        return nullptr;
    }

    auto type = cosinusType(typeVariant.toString());
    if (!type) {
        qCCritical(LOG_CosinusProvider()) << tr("Can't retrieve data: unknown type");
        return nullptr;
    }

    // Retrieves frequency
    auto freqVariant = data.value(COSINUS_FREQUENCY_KEY, COSINUS_FREQUENCY_DEFAULT_VALUE);
    if (!freqVariant.canConvert<double>()) {
        qCCritical(LOG_CosinusProvider()) << tr("Can't retrieve data: invalid frequency");
        return nullptr;
    }

    // Gets the timerange from the parameters
    double freq = freqVariant.toDouble();
    double start = std::ceil(dataRangeRequested.m_TStart * freq);
    double end = std::floor(dataRangeRequested.m_TEnd * freq);

    // We assure that timerange is valid
    if (end < start) {
        std::swap(start, end);
    }

    // Generates scalar series containing cosinus values (one value per second, end value is
    // included)
    auto dataCount = end - start + 1;

    // Number of components (depending on the cosinus type)
    auto componentCount = type->componentCount();

    auto xAxisData = std::vector<double>{};
    xAxisData.resize(dataCount);

    auto valuesData = std::vector<double>{};
    valuesData.resize(dataCount * componentCount);

    int progress = 0;
    auto progressEnd = dataCount;
    for (auto time = start; time <= end; ++time, ++dataIndex) {
        auto it = m_VariableToEnableProvider.find(acqIdentifier);
        if (it != m_VariableToEnableProvider.end() && it.value()) {
            const auto timeOnFreq = time / freq;

            xAxisData[dataIndex] = timeOnFreq;

            // Generates all components' values
            // Example: for a vector, values will be : cos(x), cos(x)/2, cos(x)/3
            auto value = std::cos(timeOnFreq);
            for (auto i = 0; i < componentCount; ++i) {
                valuesData[componentCount * dataIndex + i] = value / (i + 1);
            }

            // progression
            int currentProgress = (time - start) * 100.0 / progressEnd;
            if (currentProgress != progress) {
                progress = currentProgress;

                emit dataProvidedProgress(acqIdentifier, progress);
                qCDebug(LOG_CosinusProvider()) << "TORM: CosinusProvider::retrieveData"
                                               << QThread::currentThread()->objectName()
                                               << progress;
                // NOTE: Try to use multithread if possible
            }
        }
        else {
            if (!it.value()) {
                qCDebug(LOG_CosinusProvider())
                    << "CosinusProvider::retrieveData: ARRET De l'acquisition detecté"
                    << end - time;
            }
        }
    }
    if (progress != 100) {
        // We can close progression beacause all data has been retrieved
        emit dataProvidedProgress(acqIdentifier, 100);
    }
    return type->createDataSeries(std::move(xAxisData), std::move(valuesData),
                                  Unit{QStringLiteral("t"), true}, Unit{});
}

void CosinusProvider::requestDataLoading(QUuid acqIdentifier,
                                         const DataProviderParameters &parameters)
{
    // TODO: Add Mutex
    m_VariableToEnableProvider[acqIdentifier] = true;
    qCDebug(LOG_CosinusProvider()) << "TORM: CosinusProvider::requestDataLoading"
                                   << QThread::currentThread()->objectName();
    // NOTE: Try to use multithread if possible
    const auto times = parameters.m_Times;

    for (const auto &dateTime : qAsConst(times)) {
        if (m_VariableToEnableProvider[acqIdentifier]) {
            auto scalarSeries = this->retrieveData(acqIdentifier, dateTime, parameters.m_Data);
            emit dataProvided(acqIdentifier, scalarSeries, dateTime);
        }
    }
}

void CosinusProvider::requestDataAborting(QUuid acqIdentifier)
{
    qCDebug(LOG_CosinusProvider()) << "CosinusProvider::requestDataAborting" << acqIdentifier
                                   << QThread::currentThread()->objectName();
    auto it = m_VariableToEnableProvider.find(acqIdentifier);
    if (it != m_VariableToEnableProvider.end()) {
        it.value() = false;
    }
    else {
        qCDebug(LOG_CosinusProvider())
            << tr("Aborting progression of inexistant identifier detected !!!");
    }
}

std::shared_ptr<IDataSeries> CosinusProvider::provideDataSeries(const SqpRange &dataRangeRequested,
                                                                const QVariantHash &data)
{
    auto uid = QUuid::createUuid();
    m_VariableToEnableProvider[uid] = true;
    auto dataSeries = this->retrieveData(uid, dataRangeRequested, data);

    m_VariableToEnableProvider.remove(uid);
    return dataSeries;
}
