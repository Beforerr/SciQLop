#ifndef SCIQLOP_AMDARESULTPARSER_H
#define SCIQLOP_AMDARESULTPARSER_H

#include "AmdaGlobal.h"

#include <QLoggingCategory>

#include <memory>

class IDataSeries;

Q_DECLARE_LOGGING_CATEGORY(LOG_AmdaResultParser)

struct SCIQLOP_AMDA_EXPORT AmdaResultParser {
    enum class ValueType { SCALAR, VECTOR, UNKNOWN };

    static std::shared_ptr<IDataSeries> readTxt(const QString &filePath,
                                                ValueType valueType) noexcept;
};

#endif // SCIQLOP_AMDARESULTPARSER_H
