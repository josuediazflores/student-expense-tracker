// Author: Huy Tran
// Verify the controller's behaviour through a fake display: what it asks the model to do,
// what view-ready text it pushes out, and that a rejected action changes nothing.
#include "doctest.h"

#include "BudgetController.h"
#include "BudgetDisplay.h"
#include "BudgetModel.h"
#include "persistence_test_support.h"

#include <utility>
#include <vector>

using persistence_test::FakeDisplay;

namespace {
bool sameScreen(const BudgetScreen &a, const BudgetScreen &b) {
    if (a.totalBudget != b.totalBudget || a.totalSpending != b.totalSpending ||
        a.remaining != b.remaining || a.budgets.size() != b.budgets.size() ||
        a.expenses.size() != b.expenses.size())
        return false;
    for (qsizetype i = 0; i < a.budgets.size(); ++i) {
        const BudgetRow &x = a.budgets.at(i);
        const BudgetRow &y = b.budgets.at(i);
        if (x.category != y.category || x.total != y.total || x.period != y.period ||
            x.perDay != y.perDay)
            return false;
    }
    for (qsizetype i = 0; i < a.expenses.size(); ++i) {
        if (a.expenses.at(i).date != b.expenses.at(i).date ||
            a.expenses.at(i).amount != b.expenses.at(i).amount)
            return false;
    }
    return true;
}

// A model, a controller and a fake display, wired in the order main() wires them.
struct Wired {
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    Wired() { controller.setDisplay(&display); }
};

// The same wiring, but the model is filled before the display is attached, so the
// first pushed screen already shows two budgets and one fractional expense.
struct Seeded {
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    Seeded() {
        REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 1, 1), QDate(2026, 1, 31)}));
        REQUIRE(model.addBudget({"Transport", 5000, QDate(2026, 11, 1), QDate(2026, 11, 30)}));
        REQUIRE(model.addExpense("12.5025", QDate(2026, 2, 1)));
        controller.setDisplay(&display);
    }

    void checkRejected(bool returned, const QString &message) {
        CHECK_FALSE(returned);
        // Exactly the initial screen; the rejection produced no new one.
        CHECK(display.screens.size() == 1);
        REQUIRE(display.messages.size() == 1);
        CHECK(display.messages.at(0).first == message);
        CHECK(display.messages.at(0).second == true);
        CHECK(display.events == std::vector<char>{'S', 'M'});
        CHECK(sameScreen(controller.screen(), display.screens.at(0)));
        // The model itself is untouched: budgets, expense list, totals.
        REQUIRE(model.budgets().size() == 2);
        CHECK(model.budgets().at(0).category == QStringLiteral("Food"));
        CHECK(model.budgets().at(0).amountCents == 10000);
        CHECK(model.budgets().at(1).category == QStringLiteral("Transport"));
        CHECK(model.budgets().at(1).amountCents == 5000);
        REQUIRE(model.expenses().size() == 1);
        CHECK(model.expenses().at(0).amountCents.toString() == QStringLiteral("1250.25"));
        CHECK(model.expenses().at(0).date == QDate(2026, 2, 1));
        CHECK(model.totalCents() == 15000);
        CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
        CHECK(model.remainingBudgetCents().toString() == QStringLiteral("13749.75"));
    }
};

const QDate kJan1(2026, 1, 1);
const QDate kJan31(2026, 1, 31);
}

// ---- Attaching a display -------------------------------------------------------------

TEST_CASE("attaching a display pushes one empty screen and no message") {
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;

    controller.setDisplay(&display);

    REQUIRE(display.screens.size() == 1);
    CHECK(display.messages.empty());
    const BudgetScreen &screen = display.screens.at(0);
    CHECK(screen.totalBudget == QStringLiteral("$0.00"));
    CHECK(screen.totalSpending == QStringLiteral("$0.00"));
    CHECK(screen.remaining == QStringLiteral("$0.00"));
    CHECK(screen.budgets.isEmpty());
    CHECK(screen.expenses.isEmpty());
}

