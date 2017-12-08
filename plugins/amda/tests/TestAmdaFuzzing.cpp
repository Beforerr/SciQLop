#include "FuzzingDefs.h"
#include "FuzzingOperations.h"
#include "FuzzingUtils.h"

#include <Network/NetworkController.h>
#include <SqpApplication.h>
#include <Time/TimeController.h>
#include <Variable/VariableController.h>

#include <QLoggingCategory>
#include <QObject>
#include <QtTest>

#include <memory>

Q_LOGGING_CATEGORY(LOG_TestAmdaFuzzing, "TestAmdaFuzzing")

namespace {

// /////// //
// Aliases //
// /////// //

using VariableId = int;

using VariableOperation = std::pair<VariableId, std::shared_ptr<IFuzzingOperation> >;
using VariablesOperations = std::vector<VariableOperation>;

using OperationsPool = std::set<std::shared_ptr<IFuzzingOperation> >;
using VariablesPool = std::map<VariableId, std::shared_ptr<Variable> >;

// ///////// //
// Constants //
// ///////// //

// Defaults values used when the associated properties have not been set for the test
const auto NB_MAX_OPERATIONS_DEFAULT_VALUE = 100;
const auto NB_MAX_VARIABLES_DEFAULT_VALUE = 1;
const auto AVAILABLE_OPERATIONS_DEFAULT_VALUE
    = QVariant::fromValue(OperationsTypes{FuzzingOperationType::CREATE});

// /////// //
// Methods //
// /////// //

/// Goes through the variables pool and operations pool to determine the set of {variable/operation}
/// pairs that are valid (i.e. operation that can be executed on variable)
VariablesOperations availableOperations(const VariablesPool &variablesPool,
                                        const OperationsPool &operationsPool)
{
    VariablesOperations result{};

    for (const auto &variablesPoolEntry : variablesPool) {
        auto variableId = variablesPoolEntry.first;
        auto variable = variablesPoolEntry.second;

        for (const auto &operation : operationsPool) {
            // A pair is valid if the current operation can be executed on the current variable
            if (operation->canExecute(variable)) {
                result.push_back({variableId, operation});
            }
        }
    }

    return result;
}

OperationsPool createOperationsPool(const OperationsTypes &types)
{
    OperationsPool result{};

    std::transform(types.cbegin(), types.cend(), std::inserter(result, result.end()),
                   [](const auto &type) { return FuzzingOperationFactory::create(type); });

    return result;
}

/**
 * Class to run random tests
 */
class FuzzingTest {
public:
    explicit FuzzingTest(VariableController &variableController, Properties properties)
            : m_VariableController{variableController},
              m_Properties{std::move(properties)},
              m_VariablesPool{}
    {
        // Inits variables pool: at init, all variables are null
        for (auto variableId = 0; variableId < nbMaxVariables(); ++variableId) {
            m_VariablesPool[variableId] = nullptr;
        }
    }

    void execute()
    {
        qCInfo(LOG_TestAmdaFuzzing()) << "Running" << nbMaxOperations() << "operations on"
                                      << nbMaxVariables() << "variables...";

        auto canExecute = true;
        for (auto i = 0; i < nbMaxOperations() && canExecute; ++i) {
            // Retrieves all operations that can be executed in the current context
            auto variableOperations = availableOperations(m_VariablesPool, operationsPool());

            canExecute = !variableOperations.empty();
            if (canExecute) {
                // Of the operations available, chooses a random operation and executes it
                auto variableOperation
                    = RandomGenerator::instance().randomChoice(variableOperations);

                auto variableId = variableOperation.first;
                auto variable = m_VariablesPool.at(variableId);
                auto fuzzingOperation = variableOperation.second;

                fuzzingOperation->execute(variable, m_VariableController, m_Properties);

                // Updates variable pool with the new state of the variable after operation
                m_VariablesPool[variableId] = variable;
            }
            else {
                qCInfo(LOG_TestAmdaFuzzing())
                    << "No more operations are available, the execution of the test will stop...";
            }
        }

        qCInfo(LOG_TestAmdaFuzzing()) << "Execution of the test completed.";
    }

private:
    int nbMaxOperations() const
    {
        static auto result
            = m_Properties.value(NB_MAX_OPERATIONS_PROPERTY, NB_MAX_OPERATIONS_DEFAULT_VALUE)
                  .toInt();
        return result;
    }

    int nbMaxVariables() const
    {
        static auto result
            = m_Properties.value(NB_MAX_VARIABLES_PROPERTY, NB_MAX_VARIABLES_DEFAULT_VALUE).toInt();
        return result;
    }

    OperationsPool operationsPool() const
    {
        static auto result = createOperationsPool(
            m_Properties.value(AVAILABLE_OPERATIONS_PROPERTY, AVAILABLE_OPERATIONS_DEFAULT_VALUE)
                .value<OperationsTypes>());
        return result;
    }

    VariableController &m_VariableController;
    Properties m_Properties;
    VariablesPool m_VariablesPool;
};

} // namespace

class TestAmdaFuzzing : public QObject {
    Q_OBJECT

private slots:
    /// Input data for @sa testFuzzing()
    void testFuzzing_data();
    void testFuzzing();
};

void TestAmdaFuzzing::testFuzzing_data()
{
    // ////////////// //
    // Test structure //
    // ////////////// //

    QTest::addColumn<Properties>("properties"); // Properties for random test

    // ////////// //
    // Test cases //
    // ////////// //

    ///@todo: complete
}

void TestAmdaFuzzing::testFuzzing()
{
    QFETCH(Properties, properties);

    auto &variableController = sqpApp->variableController();
    auto &timeController = sqpApp->timeController();

    // Generates random initial range (bounded to max range)
    auto maxRange = properties.value(MAX_RANGE_PROPERTY, QVariant::fromValue(INVALID_RANGE))
                        .value<SqpRange>();

    QVERIFY(maxRange != INVALID_RANGE);

    auto initialRangeStart
        = RandomGenerator::instance().generateDouble(maxRange.m_TStart, maxRange.m_TEnd);
    auto initialRangeEnd
        = RandomGenerator::instance().generateDouble(maxRange.m_TStart, maxRange.m_TEnd);
    if (initialRangeStart > initialRangeEnd) {
        std::swap(initialRangeStart, initialRangeEnd);
    }

    // Sets initial range on time controller
    SqpRange initialRange{initialRangeStart, initialRangeEnd};
    qCInfo(LOG_TestAmdaFuzzing()) << "Setting initial range to" << initialRange << "...";
    timeController.onTimeToUpdate(initialRange);

    FuzzingTest test{variableController, properties};
    test.execute();
}

int main(int argc, char *argv[])
{
    QLoggingCategory::setFilterRules(
        "*.warning=false\n"
        "*.info=false\n"
        "*.debug=false\n"
        "FuzzingOperations.info=true\n"
        "TestAmdaFuzzing.info=true\n");

    SqpApplication app{argc, argv};
    app.setAttribute(Qt::AA_Use96Dpi, true);
    TestAmdaFuzzing testObject{};
    QTEST_SET_MAIN_SOURCE_PATH
    return QTest::qExec(&testObject, argc, argv);
}

#include "TestAmdaFuzzing.moc"
