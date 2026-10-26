// Author: Xiaochao Zhou
#include "doctest.h"

#include "BudgetModel.h"

#include <cmath>
#include <limits>

namespace {
BudgetModel modelWithBudget() {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    return model;
}
}

TEST_CASE("fractional cents below half a cent are preserved") {
    BudgetModel model = modelWithBudget();
    QString error = QStringLiteral("Previous error");

    // $12.344 remains $12.344.
    REQUIRE(model.addExpense(1234.4, &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234.4"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8765.6"));
}

TEST_CASE("half-cent expense amounts are preserved") {
    BudgetModel model = modelWithBudget();

    // $12.345 remains $12.345.
    REQUIRE(model.addExpense(1234.5));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8765.5"));
}

TEST_CASE("fractional cents above half a cent are preserved") {
    BudgetModel model = modelWithBudget();

    // $12.346 remains $12.346.
    REQUIRE(model.addExpense(1234.6));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234.6"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8765.4"));
}

TEST_CASE("the representable amount immediately below half a cent is preserved") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(std::nextafter(0.5, 0.0)));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.49999999999999994"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.50000000000000006"));
}

TEST_CASE("a floating-point zero expense remains valid") {
    BudgetModel model = modelWithBudget();
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense(0.0, &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents() == 0);
    CHECK(model.remainingBudgetCents() == 10000);
}

TEST_CASE("multiple fractional expenses accumulate without rounding") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(1250.5));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8749.5"));

    REQUIRE(model.addExpense(2375.5));
    CHECK(model.totalSpendingCents() == 3626);
    CHECK(model.remainingBudgetCents() == 6374);
    CHECK(model.totalCents() == 10000);
}

TEST_CASE("a floating-point expense just below the integer limit is accepted") {
    BudgetModel model;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    REQUIRE(model.addBudget({"Food", maximum, QDate(2026, 10, 1), QDate(2026, 10, 31)}));

    // The largest double below 2^63 equals 2^63 - 1024 cents.
    REQUIRE(model.addExpense(std::nextafter(std::ldexp(1.0, 63), 0.0)));
    CHECK(model.totalSpendingCents() == maximum - 1023);
    CHECK(model.remainingBudgetCents() == 1023);
}

TEST_CASE("separate decimal expenses sum exactly without binary floating drift") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(0.1));
    REQUIRE(model.addExpense(0.2));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.3"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.7"));
}

TEST_CASE("a tiny positive expense is not truncated to zero") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(1e-20));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.00000000000000000001"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.99999999999999999999"));
}

TEST_CASE("fractional expenses carry into a whole cent exactly") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(0.75));
    REQUIRE(model.addExpense(0.25));
    CHECK(model.totalSpendingCents() == 1);
    CHECK(model.remainingBudgetCents() == 9999);
}

TEST_CASE("overspending preserves a negative fractional remaining balance") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(10000.25));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("10000.25"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("-0.25"));
}

TEST_CASE("a float expense is interpreted without promoting its binary tail") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(0.1f));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.1"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.9"));
}

TEST_CASE("a long double fractional expense is preserved") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(0.125L));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.125"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.875"));
}

TEST_CASE("an externally computed floating amount is preserved rather than rounded") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(0.1 + 0.2));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.30000000000000004"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.69999999999999996"));
}

TEST_CASE("an explicit decimal amount keeps precision beyond native floating types") {
    BudgetModel model = modelWithBudget();
    const auto amount = DecimalCents::fromDecimal(QStringLiteral("0.12345678901234567890123456789"));
    REQUIRE(amount.has_value());

    REQUIRE(model.addExpense(*amount));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("0.12345678901234567890123456789"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("9999.87654321098765432109876543211"));
}

TEST_CASE("fractional expenses can reach the exact maximum without being rounded") {
    BudgetModel model;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    REQUIRE(model.addBudget({"Food", maximum, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(maximum - 1));

    REQUIRE(model.addExpense(0.5));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("9223372036854775806.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("0.5"));
    REQUIRE(model.addExpense(0.5));
    CHECK(model.totalSpendingCents() == maximum);
    CHECK(model.remainingBudgetCents() == 0);
}
