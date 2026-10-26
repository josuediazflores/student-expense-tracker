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

TEST_CASE("fractional cents below half a cent round down") {
    BudgetModel model = modelWithBudget();
    QString error = QStringLiteral("Previous error");

    // $12.344 becomes $12.34.
    REQUIRE(model.addExpense(1234.4, &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents() == 1234);
    CHECK(model.remainingBudgetCents() == 8766);
}

TEST_CASE("half-cent expense amounts round up") {
    BudgetModel model = modelWithBudget();

    // $12.345 becomes $12.35.
    REQUIRE(model.addExpense(1234.5));
    CHECK(model.totalSpendingCents() == 1235);
    CHECK(model.remainingBudgetCents() == 8765);
}

TEST_CASE("fractional cents above half a cent round up") {
    BudgetModel model = modelWithBudget();

    // $12.346 becomes $12.35.
    REQUIRE(model.addExpense(1234.6));
    CHECK(model.totalSpendingCents() == 1235);
    CHECK(model.remainingBudgetCents() == 8765);
}

TEST_CASE("the representable amount immediately below half a cent rounds to zero") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(std::nextafter(0.5, 0.0)));
    CHECK(model.totalSpendingCents() == 0);
    CHECK(model.remainingBudgetCents() == 10000);
}

TEST_CASE("a floating-point zero expense remains valid") {
    BudgetModel model = modelWithBudget();
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense(0.0, &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents() == 0);
    CHECK(model.remainingBudgetCents() == 10000);
}

TEST_CASE("each fractional expense is rounded before updating cumulative totals") {
    BudgetModel model = modelWithBudget();

    REQUIRE(model.addExpense(1250.5));
    CHECK(model.totalSpendingCents() == 1251);
    CHECK(model.remainingBudgetCents() == 8749);

    REQUIRE(model.addExpense(2375.5));
    CHECK(model.totalSpendingCents() == 3627);
    CHECK(model.remainingBudgetCents() == 6373);
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
