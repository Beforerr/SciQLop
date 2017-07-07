#ifndef SCIQLOP_COSINUSPROVIDER_H
#define SCIQLOP_COSINUSPROVIDER_H

#include "MockPluginGlobal.h"

#include <Data/IDataProvider.h>

#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(LOG_CosinusProvider)

/**
 * @brief The CosinusProvider class is an example of how a data provider can generate data
 */
class SCIQLOP_MOCKPLUGIN_EXPORT CosinusProvider : public IDataProvider {
public:
    void requestDataLoading(const QVector<SqpDateTime> &dateTimeList) override;


private:
    /// @sa IDataProvider::retrieveData()
    std::shared_ptr<IDataSeries> retrieveData(const DataProviderParameters &parameters) const;
    std::shared_ptr<IDataSeries> retrieveDataSeries(const SqpDateTime &dateTime);
};

#endif // SCIQLOP_COSINUSPROVIDER_H
