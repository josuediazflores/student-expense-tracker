// Author: Xiaochao Zhou
// Verify that multiple expenses update total spending and remaining budget.
#include "doctest.h"

#include "BudgetModel.h"

TEST_CASE("adding multiple expenses updates total spending and remaining budget") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    CHECK(model.totalSpendingCents() == 0);
    CHECK(model.remainingBudgetCents() == 10000);

    // Enter dollars as text; expected spending and remaining balances are in cents.
    REQUIRE(model.addExpense("12.50"));
    CHECK(model.totalSpendingCents() == 1250);
    CHECK(model.remainingBudgetCents() == 8750);

    REQUIRE(model.addExpense("23.75"));
    CHECK(model.totalSpendingCents() == 3625);
    CHECK(model.remainingBudgetCents() == 6375);

    REQUIRE(model.addExpense("5.99"));
    CHECK(model.totalSpendingCents() == 4224);
    CHECK(model.remainingBudgetCents() == 5776);
    CHECK(model.totalCents() == 10000);
}
