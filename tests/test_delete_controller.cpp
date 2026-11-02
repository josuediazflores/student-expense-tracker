// Part 4, task 15: deleting budgets and expenses through the Controller
// (docs/plans/mvc-mvp.md, D25 and D26, and "Part 4 plan reviews" items 1 to 4). A request asks
// the display one question, resolveDelete carries out the answer, and the row the user
// clicked (newest first for expenses) must map to the right Model item. A fake display
// records everything.
#include "doctest.h"

#include "BudgetController.h"
#include "BudgetDisplay.h"
#include "BudgetModel.h"
#include "persistence_test_support.h"

#include <QStringList>

#include <vector>

using namespace persistence_test;

namespace {
const QString kChooseBudget = QStringLiteral("Choose a budget to delete.");
const QString kChooseExpense = QStringLiteral("Choose an expense to delete.");
const QString kBudgetDeleted = QStringLiteral("Budget deleted.");
const QString kExpenseDeleted = QStringLiteral("Expense deleted.");
const QString kNothingDeleted = QStringLiteral("Nothing was deleted.");
const QDate kNov1(2026, 11, 1);
const QDate kNov10(2026, 11, 10);
const QDate kNov15(2026, 11, 15);
const QDate kDec1(2026, 12, 1);
const QDate kDec15(2026, 12, 15);

const QString kMealPlan250 =
    QStringLiteral("Delete the Meal plan budget of $250.00 for Nov 1, 2026 to Dec 1, 2026?");
const QString kMealPlan50 =
    QStringLiteral("Delete the Meal plan budget of $50.00 for Nov 1, 2026 to Dec 1, 2026?");

// The exact dollar text of each expense in Model (entry) order, e.g. "1000" for $10.
QStringList amountsInModelOrder(const BudgetModel &model) {
    QStringList result;
    for (const Expense &expense : model.expenses()) result.append(expense.amountCents.toString());
    return result;
}

// A controller on a fake display. Nothing is seeded; each test says what it needs.
struct Wired {
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    Wired() { controller.setDisplay(&display); }

    void clearDisplay() {
        display.screens.clear();
        display.messages.clear();
        display.conflicts.clear();
        display.deletions.clear();
        display.events.clear();
    }

    // Meal plan $250.00, Nov 1 to Dec 1, 2026.
    void seedMealPlan() {
        REQUIRE(controller.createBudget("Meal plan", "250.00", kNov1, kDec1));
        clearDisplay();
    }

    // Huy's real file: Meal plan $50 and $250, the same dates, in that order. The overlap rule
    // blocks the second addBudget, so the saved-data path restoreBudget seeds the twin.
    void seedTwins() {
        REQUIRE(model.restoreBudget({"Meal plan", 5000, kNov1, kDec1}));
        REQUIRE(model.restoreBudget({"Meal plan", 25000, kNov1, kDec1}));
        clearDisplay();
    }

    // Entered A, B, C. Shown newest first: row 0 = B ($20, Nov 10), row 1 = A ($10, Nov 10),
    // row 2 = C ($30, Nov 1). Model indices: A 0, B 1, C 2.
    void seedNewestFirst() {
        REQUIRE(controller.addExpense("10.00", kNov10, "Alpha"));
        REQUIRE(controller.addExpense("20.00", kNov10, "Beta"));
        REQUIRE(controller.addExpense("30.00", kNov1, "Gamma"));
        REQUIRE(display.screens.back().expenses.size() == 3);
        CHECK(display.screens.back().expenses.at(0).amount == QStringLiteral("$20.00"));
        CHECK(display.screens.back().expenses.at(1).amount == QStringLiteral("$10.00"));
        CHECK(display.screens.back().expenses.at(2).amount == QStringLiteral("$30.00"));
        clearDisplay();
    }

    void checkBudgetUntouched() {
        REQUIRE(model.budgets().size() == 1);
        CHECK(model.budgets().at(0).amountCents == 25000);
    }
};
}

// ---- A request asks, once, with the exact text ----------------------------------------

TEST_CASE("requesting a budget delete asks exactly one question naming that budget") {
    Wired w;
    w.seedMealPlan();

    CHECK(w.controller.requestDeleteBudget(0));

    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) == kMealPlan250);
    CHECK(w.display.events == std::vector<char>{'D'});
    CHECK(w.display.messages.empty());
    CHECK(w.display.screens.empty());
    w.checkBudgetUntouched();
}