TEST_CASE("attaching a display shows data that was already in the model") {
    Seeded s;

    REQUIRE(s.display.screens.size() == 1);
    const BudgetScreen &screen = s.display.screens.at(0);
    CHECK(screen.totalBudget == QStringLiteral("$150.00"));
    CHECK(screen.totalSpending == QStringLiteral("$12.5025"));
    CHECK(screen.remaining == QStringLiteral("$137.4975"));
    REQUIRE(screen.budgets.size() == 2);
    CHECK(screen.budgets.at(0).category == QStringLiteral("Food"));
    CHECK(screen.budgets.at(1).category == QStringLiteral("Transport"));
    REQUIRE(screen.expenses.size() == 1);
    CHECK(screen.expenses.at(0).date == QStringLiteral("Feb 1, 2026"));
    CHECK(screen.expenses.at(0).amount == QStringLiteral("$12.5025"));
}

TEST_CASE("screen() returns the same content that was pushed") {
    Seeded s;
    CHECK(sameScreen(s.controller.screen(), s.display.screens.at(0)));
}

TEST_CASE("attaching a null display pushes nothing and detaches the old one") {
    Wired w;
    REQUIRE(w.display.screens.size() == 1);

    w.controller.setDisplay(nullptr);
    CHECK(w.controller.createBudget("Food", "10", kJan1, kJan31));

    CHECK(w.display.screens.size() == 1);
    CHECK(w.display.messages.empty());
    CHECK(w.model.budgets().size() == 1);
}

TEST_CASE("attaching a second display pushes the current screen to it") {
    Wired w;
    REQUIRE(w.controller.createBudget("Food", "10", kJan1, kJan31));
    FakeDisplay second;

    w.controller.setDisplay(&second);

    REQUIRE(second.screens.size() == 1);
    CHECK(second.screens.at(0).totalBudget == QStringLiteral("$10.00"));
    CHECK(second.messages.empty());
}

// ---- createBudget --------------------------------------------------------------------

TEST_CASE("creating a budget stores it and pushes the new screen") {
    Wired w;

    CHECK(w.controller.createBudget("Groceries", "250.00", kJan1, kJan31));

    REQUIRE(w.model.budgets().size() == 1);
    CHECK(w.model.budgets().at(0).category == QStringLiteral("Groceries"));
    CHECK(w.model.budgets().at(0).amountCents == 25000);
    CHECK(w.model.budgets().at(0).startDate == kJan1);
    CHECK(w.model.budgets().at(0).endDate == kJan31);

    REQUIRE(w.display.screens.size() == 2);
    const BudgetScreen &screen = w.display.screens.at(1);
    CHECK(screen.totalBudget == QStringLiteral("$250.00"));
    CHECK(screen.totalSpending == QStringLiteral("$0.00"));
    CHECK(screen.remaining == QStringLiteral("$250.00"));
    REQUIRE(screen.budgets.size() == 1);
    CHECK(screen.budgets.at(0).category == QStringLiteral("Groceries"));
    CHECK(screen.budgets.at(0).total == QStringLiteral("$250.00"));
    CHECK(screen.budgets.at(0).period == QStringLiteral("Jan 1, 2026 to Jan 31, 2026"));
    CHECK(screen.budgets.at(0).perDay == QStringLiteral("$8.06"));
    CHECK(screen.expenses.isEmpty());
}

TEST_CASE("a successful create shows the screen first, then a success message") {
    Wired w;

    CHECK(w.controller.createBudget("Groceries", "250.00", kJan1, kJan31));

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == QStringLiteral("Budget added."));
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.events == std::vector<char>{'S', 'S', 'M'});
}

TEST_CASE("the screen after a create matches screen()") {
    Wired w;
    REQUIRE(w.controller.createBudget("Groceries", "250.00", kJan1, kJan31));
    CHECK(sameScreen(w.controller.screen(), w.display.screens.back()));
}

TEST_CASE("budget rows follow the order the budgets were added") {
    Wired w;
    REQUIRE(w.controller.createBudget("Zebra", "1", kJan1, kJan31));
    REQUIRE(w.controller.createBudget("Apple", "2", kJan1, kJan31));

    const BudgetScreen screen = w.controller.screen();
    REQUIRE(screen.budgets.size() == 2);
    CHECK(screen.budgets.at(0).category == QStringLiteral("Zebra"));
    CHECK(screen.budgets.at(1).category == QStringLiteral("Apple"));
    CHECK(screen.totalBudget == QStringLiteral("$3.00"));
}

