// Removing budgets and expenses in the Model.
// removeBudget / removeExpense take the Model's own index, keep the order of what is left,
// and change nothing when the index is out of range.
#include "doctest.h"

#include "BudgetModel.h"

namespace {
const QString kChooseBudget = QStringLiteral("Choose a budget to delete.");
const QString kChooseExpense = QStringLiteral("Choose an expense to delete.");
const QDate kNov1(2026, 11, 1);
const QDate kDec1(2026, 12, 1);

// Food $100.00, Fun $50.00, Books $20.00, as cents.
void addThreeBudgets(BudgetModel &model) {
    REQUIRE(model.addBudget({"Food", 10000, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Fun", 5000, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Books", 2000, kNov1, kDec1}));
}

// Entry order: 10 Food Nov 1, 12.345 (uncategorized) Nov 2, 0.1 Fun Nov 3.
void addThreeExpenses(BudgetModel &model) {
    REQUIRE(model.addExpense(QStringLiteral("10"), QDate(2026, 11, 1), QStringLiteral("Food")));
    REQUIRE(model.addExpense(QStringLiteral("12.345"), QDate(2026, 11, 2)));
    REQUIRE(model.addExpense(QStringLiteral("0.1"), QDate(2026, 11, 3), QStringLiteral("Fun")));
}
}

// ---- Budgets -------------------------------------------------------------------------

TEST_CASE("removing a middle budget keeps the order of the others") {
    BudgetModel model;
    addThreeBudgets(model);

    QString error = QStringLiteral("old");
    CHECK(model.removeBudget(1, &error));

    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).category == QStringLiteral("Food"));
    CHECK(model.budgets().at(1).category == QStringLiteral("Books"));
    CHECK(model.budgets().at(1).amountCents == 2000);
    CHECK(model.totalCents() == 12000);
    CHECK(error.isEmpty());
}

TEST_CASE("removing the first and the last budget works") {
    BudgetModel model;
    addThreeBudgets(model);

    CHECK(model.removeBudget(2));
    CHECK(model.removeBudget(0));

    REQUIRE(model.budgets().size() == 1);
    CHECK(model.budgets().at(0).category == QStringLiteral("Fun"));
    CHECK(model.totalCents() == 5000);
}

TEST_CASE("removing the only budget leaves none and a zero total") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, kNov1, kDec1}));

    CHECK(model.removeBudget(0));

    CHECK(model.budgets().isEmpty());
    CHECK(model.totalCents() == 0);
}

TEST_CASE("an invalid budget index is rejected with the exact message and changes nothing") {
    BudgetModel model;
    addThreeBudgets(model);
    REQUIRE(model.addExpense(QStringLiteral("5"), kNov1));

    for (qsizetype index : {qsizetype(-1), qsizetype(3), qsizetype(100)}) {
        QString error;
        CHECK_FALSE(model.removeBudget(index, &error));
        CHECK(error == kChooseBudget);
    }

    REQUIRE(model.budgets().size() == 3);
    CHECK(model.budgets().at(0).category == QStringLiteral("Food"));
    CHECK(model.budgets().at(2).category == QStringLiteral("Books"));
    CHECK(model.totalCents() == 17000);
    CHECK(model.expenses().size() == 1);
}

TEST_CASE("removing a budget from an empty model is rejected") {
    BudgetModel model;
    QString error;

    CHECK_FALSE(model.removeBudget(0, &error));

    CHECK(error == kChooseBudget);
}

TEST_CASE("removing a budget keeps every expense with its category, and the remaining budget follows") {
    BudgetModel model;
    addThreeBudgets(model);  // 17,000 cents of budget
    addThreeExpenses(model);  // 10 + 12.345 + 0.1 dollars = 2244.5 cents
    REQUIRE(model.remainingBudgetCents().toString() == QStringLiteral("14755.5"));

    REQUIRE(model.removeBudget(0));  // Food, 10,000 cents

    REQUIRE(model.expenses().size() == 3);
    CHECK(model.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(model.expenses().at(1).category.isEmpty());
    CHECK(model.expenses().at(2).category == QStringLiteral("Fun"));
    CHECK(model.expenses().at(1).amountCents.toString() == QStringLiteral("1234.5"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("2244.5"));
    CHECK(model.totalCents() == 7000);
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("4755.5"));
}

// ---- Expenses ------------------------------------------------------------------------

TEST_CASE("removing a middle expense keeps the order of the others") {
    BudgetModel model;
    addThreeExpenses(model);

    QString error = QStringLiteral("old");
    CHECK(model.removeExpense(1, &error));

    REQUIRE(model.expenses().size() == 2);
    CHECK(model.expenses().at(0).amountCents.toString() == QStringLiteral("1000"));
    CHECK(model.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(model.expenses().at(0).date == QDate(2026, 11, 1));
    CHECK(model.expenses().at(1).amountCents.toString() == QStringLiteral("10"));
    CHECK(model.expenses().at(1).category == QStringLiteral("Fun"));
    CHECK(model.expenses().at(1).date == QDate(2026, 11, 3));
    CHECK(error.isEmpty());
}

TEST_CASE("total spending is exact after removing a fractional expense") {
    BudgetModel model;
    addThreeExpenses(model);  // 1000 + 1234.5 + 10 cents
    REQUIRE(model.totalSpendingCents().toString() == QStringLiteral("2244.5"));

    CHECK(model.removeExpense(1));

    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1010"));
}

TEST_CASE("total spending is exact after removing a small fraction from a fractional total") {
    BudgetModel model;
    addThreeExpenses(model);

    CHECK(model.removeExpense(2));  // 0.1 dollars, 10 cents

    CHECK(model.totalSpendingCents().toString() == QStringLiteral("2234.5"));
}

TEST_CASE("removing every expense returns total spending to zero") {
    BudgetModel model;
    addThreeExpenses(model);

    CHECK(model.removeExpense(0));
    CHECK(model.removeExpense(0));
    CHECK(model.removeExpense(0));

    CHECK(model.expenses().isEmpty());
    CHECK(model.totalSpendingCents() == 0);
}

TEST_CASE("remaining budget grows by exactly the removed expense") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, kNov1, kDec1}));
    addThreeExpenses(model);

    CHECK(model.removeExpense(1));

    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8990"));
}

TEST_CASE("an invalid expense index is rejected with the exact message and changes nothing") {
    BudgetModel model;
    addThreeExpenses(model);
    REQUIRE(model.addBudget({"Food", 10000, kNov1, kDec1}));

    for (qsizetype index : {qsizetype(-1), qsizetype(3), qsizetype(100)}) {
        QString error;
        CHECK_FALSE(model.removeExpense(index, &error));
        CHECK(error == kChooseExpense);
    }

    CHECK(model.expenses().size() == 3);
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("2244.5"));
    CHECK(model.budgets().size() == 1);
}

TEST_CASE("removing an expense from an empty model is rejected") {
    BudgetModel model;
    QString error;

    CHECK_FALSE(model.removeExpense(0, &error));

    CHECK(error == kChooseExpense);
}

TEST_CASE("removal works without an error pointer") {
    BudgetModel model;
    addThreeBudgets(model);
    addThreeExpenses(model);

    CHECK(model.removeBudget(0));
    CHECK(model.removeExpense(0));
    CHECK_FALSE(model.removeBudget(9));
    CHECK_FALSE(model.removeExpense(9));
}
