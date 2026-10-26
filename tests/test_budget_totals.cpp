// Troy Capela
#include "doctest.h"

#include "BudgetModel.h"

TEST_CASE("total budget combines the amounts of all budgets") {
    BudgetModel model;
    CHECK(model.totalCents() == 0);

    const QDate startDate(2026, 10, 1);
    const QDate endDate(2026, 10, 31);
    REQUIRE(model.addBudget({"Meal plan", 25000, startDate, endDate}));
    REQUIRE(model.addBudget({"Groceries", 12550, startDate, endDate}));
    REQUIRE(model.addBudget({"Transport", 8000, startDate, endDate}));

    CHECK(model.totalCents() == 45550);
}