TEST_CASE("a one-day budget shows a single-day period and its full amount per day") {
    Wired w;
    REQUIRE(w.controller.createBudget("Dining", "1234.50", QDate(2026, 3, 2), QDate(2026, 3, 2)));

    const BudgetRow row = w.controller.screen().budgets.at(0);
    CHECK(row.period == QStringLiteral("Mar 2, 2026 to Mar 2, 2026"));
    CHECK(row.perDay == QStringLiteral("$1,234.50"));
}

TEST_CASE("a budget amount of 12.345 is rejected with the whole-cents message") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "12.345", kJan1, kJan31),
                    QStringLiteral("Budget amount must be in whole cents."));
}

TEST_CASE("a negative budget amount is rejected with the model's negative message") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "-5", kJan1, kJan31),
                    QStringLiteral("Budget amount cannot be negative."));
}

TEST_CASE("a non-numeric budget amount is rejected with the format message") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "abc", kJan1, kJan31),
                    QStringLiteral("Enter a decimal dollar amount such as 12.99."));
}

TEST_CASE("an empty budget amount is rejected as required") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "", kJan1, kJan31),
                    QStringLiteral("Budget amount is required."));
}

TEST_CASE("a budget amount that is too large is rejected") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "92233720368547758.08", kJan1, kJan31),
                    QStringLiteral("Budget amount is too large."));
}

TEST_CASE("a budget that would push the total past the maximum is rejected") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "92233720368547758.07", kJan1, kJan31),
                    QStringLiteral("Total budget is too large."));
}

TEST_CASE("an end date before the start date is rejected with the model's message") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "5", kJan31, kJan1),
                    QStringLiteral("End date cannot be before start date."));
}

TEST_CASE("an invalid date is rejected with the model's message") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("Fun", "5", QDate(), kJan31),
                    QStringLiteral("Choose valid start and end dates."));
}

TEST_CASE("a blank category is rejected with the model's message") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("   ", "5", kJan1, kJan31),
                    QStringLiteral("Choose a category."));
}

TEST_CASE("the amount text is checked before the dates and category") {
    Seeded s;
    s.checkRejected(s.controller.createBudget("", "abc", QDate(), QDate()),
                    QStringLiteral("Enter a decimal dollar amount such as 12.99."));
}

// ---- updateBudgetAmount --------------------------------------------------------------

TEST_CASE("updating a budget changes its amount and pushes the new screen") {
    Wired w;
    REQUIRE(w.controller.createBudget("Food", "100", kJan1, kJan31));
    REQUIRE(w.controller.createBudget("Fun", "50", kJan1, kJan31));

    CHECK(w.controller.updateBudgetAmount(0, "300"));

    CHECK(w.model.budgets().at(0).amountCents == 30000);
    CHECK(w.model.budgets().at(1).amountCents == 5000);
    const BudgetScreen &screen = w.display.screens.back();
    CHECK(screen.totalBudget == QStringLiteral("$350.00"));
    CHECK(screen.remaining == QStringLiteral("$350.00"));
    REQUIRE(screen.budgets.size() == 2);
    CHECK(screen.budgets.at(0).category == QStringLiteral("Food"));
    CHECK(screen.budgets.at(0).total == QStringLiteral("$300.00"));
    CHECK(screen.budgets.at(1).total == QStringLiteral("$50.00"));
}

TEST_CASE("a successful update shows the screen first, then a success message") {
    Wired w;
    REQUIRE(w.controller.createBudget("Food", "100", kJan1, kJan31));
    w.display.events.clear();
    w.display.messages.clear();

    CHECK(w.controller.updateBudgetAmount(0, "300"));

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == QStringLiteral("Budget amount updated."));
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.events == std::vector<char>{'S', 'M'});
}

TEST_CASE("updating a budget recalculates remaining against existing spending") {
    Seeded s;

    CHECK(s.controller.updateBudgetAmount(1, "20.00"));

    const BudgetScreen screen = s.controller.screen();
    CHECK(screen.totalBudget == QStringLiteral("$120.00"));
    CHECK(screen.totalSpending == QStringLiteral("$12.5025"));
    CHECK(screen.remaining == QStringLiteral("$107.4975"));
    CHECK(screen.expenses.size() == 1);
}

TEST_CASE("updating a budget row past the end is rejected with the choose-a-budget message") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(5, "10"),
                    QStringLiteral("Choose a budget to update."));
}

