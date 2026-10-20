#include "doctest.h"

#include "BudgetModel.h"

TEST_CASE("a budget stores its category, amount, and dates") {
    BudgetModel model;
    QString error;
    const QDate start(2026, 10, 1);
    const QDate end(2026, 10, 30);

    REQUIRE(model.addBudget({"Meal plan", 12000, start, end}, &error));
    CHECK(error.isEmpty());
    REQUIRE(model.budgets().size() == 1);
    CHECK(model.budgets().first().category == QStringLiteral("Meal plan"));
    CHECK(model.budgets().first().amountCents == 12000);
    CHECK(model.budgets().first().startDate == start);
    CHECK(model.budgets().first().endDate == end);
}

TEST_CASE("negative budgets are rejected") {
    BudgetModel model;
    QString error;

    CHECK_FALSE(model.addBudget({"Food", -1, QDate(2026, 10, 1), QDate(2026, 10, 2)}, &error));
    CHECK(error.contains(QStringLiteral("negative")));
    CHECK(model.budgets().isEmpty());
}

TEST_CASE("an end date before the start date is rejected") {
    BudgetModel model;
    QString error;

    CHECK_FALSE(model.addBudget({"Food", 100, QDate(2026, 10, 2), QDate(2026, 10, 1)}, &error));
    CHECK(error.contains(QStringLiteral("before start")));
    CHECK(model.budgets().isEmpty());
}

TEST_CASE("a daily budget includes the start and end dates") {
    Budget budget{"Food", 1000, QDate(2026, 10, 1), QDate(2026, 10, 1)};
    CHECK(budget.amountPerDay() == doctest::Approx(10.0));

    budget.endDate = QDate(2026, 10, 2);
    CHECK(budget.amountPerDay() == doctest::Approx(5.0));
}

TEST_CASE("the total combines budgets across categories") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Meal plan", 12000, QDate(2026, 10, 1), QDate(2026, 10, 30)}));
    REQUIRE(model.addBudget({"Transport", 2500, QDate(2026, 10, 1), QDate(2026, 10, 30)}));

    CHECK(model.totalCents() == 14500);
    CHECK(model.budgets().size() == 2);
}

TEST_CASE("an empty category or invalid date is rejected") {
    BudgetModel model;
    CHECK_FALSE(model.addBudget({"  ", 100, QDate(2026, 10, 1), QDate(2026, 10, 2)}));
    CHECK_FALSE(model.addBudget({"Food", 100, QDate(), QDate(2026, 10, 2)}));
    CHECK(model.budgets().isEmpty());
}

TEST_CASE("zero is a valid budget amount") {
    BudgetModel model;
    CHECK(model.addBudget({"Meal plan", 0, QDate(2026, 10, 1), QDate(2026, 10, 1)}));
    CHECK(model.totalCents() == 0);
}