TEST_CASE("requesting an expense delete names its category, amount and date") {
    Wired w;
    REQUIRE(w.controller.addExpense("12.50", kNov1, "Groceries"));
    w.clearDisplay();

    CHECK(w.controller.requestDeleteExpense(0));

    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) ==
          QStringLiteral("Delete the Groceries expense of $12.50 on Nov 1, 2026?"));
    CHECK(w.display.events == std::vector<char>{'D'});
    CHECK(w.model.expenses().size() == 1);
}

TEST_CASE("an uncategorized expense is called uncategorized in the question") {
    Wired w;
    REQUIRE(w.controller.addExpense("100", kNov1));
    w.clearDisplay();

    CHECK(w.controller.requestDeleteExpense(0));

    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) ==
          QStringLiteral("Delete the uncategorized expense of $100.00 on Nov 1, 2026?"));
}

TEST_CASE("the question keeps the fraction of a cent the screen shows") {
    Wired w;
    REQUIRE(w.controller.addExpense("12.345", kNov1, "Food"));
    w.clearDisplay();

    CHECK(w.controller.requestDeleteExpense(0));

    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) ==
          QStringLiteral("Delete the Food expense of $12.345 on Nov 1, 2026?"));
}

// ---- Delete and Cancel ---------------------------------------------------------------

TEST_CASE("choosing Delete removes the budget, shows Budget deleted. and pushes the screen") {
    Wired w;
    w.seedMealPlan();
    REQUIRE(w.controller.addExpense("5", kNov10, "Food"));
    REQUIRE(w.controller.requestDeleteBudget(0));
    w.clearDisplay();

    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(w.model.budgets().isEmpty());
    CHECK(w.display.events == std::vector<char>{'S', 'M'});
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kBudgetDeleted);
    CHECK(w.display.messages.at(0).second == false);
    REQUIRE(w.display.screens.size() == 1);
    CHECK(w.display.screens.at(0).budgets.isEmpty());
    CHECK(w.display.screens.at(0).totalBudget == QStringLiteral("$0.00"));
    CHECK(w.display.screens.at(0).remaining == QStringLiteral("-$5.00"));
    CHECK(w.display.screens.at(0).expenses.size() == 1);  // the expense stays
    CHECK(w.model.expenses().at(0).category == QStringLiteral("Food"));
}

TEST_CASE("choosing Delete removes the expense, shows Expense deleted. and pushes the screen") {
    Wired w;
    w.seedMealPlan();
    REQUIRE(w.controller.addExpense("12.345", kNov10, "Food"));
    REQUIRE(w.controller.addExpense("1", kNov1));
    REQUIRE(w.controller.requestDeleteExpense(0));  // the Nov 10 expense is shown first
    w.clearDisplay();

    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    REQUIRE(w.model.expenses().size() == 1);
    CHECK(w.model.expenses().at(0).amountCents.toString() == QStringLiteral("100"));
    CHECK(w.display.events == std::vector<char>{'S', 'M'});
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kExpenseDeleted);
    CHECK(w.display.messages.at(0).second == false);
    REQUIRE(w.display.screens.size() == 1);
    CHECK(w.display.screens.at(0).totalSpending == QStringLiteral("$1.00"));
    CHECK(w.display.screens.at(0).remaining == QStringLiteral("$249.00"));
    REQUIRE(w.display.screens.at(0).expenses.size() == 1);
    CHECK(w.display.screens.at(0).expenses.at(0).amount == QStringLiteral("$1.00"));
    w.checkBudgetUntouched();
}

TEST_CASE("choosing Cancel on a budget changes nothing and shows Nothing was deleted.") {
    Wired w;
    w.seedMealPlan();
    REQUIRE(w.controller.requestDeleteBudget(0));
    w.clearDisplay();

    CHECK(w.controller.resolveDelete(DeleteChoice::Cancel));

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kNothingDeleted);
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.events == std::vector<char>{'M'});
    w.checkBudgetUntouched();
}

