// Author: Huy Tran
// Verify the daily amount shown for each budget, counting both the start and end dates.
#include "doctest.h"

#include "BudgetModel.h"

TEST_CASE("a monthly budget is spread evenly across every day of the month") {
    // $310.00 from Oct 1 to Oct 31 covers 31 days.
    const Budget budget{"Groceries", 31000, QDate(2026, 10, 1), QDate(2026, 10, 31)};
    CHECK(budget.amountPerDay() == doctest::Approx(10.00));
}

TEST_CASE("a budget that starts and ends on the same day keeps its full amount") {
    const Budget budget{"Dining out", 2550, QDate(2026, 10, 26), QDate(2026, 10, 26)};
    CHECK(budget.amountPerDay() == doctest::Approx(25.50));
}

TEST_CASE("a two-day budget counts both the start and end dates") {
    // Without counting both dates, this would wrongly report $50.00 per day.
    const Budget budget{"Transport", 5000, QDate(2026, 10, 26), QDate(2026, 10, 27)};
    CHECK(budget.amountPerDay() == doctest::Approx(25.00));
}

TEST_CASE("a budget spanning a month boundary counts days in both months") {
    // Oct 25 to Nov 5 is 7 days in October and 5 in November.
    const Budget budget{"Meal plan", 12000, QDate(2026, 10, 25), QDate(2026, 11, 5)};
    CHECK(budget.amountPerDay() == doctest::Approx(10.00));
}

TEST_CASE("a February budget in a leap year includes February 29") {
    const Budget budget{"Groceries", 29000, QDate(2028, 2, 1), QDate(2028, 2, 29)};
    CHECK(budget.amountPerDay() == doctest::Approx(10.00));
}

TEST_CASE("an SCU quarter budget gives a fractional daily amount") {
    // Fall quarter: Sep 21 to Dec 12 is 83 days, so $500.00 is about $6.02 per day.
    const Budget budget{"Meal plan", 50000, QDate(2026, 9, 21), QDate(2026, 12, 12)};
    CHECK(budget.amountPerDay() == doctest::Approx(500.0 / 83.0));
}

TEST_CASE("a zero budget has a zero daily amount") {
    const Budget budget{"Other", 0, QDate(2026, 10, 1), QDate(2026, 10, 31)};
    CHECK(budget.amountPerDay() == doctest::Approx(0.0));
}

TEST_CASE("each saved budget keeps its own daily amount") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Groceries", 31000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addBudget({"Transport", 7000, QDate(2026, 10, 1), QDate(2026, 10, 7)}));

    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).amountPerDay() == doctest::Approx(10.00));
    CHECK(model.budgets().at(1).amountPerDay() == doctest::Approx(10.00));
}