TEST_CASE("updating with no row selected is rejected with the choose-a-budget message") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(-1, "10"),
                    QStringLiteral("Choose a budget to update."));
}

TEST_CASE("updating the row equal to the budget count is rejected") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(2, "10"),
                    QStringLiteral("Choose a budget to update."));
}

TEST_CASE("an update with non-numeric text is rejected with the format message") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(0, "abc"),
                    QStringLiteral("Enter a decimal dollar amount such as 12.99."));
}

TEST_CASE("an update with a fraction of a cent is rejected") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(0, "12.345"),
                    QStringLiteral("Budget amount must be in whole cents."));
}

TEST_CASE("an update with empty text is rejected as required") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(0, ""),
                    QStringLiteral("Budget amount is required."));
}

TEST_CASE("empty text with no row selected reports the amount before the selection") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(-1, ""),
                    QStringLiteral("Budget amount is required."));
}

TEST_CASE("a negative update amount is rejected with the model's negative message") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(0, "-5"),
                    QStringLiteral("Budget amount cannot be negative."));
}

TEST_CASE("an update that would push the total past the maximum is rejected") {
    Seeded s;
    s.checkRejected(s.controller.updateBudgetAmount(0, "92233720368547758.07"),
                    QStringLiteral("Total budget is too large."));
}

// ---- addExpense ----------------------------------------------------------------------

TEST_CASE("saving an expense stores it and pushes the new screen") {
    Wired w;
    REQUIRE(w.controller.createBudget("Food", "100", kJan1, kJan31));

    CHECK(w.controller.addExpense("12.50", QDate(2026, 3, 2)));

    REQUIRE(w.model.expenses().size() == 1);
    CHECK(w.model.expenses().at(0).amountCents == 1250);
    CHECK(w.model.expenses().at(0).date == QDate(2026, 3, 2));

    const BudgetScreen &screen = w.display.screens.back();
    CHECK(screen.totalBudget == QStringLiteral("$100.00"));
    CHECK(screen.totalSpending == QStringLiteral("$12.50"));
    CHECK(screen.remaining == QStringLiteral("$87.50"));
    REQUIRE(screen.expenses.size() == 1);
    CHECK(screen.expenses.at(0).date == QStringLiteral("Mar 2, 2026"));
    CHECK(screen.expenses.at(0).amount == QStringLiteral("$12.50"));
}

TEST_CASE("a successful expense shows the screen first, then a success message") {
    Wired w;
    w.display.events.clear();

    CHECK(w.controller.addExpense("12.50", QDate(2026, 3, 2)));

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == QStringLiteral("Expense saved."));
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.events == std::vector<char>{'S', 'M'});
}

TEST_CASE("a zero expense is saved and shown as $0.00") {
    Wired w;

    CHECK(w.controller.addExpense("0", QDate(2026, 3, 2)));

    REQUIRE(w.controller.screen().expenses.size() == 1);
    CHECK(w.controller.screen().expenses.at(0).amount == QStringLiteral("$0.00"));
}

TEST_CASE("a null date is rejected with the model's date message") {
    Seeded s;
    s.checkRejected(s.controller.addExpense("5.00", QDate()),
                    QStringLiteral("Choose a valid expense date."));
}

TEST_CASE("an impossible calendar date is rejected") {
    Seeded s;
    s.checkRejected(s.controller.addExpense("5.00", QDate(2026, 2, 30)),
                    QStringLiteral("Choose a valid expense date."));
}

TEST_CASE("an empty expense amount is rejected as required") {
    Seeded s;
    s.checkRejected(s.controller.addExpense("", QDate(2026, 3, 2)),
                    QStringLiteral("Expense amount is required."));
}

TEST_CASE("a non-numeric expense amount is rejected with the format message") {
    Seeded s;
    s.checkRejected(s.controller.addExpense("abc", QDate(2026, 3, 2)),
                    QStringLiteral("Enter a decimal dollar amount such as 12.99."));
}

TEST_CASE("a negative expense amount is rejected") {
    Seeded s;
    s.checkRejected(s.controller.addExpense("-1", QDate(2026, 3, 2)),
                    QStringLiteral("Expense amount cannot be negative."));
}