TEST_CASE("choosing Cancel on an expense changes nothing and shows Nothing was deleted.") {
    Wired w;
    REQUIRE(w.controller.addExpense("12.50", kNov1, "Groceries"));
    REQUIRE(w.controller.requestDeleteExpense(0));
    w.clearDisplay();

    CHECK(w.controller.resolveDelete(DeleteChoice::Cancel));

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kNothingDeleted);
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.events == std::vector<char>{'M'});
    REQUIRE(w.model.expenses().size() == 1);
    CHECK(w.model.expenses().at(0).amountCents.toString() == QStringLiteral("1250"));
}

TEST_CASE("every answer clears the pending delete, so a second answer does nothing") {
    for (DeleteChoice first : {DeleteChoice::Delete, DeleteChoice::Cancel}) {
        Wired w;
        w.seedMealPlan();
        REQUIRE(w.controller.addExpense("1", kNov1));
        REQUIRE(w.controller.requestDeleteExpense(0));
        REQUIRE(w.controller.resolveDelete(first));
        w.clearDisplay();

        CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));

        REQUIRE(w.display.messages.size() == 1);
        CHECK(w.display.messages.at(0).first == kNothingDeleted);
        w.checkBudgetUntouched();
    }
}

// ---- A bad shown row -----------------------------------------------------------------

TEST_CASE("a bad budget row is an error, asks nothing and leaves nothing pending") {
    Wired w;
    w.seedMealPlan();

    for (int row : {-1, 1, 50}) {
        CHECK_FALSE(w.controller.requestDeleteBudget(row));
    }

    CHECK(w.display.deletions.empty());
    REQUIRE(w.display.messages.size() == 3);
    for (const auto &message : w.display.messages) {
        CHECK(message.first == kChooseBudget);
        CHECK(message.second == true);
    }
    w.clearDisplay();
    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));
    w.checkBudgetUntouched();
}

TEST_CASE("a bad expense row is an error, asks nothing and leaves nothing pending") {
    Wired w;
    REQUIRE(w.controller.addExpense("5", kNov1));
    REQUIRE(w.controller.addExpense("6", kNov10));
    w.clearDisplay();

    for (int row : {-1, 2, 50}) {
        CHECK_FALSE(w.controller.requestDeleteExpense(row));
    }

    CHECK(w.display.deletions.empty());
    REQUIRE(w.display.messages.size() == 3);
    for (const auto &message : w.display.messages) {
        CHECK(message.first == kChooseExpense);
        CHECK(message.second == true);
    }
    w.clearDisplay();
    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));
    CHECK(w.model.expenses().size() == 2);
}

TEST_CASE("deleting from an empty list is an error too") {
    Wired w;

    CHECK_FALSE(w.controller.requestDeleteBudget(0));
    CHECK_FALSE(w.controller.requestDeleteExpense(0));

    CHECK(w.display.deletions.empty());
    REQUIRE(w.display.messages.size() == 2);
    CHECK(w.display.messages.at(0).first == kChooseBudget);
    CHECK(w.display.messages.at(1).first == kChooseExpense);
}

// ---- The shown row is not the Model index --------------------------------------------

TEST_CASE("the screen order is as the delete tests assume") {
    Wired w;
    w.seedNewestFirst();

    CHECK(amountsInModelOrder(w.model) == QStringList({"1000", "2000", "3000"}));
}

TEST_CASE("deleting shown row 0 removes the later of two same-date expenses") {
    Wired w;
    w.seedNewestFirst();

    REQUIRE(w.controller.requestDeleteExpense(0));
    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) ==
          QStringLiteral("Delete the Beta expense of $20.00 on Nov 10, 2026?"));
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(amountsInModelOrder(w.model) == QStringList({"1000", "3000"}));
}

TEST_CASE("deleting shown row 1 removes the earlier of two same-date expenses") {
    Wired w;
    w.seedNewestFirst();

    REQUIRE(w.controller.requestDeleteExpense(1));
    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) ==
          QStringLiteral("Delete the Alpha expense of $10.00 on Nov 10, 2026?"));
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(amountsInModelOrder(w.model) == QStringList({"2000", "3000"}));
}

TEST_CASE("deleting shown row 2 removes the older expense entered last") {
    Wired w;
    w.seedNewestFirst();

    REQUIRE(w.controller.requestDeleteExpense(2));
    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) ==
          QStringLiteral("Delete the Gamma expense of $30.00 on Nov 1, 2026?"));
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(amountsInModelOrder(w.model) == QStringList({"1000", "2000"}));
}

