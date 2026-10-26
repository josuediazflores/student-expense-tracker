// Author: Xiaochao Zhou
#include "doctest.h"

#include "BudgetModel.h"

TEST_CASE("a non-integer expense amount in cents is rejected") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(1250));
    QString error;

    // 2375.5 cents is half a cent more than $23.75 and must not be truncated.
    CHECK_FALSE(model.addExpense(2375.5, &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents() == 1250);
    CHECK(model.remainingBudgetCents() == 8750);
    CHECK(model.totalCents() == 10000);
}

TEST_CASE("a null expense amount is rejected") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(1250));
    QString error;

    CHECK_FALSE(model.addExpense(nullptr, &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents() == 1250);
    CHECK(model.remainingBudgetCents() == 8750);
    CHECK(model.totalCents() == 10000);
}

TEST_CASE("a zero expense amount remains valid") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(1250));
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense(0, &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents() == 1250);
    CHECK(model.remainingBudgetCents() == 8750);
    CHECK(model.totalCents() == 10000);
}
