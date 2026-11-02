// The expense category. The Model records a
// trimmed category with an expense, the old overloads record none, and the Controller shows
// an empty category as "Uncategorized".
#include "doctest.h"

#include "BudgetController.h"
#include "BudgetModel.h"
#include "persistence_test_support.h"

using persistence_test::FakeDisplay;

namespace {
const QDate kMar2(2026, 3, 2);

void checkNothingRecorded(const BudgetModel &model) {
    CHECK(model.expenses().isEmpty());
    CHECK(model.totalSpendingCents() == 0);
}
}

// ---- Model ---------------------------------------------------------------------------

TEST_CASE("the four-argument addExpense stores the amount, date and a trimmed category") {
    BudgetModel model;
    QString error = QStringLiteral("Previous error");

    CHECK(model.addExpense(QStringLiteral("12.345"), kMar2, QStringLiteral("  Food "), &error));

    CHECK(error.isEmpty());
    REQUIRE(model.expenses().size() == 1);
    const Expense &saved = model.expenses().at(0);
    CHECK(saved.category == QStringLiteral("Food"));
    CHECK(saved.date == kMar2);
    CHECK(saved.amountCents.toString() == QStringLiteral("1234.5"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1234.5"));
}

TEST_CASE("an empty or blank category is stored as empty, meaning uncategorized") {
    BudgetModel model;

    CHECK(model.addExpense(QStringLiteral("1"), kMar2, QString()));
    CHECK(model.addExpense(QStringLiteral("2"), kMar2, QStringLiteral("   ")));

    REQUIRE(model.expenses().size() == 2);
    CHECK(model.expenses().at(0).category.isEmpty());
    CHECK(model.expenses().at(1).category.isEmpty());
}

TEST_CASE("the older addExpense overloads store an empty category") {
    BudgetModel model;

    CHECK(model.addExpense(QStringLiteral("1")));
    CHECK(model.addExpense("2"));
    CHECK(model.addExpense(QStringLiteral("3"), kMar2));
    CHECK(model.addExpense("4", kMar2));

    REQUIRE(model.expenses().size() == 4);
    for (const Expense &expense : model.expenses()) CHECK(expense.category.isEmpty());
}

TEST_CASE("categories are kept per expense, in entry order, and do not change the totals") {
    BudgetModel model;
    REQUIRE(model.addExpense(QStringLiteral("1.5"), kMar2, QStringLiteral("Food")));
    REQUIRE(model.addExpense(QStringLiteral("2.5"), kMar2));
    REQUIRE(model.addExpense(QStringLiteral("3"), kMar2, QStringLiteral("Fun")));

    REQUIRE(model.expenses().size() == 3);
    CHECK(model.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(model.expenses().at(1).category.isEmpty());
    CHECK(model.expenses().at(2).category == QStringLiteral("Fun"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("700"));
}

TEST_CASE("numeric amounts are still rejected, including with a category") {
    BudgetModel model;
    QString error;

    CHECK_FALSE(model.addExpense(12.5, kMar2, QStringLiteral("Food"), &error));
    CHECK(error == QStringLiteral("Enter the expense as decimal text in dollars."));
    CHECK_FALSE(model.addExpense(1250, kMar2, QStringLiteral("Food"), &error));
    CHECK_FALSE(model.addExpense(12.5, kMar2, &error));

    checkNothingRecorded(model);
}

TEST_CASE("a rejected categorized expense leaves the expense list and totals unchanged") {
    BudgetModel model;
    REQUIRE(model.addExpense(QStringLiteral("12.5025"), kMar2, QStringLiteral("Food")));
    QString error;

    CHECK_FALSE(model.addExpense(QStringLiteral("abc"), kMar2, QStringLiteral("Fun"), &error));
    CHECK(error == QStringLiteral("Enter a decimal dollar amount such as 12.99."));
    CHECK_FALSE(model.addExpense(QStringLiteral("-1"), kMar2, QStringLiteral("Fun"), &error));
    CHECK(error == QStringLiteral("Expense amount cannot be negative."));
    CHECK_FALSE(model.addExpense(QStringLiteral(""), kMar2, QStringLiteral("Fun"), &error));
    CHECK(error == QStringLiteral("Expense amount is required."));
    CHECK_FALSE(model.addExpense(QStringLiteral("5"), QDate(), QStringLiteral("Fun"), &error));
    CHECK(error == QStringLiteral("Choose a valid expense date."));

    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
}

// ---- Controller ----------------------------------------------------------------------

TEST_CASE("the controller records the category and shows it in the expense row") {
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setDisplay(&display);
    display.messages.clear();

    CHECK(controller.addExpense("12.50", kMar2, "  Food "));

    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).category == QStringLiteral("Food"));
    REQUIRE(display.screens.back().expenses.size() == 1);
    const ExpenseRow &row = display.screens.back().expenses.at(0);
    CHECK(row.date == QStringLiteral("Mar 2, 2026"));
    CHECK(row.amount == QStringLiteral("$12.50"));
    CHECK(row.category == QStringLiteral("Food"));
    REQUIRE(display.messages.size() == 1);
    CHECK(display.messages.at(0).first == QStringLiteral("Expense saved."));
    CHECK(display.messages.at(0).second == false);
}

TEST_CASE("an expense without a category is shown as Uncategorized") {
    BudgetModel model;
    BudgetController controller(model);

    CHECK(controller.addExpense("1", QDate(2026, 3, 1)));
    CHECK(controller.addExpense("2", QDate(2026, 3, 2), QString()));
    CHECK(controller.addExpense("3", QDate(2026, 3, 3), "   "));

    const BudgetScreen screen = controller.screen();
    REQUIRE(screen.expenses.size() == 3);
    for (const ExpenseRow &row : screen.expenses) CHECK(row.category == QStringLiteral("Uncategorized"));
}

TEST_CASE("a rejected categorized expense shows only the error and records nothing") {
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setDisplay(&display);
    display.messages.clear();
    display.events.clear();

    CHECK_FALSE(controller.addExpense("abc", kMar2, "Food"));

    CHECK(display.events == std::vector<char>{'M'});
    REQUIRE(display.messages.size() == 1);
    CHECK(display.messages.at(0).first == QStringLiteral("Enter a decimal dollar amount such as 12.99."));
    CHECK(display.messages.at(0).second == true);
    checkNothingRecorded(model);
}