TEST_CASE("after a delete the next shown row maps to the right expense again") {
    Wired w;
    w.seedNewestFirst();
    REQUIRE(w.controller.requestDeleteExpense(0));  // Beta
    REQUIRE(w.controller.resolveDelete(DeleteChoice::Delete));
    w.clearDisplay();

    REQUIRE(w.controller.requestDeleteExpense(1));  // now Gamma ($30, Nov 1)
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(amountsInModelOrder(w.model) == QStringList({"1000"}));
}

TEST_CASE("two identical expenses lose exactly one when one is deleted") {
    Wired w;
    REQUIRE(w.controller.addExpense("7.00", kNov10, "Food"));
    REQUIRE(w.controller.addExpense("7.00", kNov10, "Food"));
    REQUIRE(w.controller.requestDeleteExpense(0));

    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(amountsInModelOrder(w.model) == QStringList({"700"}));
}

// ---- Twin budgets: Meal plan $50 and $250, the same dates ----------------------------

TEST_CASE("deleting the $50 twin leaves exactly the $250 budget") {
    Wired w;
    w.seedTwins();

    REQUIRE(w.controller.requestDeleteBudget(0));
    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) == kMealPlan50);
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    w.checkBudgetUntouched();
    CHECK(w.model.budgets().at(0).category == QStringLiteral("Meal plan"));
    CHECK(w.model.budgets().at(0).startDate == kNov1);
    CHECK(w.model.budgets().at(0).endDate == kDec1);
    REQUIRE(w.display.screens.size() == 1);
    CHECK(w.display.screens.at(0).totalBudget == QStringLiteral("$250.00"));
    REQUIRE(w.display.screens.at(0).budgets.size() == 1);
    CHECK(w.display.screens.at(0).budgets.at(0).total == QStringLiteral("$250.00"));
}

TEST_CASE("deleting the $250 twin leaves exactly the $50 budget") {
    Wired w;
    w.seedTwins();

    REQUIRE(w.controller.requestDeleteBudget(1));
    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) == kMealPlan250);
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    REQUIRE(w.model.budgets().size() == 1);
    CHECK(w.model.budgets().at(0).amountCents == 5000);
}

// ---- One pending delete at a time ----------------------------------------------------

TEST_CASE("a second request while one is pending is refused and shows nothing new") {
    Wired w;
    w.seedTwins();
    REQUIRE(w.controller.requestDeleteBudget(0));

    CHECK_FALSE(w.controller.requestDeleteBudget(1));
    CHECK_FALSE(w.controller.requestDeleteExpense(0));

    REQUIRE(w.display.deletions.size() == 1);
    CHECK(w.display.deletions.at(0) == kMealPlan50);
    CHECK(w.display.messages.empty());
    CHECK(w.display.events == std::vector<char>{'D'});
}

TEST_CASE("after refused requests, Delete removes only the first budget named") {
    Wired w;
    w.seedTwins();
    REQUIRE(w.controller.requestDeleteBudget(0));
    REQUIRE_FALSE(w.controller.requestDeleteBudget(1));

    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    REQUIRE(w.model.budgets().size() == 1);
    CHECK(w.model.budgets().at(0).amountCents == 25000);
}

TEST_CASE("after refused requests, Delete removes only the first expense named") {
    Wired w;
    w.seedNewestFirst();
    REQUIRE(w.controller.requestDeleteExpense(2));  // Gamma
    REQUIRE_FALSE(w.controller.requestDeleteExpense(0));
    REQUIRE_FALSE(w.controller.requestDeleteExpense(1));

    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(amountsInModelOrder(w.model) == QStringList({"1000", "2000"}));
}

TEST_CASE("a request of the other kind is refused too, and Delete removes only the first item") {
    Wired w;
    w.seedMealPlan();
    REQUIRE(w.controller.addExpense("5", kNov10));
    REQUIRE(w.controller.requestDeleteExpense(0));
    w.clearDisplay();

    CHECK_FALSE(w.controller.requestDeleteBudget(0));
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(w.model.expenses().isEmpty());
    w.checkBudgetUntouched();
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kExpenseDeleted);
}

TEST_CASE("after Cancel a new request is accepted again") {
    Wired w;
    w.seedTwins();
    REQUIRE(w.controller.requestDeleteBudget(0));
    REQUIRE(w.controller.resolveDelete(DeleteChoice::Cancel));

    CHECK(w.controller.requestDeleteBudget(1));

    REQUIRE(w.display.deletions.size() == 2);
    CHECK(w.display.deletions.at(1) == kMealPlan250);
}