TEST_CASE("an expense that would overflow total spending is rejected") {
    Seeded s;
    s.checkRejected(s.controller.addExpense("92233720368547758.07", QDate(2026, 3, 2)),
                    QStringLiteral("Total spending is too large."));
}

TEST_CASE("an amount error is reported before a date error") {
    Seeded s;
    s.checkRejected(s.controller.addExpense("abc", QDate()),
                    QStringLiteral("Enter a decimal dollar amount such as 12.99."));
}

// ---- Expense list order --------------------------------------------------------------

TEST_CASE("expenses on the same date list the later entry first") {
    Wired w;
    REQUIRE(w.controller.addExpense("1", QDate(2026, 3, 1)));
    REQUIRE(w.controller.addExpense("3", QDate(2026, 3, 3)));
    REQUIRE(w.controller.addExpense("2", QDate(2026, 3, 1)));

    const BudgetScreen screen = w.controller.screen();
    REQUIRE(screen.expenses.size() == 3);
    CHECK(screen.expenses.at(0).amount == QStringLiteral("$3.00"));
    CHECK(screen.expenses.at(1).amount == QStringLiteral("$2.00"));
    CHECK(screen.expenses.at(2).amount == QStringLiteral("$1.00"));
    CHECK(screen.expenses.at(0).date == QStringLiteral("Mar 3, 2026"));
    CHECK(screen.expenses.at(1).date == QStringLiteral("Mar 1, 2026"));
    CHECK(screen.expenses.at(2).date == QStringLiteral("Mar 1, 2026"));
}

TEST_CASE("the model keeps expenses in entry order while the screen sorts them") {
    Wired w;
    REQUIRE(w.controller.addExpense("1", QDate(2026, 3, 1)));
    REQUIRE(w.controller.addExpense("3", QDate(2026, 3, 3)));
    REQUIRE(w.controller.addExpense("2", QDate(2026, 3, 1)));

    REQUIRE(w.model.expenses().size() == 3);
    CHECK(w.model.expenses().at(0).amountCents == 100);
    CHECK(w.model.expenses().at(1).amountCents == 300);
    CHECK(w.model.expenses().at(2).amountCents == 200);
}

TEST_CASE("expenses are ordered by calendar date, not by the text of the date") {
    Wired w;
    // As text, "Mar" sorts after "Feb" and "Apr", and "Mar 9" after "Mar 10".
    REQUIRE(w.controller.addExpense("1", QDate(2026, 3, 9)));
    REQUIRE(w.controller.addExpense("2", QDate(2026, 4, 5)));
    REQUIRE(w.controller.addExpense("3", QDate(2026, 3, 10)));
    REQUIRE(w.controller.addExpense("4", QDate(2026, 2, 20)));

    const BudgetScreen screen = w.controller.screen();
    REQUIRE(screen.expenses.size() == 4);
    CHECK(screen.expenses.at(0).date == QStringLiteral("Apr 5, 2026"));
    CHECK(screen.expenses.at(1).date == QStringLiteral("Mar 10, 2026"));
    CHECK(screen.expenses.at(2).date == QStringLiteral("Mar 9, 2026"));
    CHECK(screen.expenses.at(3).date == QStringLiteral("Feb 20, 2026"));
}

TEST_CASE("expenses are ordered across a year boundary") {
    Wired w;
    REQUIRE(w.controller.addExpense("1", QDate(2025, 12, 31)));
    REQUIRE(w.controller.addExpense("2", QDate(2026, 1, 1)));

    const BudgetScreen screen = w.controller.screen();
    REQUIRE(screen.expenses.size() == 2);
    CHECK(screen.expenses.at(0).date == QStringLiteral("Jan 1, 2026"));
    CHECK(screen.expenses.at(1).date == QStringLiteral("Dec 31, 2025"));
}

TEST_CASE("an older expense entered last still lists after newer dates") {
    Wired w;
    REQUIRE(w.controller.addExpense("5", QDate(2026, 3, 10)));
    REQUIRE(w.controller.addExpense("6", QDate(2026, 3, 1)));

    const BudgetScreen screen = w.controller.screen();
    CHECK(screen.expenses.at(0).amount == QStringLiteral("$5.00"));
    CHECK(screen.expenses.at(1).amount == QStringLiteral("$6.00"));
}

// ---- Money formatting ----------------------------------------------------------------

