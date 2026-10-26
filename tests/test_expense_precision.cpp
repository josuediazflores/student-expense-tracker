// Author: Xiaochao Zhou
// Each case isolates one precision rule using literal dollar text.
#include "doctest.h"

#include "BudgetModel.h"

#include <limits>

namespace {
BudgetModel modelWithBudget(qint64 amountCents = 10000) {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", amountCents, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    return model;
}
}

TEST_CASE("a decimal dollar string converts exactly to whole cents") {
    BudgetModel model = modelWithBudget();
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense(QStringLiteral("12.99"), &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents() == 1299);
    CHECK(model.remainingBudgetCents() == 8701);
}

TEST_CASE("a third decimal place below half a cent is preserved") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("12.344"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234.4"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8765.6"));
}

TEST_CASE("a third decimal place at half a cent is preserved") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("12.345"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8765.5"));
}

TEST_CASE("the dollar string 0.125 becomes exactly 12.5 cents") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.125"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("12.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9987.5"));
}

TEST_CASE("a third decimal place above half a cent is preserved") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("12.346"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234.6"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8765.4"));
}

TEST_CASE("an amount below half a cent remains positive and exact") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.0049"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.49"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.51"));
}

TEST_CASE("an amount of exactly half a cent is preserved") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.005"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.5"));
}

TEST_CASE("an amount above half a cent remains fractional and exact") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.0051"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.51"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.49"));
}

TEST_CASE("a zero dollar string leaves fractional balances unchanged") {
    BudgetModel model = modelWithBudget();
    REQUIRE(model.addExpense("12.5025"));
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense("0.000", &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8749.75"));
}

TEST_CASE("a negative zero dollar string is accepted as zero") {
    BudgetModel model = modelWithBudget();
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense("-0.000", &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0"));
    CHECK_FALSE(model.totalSpendingCents().isNegative());
    CHECK(model.remainingBudgetCents() == 10000);
}

TEST_CASE("multiple fractional dollar strings accumulate without rounding") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("12.505"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8749.5"));

    REQUIRE(model.addExpense("23.755"));
    CHECK(model.totalSpendingCents() == 3626);
    CHECK(model.remainingBudgetCents() == 6374);
    CHECK(model.totalCents() == 10000);
}

TEST_CASE("separate decimal dollar strings add without binary floating drift") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.001"));
    REQUIRE(model.addExpense("0.002"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.3"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.7"));
}

TEST_CASE("a tiny positive dollar string is not truncated to zero") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.000000000001"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.0000000001"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.9999999999"));
}

TEST_CASE("fractional dollar strings carry into a whole cent exactly") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.0075"));
    REQUIRE(model.addExpense("0.0025"));
    CHECK(model.totalSpendingCents() == 1);
    CHECK(model.remainingBudgetCents() == 9999);
}

TEST_CASE("overspending preserves a negative fractional remaining balance") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("100.0025"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("10000.25"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("-0.25"));
}

TEST_CASE("surrounding whitespace is trimmed from a dollar string") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(QStringLiteral(" 12.99 ")));
    CHECK(model.totalSpendingCents() == 1299);
    CHECK(model.remainingBudgetCents() == 8701);
}

TEST_CASE("leading and trailing zeros preserve the dollar string value") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("00012.3400"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234"));
    CHECK(model.remainingBudgetCents() == 8766);
}

TEST_CASE("a dollar string retains precision beyond native floating types") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense("0.0012345678901234567890123456789"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.12345678901234567890123456789"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.87654321098765432109876543211"));
}

TEST_CASE("a dollar string preserves cents beyond the exact double integer range") {
    const qint64 budgetCents = 9007199254740993LL;
    BudgetModel model = modelWithBudget(budgetCents);

    REQUIRE(model.addExpense("90071992547409.93"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("9007199254740993"));
    CHECK(model.remainingBudgetCents() == 0);
}

TEST_CASE("the exact maximum supported dollar string is accepted") {
    const qint64 maximum = std::numeric_limits<qint64>::max();
    BudgetModel model = modelWithBudget(maximum);

    REQUIRE(model.addExpense("92233720368547758.07"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("9223372036854775807"));
    CHECK(model.remainingBudgetCents() == 0);
}

TEST_CASE("fractional dollar strings can reach the exact maximum spending") {
    const qint64 maximum = std::numeric_limits<qint64>::max();
    BudgetModel model = modelWithBudget(maximum);

    REQUIRE(model.addExpense("92233720368547758.069"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("9223372036854775806.9"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("0.1"));

    REQUIRE(model.addExpense("0.001"));
    CHECK(model.totalSpendingCents() == maximum);
    CHECK(model.remainingBudgetCents() == 0);
    CHECK(model.totalCents() == maximum);
}