TEST_CASE("Delete removes nothing when the pending item is no longer at its index") {
    Wired w;
    w.seedTwins();
    REQUIRE(w.controller.requestDeleteBudget(0));  // the $50 twin
    REQUIRE(w.model.removeBudget(0));              // gone behind the controller's back
    w.clearDisplay();

    w.controller.resolveDelete(DeleteChoice::Delete);

    REQUIRE(w.model.budgets().size() == 1);
    CHECK(w.model.budgets().at(0).amountCents == 25000);  // the $250 twin now sits at index 0
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kNothingDeleted);
    CHECK(w.display.messages.at(0).second == false);
}

TEST_CASE("Delete removes nothing when the pending expense was changed or moved") {
    Wired w;
    w.seedNewestFirst();
    REQUIRE(w.controller.requestDeleteExpense(0));  // Beta, Model index 1
    REQUIRE(w.model.removeExpense(0));              // Alpha goes, so Beta moves to index 0
    w.clearDisplay();

    w.controller.resolveDelete(DeleteChoice::Delete);

    // Index 1 is now Gamma, which is not the item that was named.
    CHECK(amountsInModelOrder(w.model) == QStringList({"2000", "3000"}));
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kNothingDeleted);
    CHECK(w.display.messages.at(0).second == false);
}

// ---- A stale answer ------------------------------------------------------------------

TEST_CASE("resolving with nothing pending shows Nothing was deleted. and returns false") {
    Wired w;
    w.seedMealPlan();

    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));
    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Cancel));

    REQUIRE(w.display.messages.size() == 2);
    for (const auto &message : w.display.messages) {
        CHECK(message.first == kNothingDeleted);
        CHECK(message.second == false);
    }
    CHECK(w.display.screens.empty());
    w.checkBudgetUntouched();
}

TEST_CASE("loadSaved clears a pending delete, so a later Delete does nothing") {
    Wired w;
    w.seedMealPlan();
    REQUIRE(w.controller.requestDeleteBudget(0));
    REQUIRE(w.controller.loadSaved());
    w.clearDisplay();

    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));

    w.checkBudgetUntouched();
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kNothingDeleted);
}

TEST_CASE("any other controller action clears a pending delete") {
    Wired w;
    w.seedMealPlan();

    REQUIRE(w.controller.requestDeleteBudget(0));
    REQUIRE(w.controller.addExpense("5", kNov10));
    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));

    REQUIRE(w.controller.requestDeleteBudget(0));
    REQUIRE_FALSE(w.controller.updateBudgetAmount(9, "1"));
    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));

    REQUIRE(w.controller.requestDeleteBudget(0));
    REQUIRE(w.controller.createBudget("Transport", "10", kNov1, kDec1));
    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(w.model.budgets().size() == 2);
    CHECK(w.model.expenses().size() == 1);
}

// ---- The delete prompt and the conflict prompt clear each other ----------------------

TEST_CASE("a delete request clears a pending duplicate-budget question") {
    Wired w;
    w.seedMealPlan();
    REQUIRE_FALSE(w.controller.createBudget("Meal plan", "300.00", kNov15, kDec15));
    REQUIRE(w.display.conflicts.size() == 1);

    CHECK(w.controller.requestDeleteBudget(0));

    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));
    w.checkBudgetUntouched();
    CHECK(w.model.budgets().at(0).startDate == kNov1);
    CHECK(w.controller.resolveDelete(DeleteChoice::Delete));
    CHECK(w.model.budgets().isEmpty());
}

TEST_CASE("a duplicate-budget question clears a pending delete") {
    Wired w;
    w.seedMealPlan();
    REQUIRE(w.controller.requestDeleteBudget(0));

    CHECK_FALSE(w.controller.createBudget("Meal plan", "300.00", kNov15, kDec15));

    REQUIRE(w.display.conflicts.size() == 1);
    w.clearDisplay();
    CHECK_FALSE(w.controller.resolveDelete(DeleteChoice::Delete));
    w.checkBudgetUntouched();
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kNothingDeleted);
    CHECK(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));
    REQUIRE(w.model.budgets().size() == 1);
    CHECK(w.model.budgets().at(0).amountCents == 30000);
}