TEST_CASE("an overspent budget shows a minus sign before the dollar sign") {
    Wired w;
    REQUIRE(w.controller.createBudget("Food", "100", kJan1, kJan31));
    REQUIRE(w.controller.addExpense("400", QDate(2026, 1, 5)));

    const BudgetScreen screen = w.controller.screen();
    CHECK(screen.totalBudget == QStringLiteral("$100.00"));
    CHECK(screen.totalSpending == QStringLiteral("$400.00"));
    CHECK(screen.remaining == QStringLiteral("-$300.00"));
}

TEST_CASE("a fraction of a cent is shown exactly, including a negative remaining") {
    Wired w;
    REQUIRE(w.controller.createBudget("Food", "10.00", kJan1, kJan31));
    REQUIRE(w.controller.addExpense("10.005", QDate(2026, 1, 5)));

    const BudgetScreen screen = w.controller.screen();
    CHECK(screen.totalSpending == QStringLiteral("$10.005"));
    CHECK(screen.remaining == QStringLiteral("-$0.005"));
    CHECK(screen.expenses.at(0).amount == QStringLiteral("$10.005"));
}

TEST_CASE("a three-decimal expense is not rounded to cents") {
    Wired w;
    REQUIRE(w.controller.addExpense("12.345", QDate(2026, 1, 5)));
    CHECK(w.controller.screen().totalSpending == QStringLiteral("$12.345"));
}

TEST_CASE("a one-decimal amount is padded to two decimal places") {
    Wired w;
    REQUIRE(w.controller.addExpense("1234.5", QDate(2026, 1, 5)));
    CHECK(w.controller.screen().expenses.at(0).amount == QStringLiteral("$1,234.50"));
    CHECK(w.controller.screen().totalSpending == QStringLiteral("$1,234.50"));
}

TEST_CASE("amounts under one dollar keep a leading zero") {
    Wired w;
    REQUIRE(w.controller.createBudget("Food", "0.05", kJan1, kJan31));
    CHECK(w.controller.screen().totalBudget == QStringLiteral("$0.05"));
    CHECK(w.controller.screen().budgets.at(0).total == QStringLiteral("$0.05"));
}

TEST_CASE("thousands are grouped with commas") {
    Wired w;
    REQUIRE(w.controller.createBudget("A", "999.99", kJan1, kJan31));
    CHECK(w.controller.screen().totalBudget == QStringLiteral("$999.99"));
    REQUIRE(w.controller.updateBudgetAmount(0, "1000"));
    CHECK(w.controller.screen().totalBudget == QStringLiteral("$1,000.00"));
    REQUIRE(w.controller.updateBudgetAmount(0, "1234567.89"));
    CHECK(w.controller.screen().totalBudget == QStringLiteral("$1,234,567.89"));
    REQUIRE(w.controller.updateBudgetAmount(0, "100000"));
    CHECK(w.controller.screen().budgets.at(0).total == QStringLiteral("$100,000.00"));
}

TEST_CASE("a large negative remaining is grouped and signed") {
    Wired w;
    REQUIRE(w.controller.addExpense("1234.5", QDate(2026, 1, 5)));
    CHECK(w.controller.screen().remaining == QStringLiteral("-$1,234.50"));
}

TEST_CASE("the largest expense is shown exactly") {
    Wired w;
    REQUIRE(w.controller.addExpense("92233720368547758.07", QDate(2026, 1, 5)));

    const BudgetScreen screen = w.controller.screen();
    CHECK(screen.totalSpending == QStringLiteral("$92,233,720,368,547,758.07"));
    CHECK(screen.expenses.at(0).amount == QStringLiteral("$92,233,720,368,547,758.07"));
    CHECK(screen.remaining == QStringLiteral("-$92,233,720,368,547,758.07"));
}

TEST_CASE("the largest budget is shown exactly") {
    Wired w;
    REQUIRE(w.controller.createBudget("Max", "92233720368547758.07", kJan1, kJan31));

    const BudgetScreen screen = w.controller.screen();
    CHECK(screen.totalBudget == QStringLiteral("$92,233,720,368,547,758.07"));
    CHECK(screen.budgets.at(0).total == QStringLiteral("$92,233,720,368,547,758.07"));
    CHECK(screen.remaining == QStringLiteral("$92,233,720,368,547,758.07"));
}

