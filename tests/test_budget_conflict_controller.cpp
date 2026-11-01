// Part 3, task 12: the conflict prompt in the Controller (docs/plans/mvc-mvp.md, D19). A
// duplicate createBudget does not add or save; it asks the display one question, and
// resolveBudgetConflict carries out the user's answer. A fake display records everything.
#include "doctest.h"

#include "BudgetController.h"
#include "BudgetDisplay.h"
#include "BudgetModel.h"
#include "BudgetStore.h"
#include "persistence_test_support.h"

#include <vector>

using namespace persistence_test;

namespace {
const QString kQuestion = QStringLiteral(
    "You already have a Meal plan budget of $250.00 for Nov 1, 2026 to Dec 1, 2026.");
const QString kOverlap = QStringLiteral("A budget for this category already covers some of those dates.");
const QString kUpdated = QStringLiteral("Budget updated.");
const QString kAnother = QStringLiteral("Choose another category for this budget.");
const QString kNotAdded = QStringLiteral("Budget not added.");
const QDate kNov1(2026, 11, 1);
const QDate kNov15(2026, 11, 15);
const QDate kNov25(2026, 11, 25);
const QDate kDec1(2026, 12, 1);
const QDate kDec15(2026, 12, 15);
const QDate kDec20(2026, 12, 20);
const QDate kJan10(2027, 1, 10);

// Meal plan $250.00, Nov 1 to Dec 1, 2026, then a clean slate on the display.
struct Wired {
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    Wired() {
        controller.setDisplay(&display);
        REQUIRE(controller.createBudget("Meal plan", "250.00", kNov1, kDec1));
        clearDisplay();
    }

    void clearDisplay() {
        display.screens.clear();
        display.messages.clear();
        display.conflicts.clear();
        display.events.clear();
    }

    // The duplicate Huy hit: the same category over overlapping dates.
    bool createDuplicate(const QString &amount = QStringLiteral("300.00")) {
        return controller.createBudget("Meal plan", amount, kNov15, kDec15);
    }

    void checkModelUntouched() {
        REQUIRE(model.budgets().size() == 1);
        CHECK(model.budgets().at(0).amountCents == 25000);
        CHECK(model.budgets().at(0).startDate == kNov1);
        CHECK(model.budgets().at(0).endDate == kDec1);
        CHECK(model.totalCents() == 25000);
    }
};
}

// ---- A duplicate create asks instead of adding ---------------------------------------

TEST_CASE("a duplicate create returns false and asks exactly one question naming the existing budget") {
    Wired w;

    CHECK_FALSE(w.createDuplicate());

    REQUIRE(w.display.conflicts.size() == 1);
    CHECK(w.display.conflicts.at(0) == kQuestion);
    CHECK(w.display.events == std::vector<char>{'Q'});
    CHECK(w.display.messages.empty());
    CHECK(w.display.screens.empty());
    w.checkModelUntouched();
}

TEST_CASE("the question uses the existing budget's category spelling and values") {
    Wired w;

    CHECK_FALSE(w.controller.createBudget("  meal PLAN ", "99.00", kNov25, kDec20));

    REQUIRE(w.display.conflicts.size() == 1);
    CHECK(w.display.conflicts.at(0) == kQuestion);
}

TEST_CASE("a budget that does not overlap, or has another category, is added without a question") {
    Wired w;

    CHECK(w.controller.createBudget("Meal plan", "10", QDate(2026, 12, 2), kDec20));
    CHECK(w.controller.createBudget("Transport", "10", kNov1, kDec1));

    CHECK(w.display.conflicts.empty());
    CHECK(w.model.budgets().size() == 3);
    CHECK(w.display.events == std::vector<char>{'S', 'M', 'S', 'M'});
}

TEST_CASE("an overlap with a bad amount reports the amount problem, not a conflict") {
    Wired w;

    CHECK_FALSE(w.controller.createBudget("Meal plan", "abc", kNov15, kDec15));
    CHECK_FALSE(w.controller.createBudget("Meal plan", "-5", kNov15, kDec15));

    CHECK(w.display.conflicts.empty());
    REQUIRE(w.display.messages.size() == 2);
    CHECK(w.display.messages.at(0).first == QStringLiteral("Enter a decimal dollar amount such as 12.99."));
    CHECK(w.display.messages.at(1).first == QStringLiteral("Budget amount cannot be negative."));
    w.checkModelUntouched();
}

