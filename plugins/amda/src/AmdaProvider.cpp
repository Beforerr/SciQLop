#include "AmdaProvider.h"
#include "AmdaDefs.h"
#include "AmdaResultParser.h"

#include <Common/DateUtils.h>
#include <Data/DataProviderParameters.h>
#include <Network/NetworkController.h>
#include <SqpApplication.h>
#include <Variable/Variable.h>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QTemporaryFile>
#include <QThread>

Q_LOGGING_CATEGORY(LOG_AmdaProvider, "AmdaProvider")

namespace {

/// URL format for a request on AMDA server. The parameters are as follows:
/// - %1: start date
/// - %2: end date
/// - %3: parameter id
const auto AMDA_URL_FORMAT = QStringLiteral(
    "http://amda.irap.omp.eu/php/rest/"
    "getParameter.php?startTime=%1&stopTime=%2&parameterID=%3&outputFormat=ASCII&"
    "timeFormat=ISO8601&gzip=0");

/// Dates format passed in the URL (e.g 2013-09-23T09:00)
const auto AMDA_TIME_FORMAT = QStringLiteral("yyyy-MM-ddThh:mm:ss");

// struct AmdaProgression {
//    QUuid acqIdentifier;
//    std::map<QNetworkRequest, double> m_RequestId;
//};

/// Formats a time to a date that can be passed in URL
QString dateFormat(double sqpRange) noexcept
{
    auto dateTime = DateUtils::dateTime(sqpRange);
    return dateTime.toString(AMDA_TIME_FORMAT);
}

AmdaResultParser::ValueType valueType(const QString &valueType)
{
    if (valueType == QStringLiteral("scalar")) {
        return AmdaResultParser::ValueType::SCALAR;
    }
    else if (valueType == QStringLiteral("vector")) {
        return AmdaResultParser::ValueType::VECTOR;
    }
    else {
        return AmdaResultParser::ValueType::UNKNOWN;
    }
}

} // namespace

AmdaProvider::AmdaProvider()
{
    qCDebug(LOG_AmdaProvider()) << tr("AmdaProvider::AmdaProvider") << QThread::currentThread();
    if (auto app = sqpApp) {
        auto &networkController = app->networkController();
        connect(this, SIGNAL(requestConstructed(QNetworkRequest, QUuid,
                                                std::function<void(QNetworkReply *, QUuid)>)),
                &networkController,
                SLOT(onProcessRequested(QNetworkRequest, QUuid,
                                        std::function<void(QNetworkReply *, QUuid)>)));


        connect(&sqpApp->networkController(),
                SIGNAL(replyDownloadProgress(QUuid, const QNetworkRequest &, double)), this,
                SLOT(onReplyDownloadProgress(QUuid, const QNetworkRequest &, double)));
    }
}

std::shared_ptr<IDataProvider> AmdaProvider::clone() const
{
    // No copy is made in the clone
    return std::make_shared<AmdaProvider>();
}

void AmdaProvider::requestDataLoading(QUuid acqIdentifier, const DataProviderParameters &parameters)
{
    // NOTE: Try to use multithread if possible
    const auto times = parameters.m_Times;
    const auto data = parameters.m_Data;
    for (const auto &dateTime : qAsConst(times)) {
        this->retrieveData(acqIdentifier, dateTime, data);


        // TORM when AMDA will support quick asynchrone request
        QThread::msleep(1000);
    }
}

void AmdaProvider::requestDataAborting(QUuid acqIdentifier)
{
    if (auto app = sqpApp) {
        auto &networkController = app->networkController();
        networkController.onReplyCanceled(acqIdentifier);
    }
}

void AmdaProvider::onReplyDownloadProgress(QUuid acqIdentifier,
                                           const QNetworkRequest &networkRequest, double progress)
{
    qCCritical(LOG_AmdaProvider()) << tr("onReplyDownloadProgress") << progress;
    auto acqIdToRequestProgressMapIt = m_AcqIdToRequestProgressMap.find(acqIdentifier);
    if (acqIdToRequestProgressMapIt != m_AcqIdToRequestProgressMap.end()) {

        qCCritical(LOG_AmdaProvider()) << tr("1 onReplyDownloadProgress") << progress;
        auto requestPtr = &networkRequest;
        auto findRequest
            = [requestPtr](const auto &entry) { return requestPtr == entry.first.get(); };

        auto &requestProgressMap = acqIdToRequestProgressMapIt->second;
        auto requestProgressMapEnd = requestProgressMap.end();
        auto requestProgressMapIt
            = std::find_if(requestProgressMap.begin(), requestProgressMapEnd, findRequest);

        if (requestProgressMapIt != requestProgressMapEnd) {
            requestProgressMapIt->second = progress;
        }
        else {
            qCCritical(LOG_AmdaProvider()) << tr("Can't retrieve Request in progress");
        }
    }

    acqIdToRequestProgressMapIt = m_AcqIdToRequestProgressMap.find(acqIdentifier);
    if (acqIdToRequestProgressMapIt != m_AcqIdToRequestProgressMap.end()) {
        qCCritical(LOG_AmdaProvider()) << tr("2 onReplyDownloadProgress") << progress;
        double finalProgress = 0.0;

        auto &requestProgressMap = acqIdToRequestProgressMapIt->second;
        auto fraq = requestProgressMap.size();

        for (auto requestProgress : requestProgressMap) {
            finalProgress += requestProgress.second;
        }

        if (fraq > 0) {
            finalProgress = finalProgress / fraq;
        }

        qCCritical(LOG_AmdaProvider()) << tr("2 onReplyDownloadProgress") << finalProgress;
        emit dataProvidedProgress(acqIdentifier, finalProgress);
    }
    else {
        emit dataProvidedProgress(acqIdentifier, 0.0);
    }
}