TEST_CASE("the per-day amount for a 31-day budget is rounded to cents") {
    Seeded s;
    const BudgetScreen screen = s.controller.screen();
    // Food: $100.00 over Jan 1-31 (31 days) is about $3.23.
    CHECK(screen.budgets.at(0).perDay == QStringLiteral("$3.23"));
    // Transport: $50.00 over Nov 1-30 (30 days) is about $1.67.
    CHECK(screen.budgets.at(1).perDay == QStringLiteral("$1.67"));
    CHECK(screen.budgets.at(1).period == QStringLiteral("Nov 1, 2026 to Nov 30, 2026"));
}

// ---- Behaviour without a display -----------------------------------------------------

TEST_CASE("actions without a display still change the model and return true") {
    BudgetModel model;
    BudgetController controller(model);

    CHECK(controller.createBudget("Food", "100", kJan1, kJan31));
    CHECK(controller.addExpense("12.50", QDate(2026, 3, 2)));
    CHECK(controller.updateBudgetAmount(0, "200"));

    CHECK(model.budgets().at(0).amountCents == 20000);
    CHECK(model.expenses().size() == 1);
    const BudgetScreen screen = controller.screen();
    CHECK(screen.totalBudget == QStringLiteral("$200.00"));
    CHECK(screen.remaining == QStringLiteral("$187.50"));
}

TEST_CASE("rejected actions without a display return false and change nothing") {
    BudgetModel model;
    BudgetController controller(model);
    REQUIRE(controller.createBudget("Food", "100", kJan1, kJan31));

    CHECK_FALSE(controller.createBudget("Food", "abc", kJan1, kJan31));
    CHECK_FALSE(controller.updateBudgetAmount(3, "10"));
    CHECK_FALSE(controller.addExpense("abc", QDate(2026, 3, 2)));
    CHECK_FALSE(controller.addExpense("5", QDate()));

    CHECK(model.budgets().size() == 1);
    CHECK(model.budgets().at(0).amountCents == 10000);
    CHECK(model.expenses().isEmpty());
    CHECK(model.totalCents() == 10000);
    CHECK(model.totalSpendingCents() == 0);
}

TEST_CASE("a display attached later shows what happened before it was attached") {
    BudgetModel model;
    BudgetController controller(model);
    REQUIRE(controller.createBudget("Food", "100", kJan1, kJan31));
    REQUIRE(controller.addExpense("25", QDate(2026, 3, 2)));
    FakeDisplay display;

    controller.setDisplay(&display);

    REQUIRE(display.screens.size() == 1);
    CHECK(display.screens.at(0).totalBudget == QStringLiteral("$100.00"));
    CHECK(display.screens.at(0).totalSpending == QStringLiteral("$25.00"));
    CHECK(display.screens.at(0).remaining == QStringLiteral("$75.00"));
}

TEST_CASE("the controller reads current model state, even if the model changed directly") {
    Wired w;
    REQUIRE(w.model.addBudget({"Direct", 4200, kJan1, kJan31}));

    const BudgetScreen screen = w.controller.screen();
    CHECK(screen.totalBudget == QStringLiteral("$42.00"));
    REQUIRE(screen.budgets.size() == 1);
    CHECK(screen.budgets.at(0).category == QStringLiteral("Direct"));
}

TEST_CASE("every action produces exactly one message") {
    Wired w;
    CHECK(w.controller.createBudget("Food", "100", kJan1, kJan31));
    CHECK_FALSE(w.controller.createBudget("Food", "", kJan1, kJan31));
    CHECK(w.controller.updateBudgetAmount(0, "90"));
    CHECK_FALSE(w.controller.updateBudgetAmount(4, "90"));
    CHECK(w.controller.addExpense("1", QDate(2026, 1, 2)));
    CHECK_FALSE(w.controller.addExpense("x", QDate(2026, 1, 2)));

    REQUIRE(w.display.messages.size() == 6);
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.messages.at(1).second == true);
    CHECK(w.display.messages.at(2).second == false);
    CHECK(w.display.messages.at(3).second == true);
    CHECK(w.display.messages.at(4).second == false);
    CHECK(w.display.messages.at(5).second == true);
    // One initial screen plus one per success; none for the three rejections.
    CHECK(w.display.screens.size() == 4);
}
