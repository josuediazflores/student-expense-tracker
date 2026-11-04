// Author: Huy Tran
// Verify the rules for changing the amount of a budget that already exists.
#include "doctest.h"

#include "BudgetModel.h"

#include <limits>

namespace {
const QDate kStart(2026, 10, 1);
const QDate kEnd(2026, 10, 31);
const qint64 kMax = std::numeric_limits<qint64>::max();

// Two budgets and one fractional expense, so an accidental reset, reorder or
// rounding after a rejected update would show up in the checks below.
BudgetModel modelWithTwoBudgets() {
    BudgetModel model;
    REQUIRE(model.addBudget({"Groceries", 20000, kStart, kEnd}));
    REQUIRE(model.addBudget({"Transport", 5000, QDate(2026, 11, 1), QDate(2026, 11, 30)}));
    REQUIRE(model.addExpense("12.5025"));
    return model;
}

void checkNothingChanged(const BudgetModel &model) {
    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).category == QStringLiteral("Groceries"));
    CHECK(model.budgets().at(0).amountCents == 20000);
    CHECK(model.budgets().at(0).startDate == kStart);
    CHECK(model.budgets().at(0).endDate == kEnd);
    CHECK(model.budgets().at(1).category == QStringLiteral("Transport"));
    CHECK(model.budgets().at(1).amountCents == 5000);
    CHECK(model.budgets().at(1).startDate == QDate(2026, 11, 1));
    CHECK(model.budgets().at(1).endDate == QDate(2026, 11, 30));
    CHECK(model.totalCents() == 25000);
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("23749.75"));
    CHECK(model.expenses().size() == 1);
}

void checkRejected(BudgetModel &model, qsizetype index, qint64 amountCents, const QString &message) {
    QString error;
    CHECK_FALSE(model.updateBudgetAmount(index, amountCents, &error));
    CHECK(error == message);
    checkNothingChanged(model);
}
}

TEST_CASE("updating a budget changes only that budget's amount") {
    BudgetModel model = modelWithTwoBudgets();

    REQUIRE(model.updateBudgetAmount(1, 7500));
    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).category == QStringLiteral("Groceries"));
    CHECK(model.budgets().at(0).amountCents == 20000);
    CHECK(model.budgets().at(0).startDate == kStart);
    CHECK(model.budgets().at(0).endDate == kEnd);
    CHECK(model.budgets().at(1).category == QStringLiteral("Transport"));
    CHECK(model.budgets().at(1).amountCents == 7500);
    CHECK(model.budgets().at(1).startDate == QDate(2026, 11, 1));
    CHECK(model.budgets().at(1).endDate == QDate(2026, 11, 30));
}

TEST_CASE("updating the first budget keeps budget order and count") {
    BudgetModel model = modelWithTwoBudgets();

    REQUIRE(model.updateBudgetAmount(0, 1));
    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).category == QStringLiteral("Groceries"));
    CHECK(model.budgets().at(0).amountCents == 1);
    CHECK(model.budgets().at(1).category == QStringLiteral("Transport"));
    CHECK(model.budgets().at(1).amountCents == 5000);
}

TEST_CASE("updating a budget recalculates the total and remaining budget") {
    BudgetModel model = modelWithTwoBudgets();

    REQUIRE(model.updateBudgetAmount(0, 30000));
    CHECK(model.totalCents() == 35000);
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("33749.75"));
    // Spending and the expense list are not touched by a budget update.
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
    CHECK(model.expenses().size() == 1);
}

TEST_CASE("lowering a budget below spending makes the remaining budget negative") {
    BudgetModel model = modelWithTwoBudgets();

    REQUIRE(model.updateBudgetAmount(0, 0));
    REQUIRE(model.updateBudgetAmount(1, 1000));
    CHECK(model.totalCents() == 1000);
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("-250.25"));
}

TEST_CASE("a budget can be updated to zero") {
    BudgetModel model = modelWithTwoBudgets();

    REQUIRE(model.updateBudgetAmount(0, 0));
    CHECK(model.budgets().at(0).amountCents == 0);
    CHECK(model.totalCents() == 5000);
}