TEST_CASE("a duplicate create without a display returns false and changes nothing") {
    BudgetModel model;
    BudgetController controller(model);
    REQUIRE(controller.createBudget("Meal plan", "250.00", kNov1, kDec1));

    CHECK_FALSE(controller.createBudget("Meal plan", "300.00", kNov15, kDec15));

    REQUIRE(model.budgets().size() == 1);
    CHECK(model.budgets().at(0).amountCents == 25000);
}

// ---- Update existing -----------------------------------------------------------------

TEST_CASE("choosing Update existing replaces the budget instead of adding another") {
    Wired w;
    REQUIRE_FALSE(w.createDuplicate("300.00"));
    w.clearDisplay();

    CHECK(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    REQUIRE(w.model.budgets().size() == 1);
    CHECK(w.model.budgets().at(0).category == QStringLiteral("Meal plan"));
    CHECK(w.model.budgets().at(0).amountCents == 30000);
    CHECK(w.model.budgets().at(0).startDate == kNov15);
    CHECK(w.model.budgets().at(0).endDate == kDec15);
    CHECK(w.display.events == std::vector<char>{'S', 'M'});
    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kUpdated);
    CHECK(w.display.messages.at(0).second == false);
    REQUIRE(w.display.screens.size() == 1);
    CHECK(w.display.screens.at(0).totalBudget == QStringLiteral("$300.00"));
    REQUIRE(w.display.screens.at(0).budgets.size() == 1);
    CHECK(w.display.screens.at(0).budgets.at(0).period == QStringLiteral("Nov 15, 2026 to Dec 15, 2026"));
}

TEST_CASE("Update existing replaces the clashing row, not the first or last row") {
    BudgetModel model;
    BudgetController controller(model);
    REQUIRE(controller.createBudget("Food", "10", kNov1, kDec1));
    REQUIRE(controller.createBudget("Meal plan", "250.00", kNov1, kDec1));
    REQUIRE(controller.createBudget("Transport", "20", kNov1, kDec1));
    REQUIRE_FALSE(controller.createBudget("Meal plan", "300.00", kNov15, kDec15));

    CHECK(controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    REQUIRE(model.budgets().size() == 3);
    CHECK(model.budgets().at(0).category == QStringLiteral("Food"));
    CHECK(model.budgets().at(0).amountCents == 1000);
    CHECK(model.budgets().at(1).category == QStringLiteral("Meal plan"));
    CHECK(model.budgets().at(1).amountCents == 30000);
    CHECK(model.budgets().at(1).startDate == kNov15);
    CHECK(model.budgets().at(2).category == QStringLiteral("Transport"));
    CHECK(model.budgets().at(2).amountCents == 2000);
}

TEST_CASE("when the replacement itself is rejected, Update existing shows the error and changes nothing") {
    Wired w;
    // A second Meal plan far from the first, so the first can be duplicated by a range that
    // spans both; replacing row 0 with that range would then newly clash with row 1.
    REQUIRE(w.controller.createBudget("Meal plan", "50.00", kDec20, kJan10));
    REQUIRE_FALSE(w.controller.createBudget("Meal plan", "300.00", kNov15, QDate(2026, 12, 25)));
    w.clearDisplay();

    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kOverlap);
    CHECK(w.display.messages.at(0).second == true);
    REQUIRE(w.model.budgets().size() == 2);
    CHECK(w.model.budgets().at(0).amountCents == 25000);
    CHECK(w.model.budgets().at(0).endDate == kDec1);
}

// ---- The other two choices -----------------------------------------------------------

TEST_CASE("choosing Choose another category changes nothing and shows its message") {
    Wired w;
    REQUIRE_FALSE(w.createDuplicate());
    w.clearDisplay();

    w.controller.resolveBudgetConflict(ConflictChoice::ChooseAnotherCategory);

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kAnother);
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.conflicts.empty());
    w.checkModelUntouched();
}

TEST_CASE("choosing Cancel changes nothing and shows its message") {
    Wired w;
    REQUIRE_FALSE(w.createDuplicate());
    w.clearDisplay();

    w.controller.resolveBudgetConflict(ConflictChoice::Cancel);

    REQUIRE(w.display.messages.size() == 1);
    CHECK(w.display.messages.at(0).first == kNotAdded);
    CHECK(w.display.messages.at(0).second == false);
    CHECK(w.display.conflicts.empty());
    w.checkModelUntouched();
}