void AmdaProvider::retrieveData(QUuid token, const SqpRange &dateTime, const QVariantHash &data)
{
    // Retrieves product ID from data: if the value is invalid, no request is made
    auto productId = data.value(AMDA_XML_ID_KEY).toString();
    if (productId.isNull()) {
        qCCritical(LOG_AmdaProvider()) << tr("Can't retrieve data: unknown product id");
        return;
    }
    qCDebug(LOG_AmdaProvider()) << tr("AmdaProvider::retrieveData") << dateTime;

    // Retrieves the data type that determines whether the expected format for the result file is
    // scalar, vector...
    auto productValueType = valueType(data.value(AMDA_DATA_TYPE_KEY).toString());

    // /////////// //
    // Creates URL //
    // /////////// //

    auto startDate = dateFormat(dateTime.m_TStart);
    auto endDate = dateFormat(dateTime.m_TEnd);

    auto url = QUrl{QString{AMDA_URL_FORMAT}.arg(startDate, endDate, productId)};
    qCInfo(LOG_AmdaProvider()) << tr("TORM AmdaProvider::retrieveData url:") << url;
    auto tempFile = std::make_shared<QTemporaryFile>();

    // LAMBDA
    auto httpDownloadFinished = [this, dateTime, tempFile,
                                 productValueType](QNetworkReply *reply, QUuid dataId) noexcept {

        // Don't do anything if the reply was abort
        if (reply->error() != QNetworkReply::OperationCanceledError) {

            if (tempFile) {
                auto replyReadAll = reply->readAll();
                if (!replyReadAll.isEmpty()) {
                    tempFile->write(replyReadAll);
                }
                tempFile->close();

                // Parse results file
                if (auto dataSeries
                    = AmdaResultParser::readTxt(tempFile->fileName(), productValueType)) {
                    emit dataProvided(dataId, dataSeries, dateTime);
                }
                else {
                    /// @todo ALX : debug
                }
            }
            m_AcqIdToRequestProgressMap.erase(dataId);
        }

    };
    auto httpFinishedLambda
        = [this, httpDownloadFinished, tempFile](QNetworkReply *reply, QUuid dataId) noexcept {

              // Don't do anything if the reply was abort
              if (reply->error() != QNetworkReply::OperationCanceledError) {
                  auto downloadFileUrl = QUrl{QString{reply->readAll()}};

                  qCInfo(LOG_AmdaProvider())
                      << tr("TORM AmdaProvider::retrieveData downloadFileUrl:") << downloadFileUrl;
                  // Executes request for downloading file //

                  // Creates destination file
                  if (tempFile->open()) {
                      // Executes request
                      auto request = std::make_shared<QNetworkRequest>(downloadFileUrl);
                      updateRequestProgress(dataId, request, 0.0);
                      emit requestConstructed(*request.get(), dataId, httpDownloadFinished);
                  }
              }
              else {
                  m_AcqIdToRequestProgressMap.erase(dataId);
              }
          };

    // //////////////// //
    // Executes request //
    // //////////////// //

    auto request = std::make_shared<QNetworkRequest>(url);
    updateRequestProgress(token, request, 0.0);

    emit requestConstructed(*request.get(), token, httpFinishedLambda);
}

void AmdaProvider::updateRequestProgress(QUuid acqIdentifier,
                                         std::shared_ptr<QNetworkRequest> request, double progress)
{
    auto acqIdToRequestProgressMapIt = m_AcqIdToRequestProgressMap.find(acqIdentifier);
    if (acqIdToRequestProgressMapIt != m_AcqIdToRequestProgressMap.end()) {
        auto &requestProgressMap = acqIdToRequestProgressMapIt->second;
        auto requestProgressMapIt = requestProgressMap.find(request);
        if (requestProgressMapIt != requestProgressMap.end()) {
            requestProgressMapIt->second = progress;
        }
        else {
            acqIdToRequestProgressMapIt->second.insert(std::make_pair(request, progress));
        }
    }
    else {
        auto requestProgressMap = std::map<std::shared_ptr<QNetworkRequest>, double>{};
        requestProgressMap.insert(std::make_pair(request, progress));
        m_AcqIdToRequestProgressMap.insert(
            std::make_pair(acqIdentifier, std::move(requestProgressMap)));
    }
}