TEST_CASE("a successful update clears a previous error") {
    BudgetModel model = modelWithTwoBudgets();
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.updateBudgetAmount(0, 100, &error));
    CHECK(error.isEmpty());
}

TEST_CASE("an update works without an error out-parameter") {
    BudgetModel model = modelWithTwoBudgets();

    CHECK(model.updateBudgetAmount(0, 100));
    CHECK_FALSE(model.updateBudgetAmount(9, 100));
    CHECK_FALSE(model.updateBudgetAmount(0, -1));
}

TEST_CASE("a negative index is rejected") {
    BudgetModel model = modelWithTwoBudgets();
    checkRejected(model, -1, 100, QStringLiteral("Choose a budget to update."));
}

TEST_CASE("an index equal to the budget count is rejected") {
    BudgetModel model = modelWithTwoBudgets();
    checkRejected(model, 2, 100, QStringLiteral("Choose a budget to update."));
}

TEST_CASE("an index far past the end is rejected") {
    BudgetModel model = modelWithTwoBudgets();
    checkRejected(model, 1000, 100, QStringLiteral("Choose a budget to update."));
}

TEST_CASE("updating an empty model is rejected") {
    BudgetModel model;
    QString error;

    CHECK_FALSE(model.updateBudgetAmount(0, 100, &error));
    CHECK(error == QStringLiteral("Choose a budget to update."));
    CHECK(model.budgets().isEmpty());
    CHECK(model.totalCents() == 0);
}

TEST_CASE("a bad index is reported before a negative amount") {
    BudgetModel model = modelWithTwoBudgets();
    checkRejected(model, 5, -1, QStringLiteral("Choose a budget to update."));
}

TEST_CASE("a negative amount is rejected") {
    BudgetModel model = modelWithTwoBudgets();
    checkRejected(model, 0, -1, QStringLiteral("Budget amount cannot be negative."));
}

TEST_CASE("the most negative amount is rejected as negative, not as too large") {
    BudgetModel model = modelWithTwoBudgets();
    checkRejected(model, 1, std::numeric_limits<qint64>::min(),
                  QStringLiteral("Budget amount cannot be negative."));
}

TEST_CASE("an update that would push the total budget past the maximum is rejected") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Big", kMax - 100, kStart, kEnd}));
    REQUIRE(model.addBudget({"Small", 100, kStart, kEnd}));
    QString error;

    CHECK_FALSE(model.updateBudgetAmount(1, 101, &error));
    CHECK(error == QStringLiteral("Total budget is too large."));
    CHECK(model.budgets().at(0).amountCents == kMax - 100);
    CHECK(model.budgets().at(1).amountCents == 100);
    CHECK(model.totalCents() == kMax);
}

TEST_CASE("an update may keep the total exactly at the maximum") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Big", kMax - 100, kStart, kEnd}));
    REQUIRE(model.addBudget({"Small", 100, kStart, kEnd}));
    QString error = QStringLiteral("Previous error");

    // The other budget is 100, so this budget may be at most kMax - 100.
    REQUIRE(model.updateBudgetAmount(0, kMax - 100, &error));
    CHECK(error.isEmpty());
    CHECK(model.totalCents() == kMax);
}

TEST_CASE("raising a budget by one cent past the maximum total is rejected") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Big", kMax - 100, kStart, kEnd}));
    REQUIRE(model.addBudget({"Small", 100, kStart, kEnd}));
    QString error;

    CHECK_FALSE(model.updateBudgetAmount(0, kMax - 99, &error));
    CHECK(error == QStringLiteral("Total budget is too large."));
    CHECK(model.budgets().at(0).amountCents == kMax - 100);
    CHECK(model.totalCents() == kMax);
}

TEST_CASE("lowering a budget in a model at the maximum total is accepted") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Big", kMax - 100, kStart, kEnd}));
    REQUIRE(model.addBudget({"Small", 100, kStart, kEnd}));

    REQUIRE(model.updateBudgetAmount(1, 0));
    CHECK(model.totalCents() == kMax - 100);
}

TEST_CASE("an update to the same amount is accepted and changes nothing") {
    BudgetModel model = modelWithTwoBudgets();

    REQUIRE(model.updateBudgetAmount(0, 20000));
    checkNothingChanged(model);
}