// ---- The pending request -------------------------------------------------------------

TEST_CASE("resolving with nothing pending returns false and does nothing") {
    Wired w;

    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));
    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::ChooseAnotherCategory));
    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::Cancel));

    CHECK(w.display.events.empty());
    w.checkModelUntouched();
}

TEST_CASE("every choice clears the pending request") {
    for (ConflictChoice choice : {ConflictChoice::UpdateExisting, ConflictChoice::ChooseAnotherCategory,
                                  ConflictChoice::Cancel}) {
        Wired w;
        REQUIRE_FALSE(w.createDuplicate());
        w.controller.resolveBudgetConflict(choice);
        w.clearDisplay();
        const qint64 before = w.model.budgets().at(0).amountCents;

        CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

        CHECK(w.display.events.empty());
        CHECK(w.model.budgets().at(0).amountCents == before);
    }
}

TEST_CASE("any other controller action clears the pending request") {
    Wired w;

    REQUIRE_FALSE(w.createDuplicate());
    REQUIRE(w.controller.addExpense("5", QDate(2026, 11, 2)));
    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    REQUIRE_FALSE(w.createDuplicate());
    REQUIRE_FALSE(w.controller.updateBudgetAmount(9, "1"));
    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    REQUIRE_FALSE(w.createDuplicate());
    REQUIRE(w.controller.createBudget("Transport", "10", kNov1, kDec1));
    CHECK_FALSE(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    CHECK(w.model.budgets().size() == 2);
    CHECK(w.model.budgets().at(0).amountCents == 25000);
}

TEST_CASE("a second duplicate create replaces the pending request") {
    Wired w;
    REQUIRE_FALSE(w.createDuplicate("300.00"));
    REQUIRE_FALSE(w.createDuplicate("400.00"));
    CHECK(w.display.conflicts.size() == 2);

    CHECK(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    CHECK(w.model.budgets().at(0).amountCents == 40000);
}

// ---- Saving --------------------------------------------------------------------------

namespace {
// Wired like the app: a store on a temporary file, loaded before the first action.
struct WiredWithStore {
    TempDir tmp;
    BudgetStore store{tmp.path()};
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    WiredWithStore() {
        controller.setStore(&store);
        controller.setDisplay(&display);
        REQUIRE(controller.loadSaved());
        REQUIRE(controller.createBudget("Meal plan", "250.00", kNov1, kDec1));
        // A marker row that any later save would overwrite.
        REQUIRE(rawExec(tmp.path(), {QStringLiteral("UPDATE budgets SET category = 'Marker'")}));
        display.messages.clear();
        display.events.clear();
    }

    bool fileIsUntouched() {
        return rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets WHERE category = 'Marker'") == 1 &&
               rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets") == 1;
    }
};
}

TEST_CASE("a duplicate create, Cancel and Choose another category do not save") {
    WiredWithStore w;

    CHECK_FALSE(w.controller.createBudget("Meal plan", "300.00", kNov15, kDec15));
    CHECK(w.fileIsUntouched());
    w.controller.resolveBudgetConflict(ConflictChoice::Cancel);
    CHECK(w.fileIsUntouched());

    CHECK_FALSE(w.controller.createBudget("Meal plan", "300.00", kNov15, kDec15));
    w.controller.resolveBudgetConflict(ConflictChoice::ChooseAnotherCategory);
    CHECK(w.fileIsUntouched());
    CHECK(w.display.conflicts.size() == 2);
}

TEST_CASE("Update existing saves the replacement through the store") {
    WiredWithStore w;
    REQUIRE_FALSE(w.controller.createBudget("Meal plan", "300.00", kNov15, kDec15));

    CHECK(w.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));

    CHECK(rawInt(w.tmp.path(), "SELECT COUNT(*) FROM budgets") == 1);
    CHECK(rawInt(w.tmp.path(), "SELECT amount_cents FROM budgets") == 30000);
    CHECK(rawRows(w.tmp.path(), "SELECT start_date, end_date FROM budgets").at(0) ==
          QVariantList({QStringLiteral("2026-11-15"), QStringLiteral("2026-12-15")}));
}
