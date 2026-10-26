// Author: Huy Tran
// Verify the rules a budget must meet before the model saves it.
#include "doctest.h"

#include "BudgetModel.h"

#include <limits>

namespace {
const QDate kStart(2026, 10, 1);
const QDate kEnd(2026, 10, 31);

BudgetModel modelWithExistingBudget() {
    BudgetModel model;
    REQUIRE(model.addBudget({"Groceries", 20000, kStart, kEnd}));
    return model;
}

void checkRejectedBudgetChangesNothing(const BudgetModel &model, const QString &error) {
    CHECK_FALSE(error.isEmpty());
    CHECK(model.budgets().size() == 1);
    CHECK(model.totalCents() == 20000);
    CHECK(model.remainingBudgetCents() == 20000);
}
}

TEST_CASE("a valid budget is saved with its details") {
    BudgetModel model;
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addBudget({"Meal plan", 30000, kStart, kEnd}, &error));
    CHECK(error.isEmpty());
    REQUIRE(model.budgets().size() == 1);
    const Budget &saved = model.budgets().at(0);
    CHECK(saved.category == QStringLiteral("Meal plan"));
    CHECK(saved.amountCents == 30000);
    CHECK(saved.startDate == kStart);
    CHECK(saved.endDate == kEnd);
}

TEST_CASE("a negative budget amount is rejected") {
    BudgetModel model = modelWithExistingBudget();
    QString error;

    CHECK_FALSE(model.addBudget({"Transport", -1, kStart, kEnd}, &error));
    checkRejectedBudgetChangesNothing(model, error);
}

TEST_CASE("a zero budget amount is allowed") {
    BudgetModel model = modelWithExistingBudget();

    REQUIRE(model.addBudget({"Other", 0, kStart, kEnd}));
    CHECK(model.budgets().size() == 2);
    CHECK(model.totalCents() == 20000);
}

TEST_CASE("an end date before the start date is rejected") {
    BudgetModel model = modelWithExistingBudget();
    QString error;

    CHECK_FALSE(model.addBudget({"Transport", 5000, kEnd, kStart}, &error));
    checkRejectedBudgetChangesNothing(model, error);
}

TEST_CASE("an end date one day before the start date is rejected") {
    BudgetModel model = modelWithExistingBudget();
    QString error;

    CHECK_FALSE(model.addBudget({"Transport", 5000, QDate(2026, 10, 2), QDate(2026, 10, 1)}, &error));
    checkRejectedBudgetChangesNothing(model, error);
}

TEST_CASE("a missing start or end date is rejected") {
    BudgetModel model = modelWithExistingBudget();
    QString error;

    CHECK_FALSE(model.addBudget({"Transport", 5000, QDate(), kEnd}, &error));
    checkRejectedBudgetChangesNothing(model, error);

    error.clear();
    CHECK_FALSE(model.addBudget({"Transport", 5000, kStart, QDate()}, &error));
    checkRejectedBudgetChangesNothing(model, error);
}

TEST_CASE("an empty or blank category is rejected") {
    BudgetModel model = modelWithExistingBudget();
    QString error;

    CHECK_FALSE(model.addBudget({QString(), 5000, kStart, kEnd}, &error));
    checkRejectedBudgetChangesNothing(model, error);

    error.clear();
    CHECK_FALSE(model.addBudget({QStringLiteral("   "), 5000, kStart, kEnd}, &error));
    checkRejectedBudgetChangesNothing(model, error);
}

TEST_CASE("surrounding spaces are trimmed from a category name") {
    BudgetModel model;

    REQUIRE(model.addBudget({QStringLiteral("  Dining out  "), 5000, kStart, kEnd}));
    CHECK(model.budgets().at(0).category == QStringLiteral("Dining out"));
}

TEST_CASE("a budget that would overflow the combined total is rejected") {
    BudgetModel model = modelWithExistingBudget();
    QString error;
    const qint64 tooLarge = std::numeric_limits<qint64>::max() - 20000 + 1;

    CHECK_FALSE(model.addBudget({"Other", tooLarge, kStart, kEnd}, &error));
    checkRejectedBudgetChangesNothing(model, error);
}

TEST_CASE("a budget that exactly reaches the largest supported total is accepted") {
    BudgetModel model = modelWithExistingBudget();
    const qint64 largest = std::numeric_limits<qint64>::max() - 20000;

    REQUIRE(model.addBudget({"Other", largest, kStart, kEnd}));
    CHECK(model.totalCents() == std::numeric_limits<qint64>::max());
}

TEST_CASE("a successful budget clears an earlier error message") {
    BudgetModel model;
    QString error;

    CHECK_FALSE(model.addBudget({"Transport", -500, kStart, kEnd}, &error));
    CHECK_FALSE(error.isEmpty());

    REQUIRE(model.addBudget({"Transport", 500, kStart, kEnd}, &error));
    CHECK(error.isEmpty());
}
