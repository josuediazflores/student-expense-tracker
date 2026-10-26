#include "doctest.h"

#include "BudgetModel.h"

TEST_CASE("a valid expense succeeds after an invalid entry") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 2000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    QString error;

    CHECK_FALSE(model.addExpense("abc", &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents() == 0);

    REQUIRE(model.addExpense("5.00", &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents() == 500);
    CHECK(model.remainingBudgetCents() == 1500);
    CHECK(model.totalCents() == 2000);
}
