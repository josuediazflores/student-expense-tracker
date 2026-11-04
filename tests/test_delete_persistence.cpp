// A delete is saved through the store the controller is wired to. Every test uses a temporary file. A delete must survive a
// reload with the right item gone and the order kept; Cancel, a bad row, a stale answer and
// a plain request must not write to the file at all.
#include "doctest.h"

#include "BudgetController.h"
#include "BudgetModel.h"
#include "BudgetStore.h"
#include "persistence_test_support.h"

#include <QStringList>

#include <functional>
#include <memory>

using namespace persistence_test;

namespace {
const QDate kNov1(2026, 11, 1);
const QDate kNov2(2026, 11, 2);
const QDate kNov3(2026, 11, 3);
const QDate kNov10(2026, 11, 10);
const QDate kDec1(2026, 12, 1);

const QString kBudgetRows =
    QStringLiteral("SELECT position, category, amount_cents, start_date, end_date FROM budgets"
                   " ORDER BY position");
const QString kExpenseRows =
    QStringLiteral("SELECT position, amount_dollars, date, category FROM expenses ORDER BY position");

// The app's wiring order: store, display, then loadSaved(). `seed` writes the first file
// with a plain store and model, as an earlier run of the app would have left it.
struct App {
    TempDir tmp;
    std::unique_ptr<BudgetStore> store;
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    explicit App(const std::function<void(BudgetModel &)> &seed = nullptr) {
        if (seed) {
            BudgetStore seedStore(tmp.path());
            BudgetModel seedModel;
            REQUIRE(seedStore.load(seedModel));
            seed(seedModel);
            REQUIRE(seedStore.save(seedModel));
        }
        store = std::make_unique<BudgetStore>(tmp.path());
        controller.setStore(store.get());
        controller.setDisplay(&display);
        REQUIRE(controller.loadSaved());
        display.screens.clear();
        display.messages.clear();
        display.events.clear();
    }

    // Ends this app's hold on the file (the store's lock) so a test can open it again.
    void closeStore() {
        controller.setStore(nullptr);
        store.reset();
    }

    // Overwrites the saved text of both tables, so any later save would visibly replace it.
    void markFile() {
        REQUIRE(rawExec(tmp.path(), {QStringLiteral("UPDATE budgets SET category = 'Marker'"),
                                     QStringLiteral("UPDATE expenses SET category = 'Marker'")}));
        marked = rawRows(tmp.path(), kBudgetRows) + rawRows(tmp.path(), kExpenseRows);
        REQUIRE_FALSE(marked.isEmpty());
    }

    bool fileIsUntouched() const {
        return marked == rawRows(tmp.path(), kBudgetRows) + rawRows(tmp.path(), kExpenseRows);
    }

    QVector<QVariantList> marked;
};

QStringList amounts(const BudgetModel &model) {
    QStringList result;
    for (const Expense &expense : model.expenses()) result.append(expense.amountCents.toString());
    return result;
}

QStringList categories(const BudgetModel &model) {
    QStringList result;
    for (const Budget &budget : model.budgets()) result.append(budget.category);
    return result;
}

// Food $100, Fun $50 and Books $20 over the same days.
void seedThreeBudgets(BudgetModel &model) {
    REQUIRE(model.addBudget({"Food", 10000, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Fun", 5000, kNov1, kDec1}));
    REQUIRE(model.addBudget({"Books", 2000, kNov1, kDec1}));
}

// Entered $1 Nov 1, $2 Nov 2, $3 Nov 3. Shown newest first: $3, $2, $1.
void seedThreeExpenses(BudgetModel &model) {
    REQUIRE(model.addExpense(QStringLiteral("1"), kNov1, QStringLiteral("Food")));
    REQUIRE(model.addExpense(QStringLiteral("2"), kNov2, QStringLiteral("Fun")));
    REQUIRE(model.addExpense(QStringLiteral("3"), kNov3, QStringLiteral("Books")));
}

// Huy's real file: Meal plan $50 and $250, Nov 1 to Dec 1, 2026, and a $100 expense.
void seedTwins(BudgetModel &model) {
    REQUIRE(model.restoreBudget({"Meal plan", 5000, kNov1, kDec1}));
    REQUIRE(model.restoreBudget({"Meal plan", 25000, kNov1, kDec1}));
    REQUIRE(model.addExpense(QStringLiteral("100"), kNov10, QStringLiteral("Meal plan")));
}
}

// ---- A delete survives a reload ------------------------------------------------------

TEST_CASE("a deleted budget is gone after a reload") {
    App app(seedThreeBudgets);
    REQUIRE(app.controller.requestDeleteBudget(1));

    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());
    CHECK(categories(reopened) == QStringList({"Food", "Books"}));
    CHECK(reopened.totalCents() == 12000);
}

TEST_CASE("a deleted expense is gone after a reload, and spending stays exact") {
    App app([](BudgetModel &model) {
        REQUIRE(model.addExpense(QStringLiteral("12.345"), kNov1));
        REQUIRE(model.addExpense(QStringLiteral("3"), kNov2));
    });
    REQUIRE(app.controller.requestDeleteExpense(1));  // shown newest first: Nov 1 is row 1

    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());
    CHECK(amounts(reopened) == QStringList({"300"}));
    CHECK(reopened.totalSpendingCents().toString() == QStringLiteral("300"));
}

TEST_CASE("deleting a middle budget keeps the order and renumbers the saved positions") {
    App app(seedThreeBudgets);
    REQUIRE(app.controller.requestDeleteBudget(1));
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());

    CHECK(categories(reopened) == QStringList({"Food", "Books"}));
    const QVector<QVariantList> rows = rawRows(app.tmp.path(), kBudgetRows);
    REQUIRE(rows.size() == 2);
    CHECK(rows.at(0).at(0).toInt() == 0);
    CHECK(rows.at(0).at(1).toString() == QStringLiteral("Food"));
    CHECK(rows.at(1).at(0).toInt() == 1);
    CHECK(rows.at(1).at(1).toString() == QStringLiteral("Books"));
}

TEST_CASE("deleting the middle shown expense keeps the order after a reload") {
    App app(seedThreeExpenses);
    REQUIRE(app.controller.requestDeleteExpense(1));  // shown: $3, $2, $1, so row 1 is $2
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());

    CHECK(amounts(reopened) == QStringList({"100", "300"}));
    REQUIRE(reopened.expenses().size() == 2);
    CHECK(reopened.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(reopened.expenses().at(0).date == kNov1);
    CHECK(reopened.expenses().at(1).category == QStringLiteral("Books"));
    CHECK(reopened.expenses().at(1).date == kNov3);
    const QVector<QVariantList> rows = rawRows(app.tmp.path(), kExpenseRows);
    REQUIRE(rows.size() == 2);
    CHECK(rows.at(0).at(0).toInt() == 0);
    CHECK(rows.at(1).at(0).toInt() == 1);
}

TEST_CASE("the right same-date expense is the one gone after a reload") {
    App app([](BudgetModel &model) {
        REQUIRE(model.addExpense(QStringLiteral("10"), kNov10));  // A
        REQUIRE(model.addExpense(QStringLiteral("20"), kNov10));  // B
        REQUIRE(model.addExpense(QStringLiteral("30"), kNov1));   // C, older, entered last
    });
    REQUIRE(app.controller.requestDeleteExpense(0));  // shown first: B
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());

    CHECK(amounts(reopened) == QStringList({"1000", "3000"}));
}

TEST_CASE("deleting the only budget and the only expense leaves an empty file that reloads") {
    App app([](BudgetModel &model) {
        REQUIRE(model.addBudget({"Food", 10000, kNov1, kDec1}));
        REQUIRE(model.addExpense(QStringLiteral("5"), kNov1));
    });
    REQUIRE(app.controller.requestDeleteBudget(0));
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    REQUIRE(app.controller.requestDeleteExpense(0));
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());

    CHECK(reopened.budgets().isEmpty());
    CHECK(reopened.expenses().isEmpty());
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM budgets") == 0);
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM expenses") == 0);
}

TEST_CASE("deleting a budget keeps every saved expense and its category") {
    App app([](BudgetModel &model) {
        seedThreeBudgets(model);
        seedThreeExpenses(model);
    });
    REQUIRE(app.controller.requestDeleteBudget(0));
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());

    CHECK(categories(reopened) == QStringList({"Fun", "Books"}));
    CHECK(amounts(reopened) == QStringList({"100", "200", "300"}));
    CHECK(reopened.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(reopened.totalSpendingCents().toString() == QStringLiteral("600"));
    CHECK(reopened.remainingBudgetCents().toString() == QStringLiteral("6400"));
}

// ---- Huy's duplicate Meal plan budget ------------------------------------------------

TEST_CASE("deleting the $50 Meal plan leaves exactly the $250 one, in the file and after a reload") {
    App app(seedTwins);
    REQUIRE(app.model.budgets().size() == 2);
    REQUIRE(app.model.budgets().at(0).amountCents == 5000);
    REQUIRE(app.controller.requestDeleteBudget(0));
    REQUIRE(app.display.deletions.size() == 1);
    CHECK(app.display.deletions.at(0) ==
          QStringLiteral("Delete the Meal plan budget of $50.00 for Nov 1, 2026 to Dec 1, 2026?"));

    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM budgets") == 1);
    CHECK(rawInt(app.tmp.path(), "SELECT amount_cents FROM budgets") == 25000);
    const BudgetModel reopened = reopen(app.tmp.path());
    REQUIRE(reopened.budgets().size() == 1);
    CHECK(reopened.budgets().at(0).category == QStringLiteral("Meal plan"));
    CHECK(reopened.budgets().at(0).amountCents == 25000);
    CHECK(reopened.budgets().at(0).startDate == kNov1);
    CHECK(reopened.budgets().at(0).endDate == kDec1);
    CHECK(reopened.expenses().size() == 1);
}

TEST_CASE("after deleting the $50 twin and reloading, the $100 expense can be deleted too") {
    App app(seedTwins);
    REQUIRE(app.controller.requestDeleteBudget(0));
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    REQUIRE(app.controller.loadSaved());

    REQUIRE(app.controller.requestDeleteExpense(0));
    REQUIRE(app.display.deletions.size() == 2);
    CHECK(app.display.deletions.at(1) ==
          QStringLiteral("Delete the Meal plan expense of $100.00 on Nov 10, 2026?"));
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());
    REQUIRE(reopened.budgets().size() == 1);
    CHECK(reopened.budgets().at(0).amountCents == 25000);
    CHECK(reopened.expenses().isEmpty());
}

// ---- Nothing is saved unless something was deleted -----------------------------------

TEST_CASE("Cancel on a budget or an expense saves nothing") {
    App app(seedTwins);
    app.markFile();

    REQUIRE(app.controller.requestDeleteBudget(0));
    CHECK(app.fileIsUntouched());
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Cancel));
    CHECK(app.fileIsUntouched());

    REQUIRE(app.controller.requestDeleteExpense(0));
    REQUIRE(app.controller.resolveDelete(DeleteChoice::Cancel));
    CHECK(app.fileIsUntouched());
    CHECK(app.model.budgets().size() == 2);
    CHECK(app.model.expenses().size() == 1);
}

TEST_CASE("a bad row and a stale answer save nothing") {
    App app(seedTwins);
    app.markFile();

    CHECK_FALSE(app.controller.requestDeleteBudget(-1));
    CHECK_FALSE(app.controller.requestDeleteBudget(2));
    CHECK_FALSE(app.controller.requestDeleteExpense(-1));
    CHECK_FALSE(app.controller.requestDeleteExpense(1));
    CHECK_FALSE(app.controller.resolveDelete(DeleteChoice::Delete));
    CHECK_FALSE(app.controller.resolveDelete(DeleteChoice::Cancel));

    CHECK(app.fileIsUntouched());
    CHECK(app.model.budgets().size() == 2);
    CHECK(app.model.expenses().size() == 1);
}

TEST_CASE("a refused second request saves nothing and Delete saves only the first item") {
    App app(seedTwins);
    REQUIRE(app.controller.requestDeleteBudget(0));
    REQUIRE_FALSE(app.controller.requestDeleteBudget(1));
    REQUIRE_FALSE(app.controller.requestDeleteExpense(0));

    REQUIRE(app.controller.resolveDelete(DeleteChoice::Delete));
    app.closeStore();

    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM budgets") == 1);
    CHECK(rawInt(app.tmp.path(), "SELECT amount_cents FROM budgets") == 25000);
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM expenses") == 1);
}

TEST_CASE("loadSaved clears a pending delete, so a later Delete saves nothing") {
    App app(seedTwins);
    app.markFile();
    REQUIRE(app.controller.requestDeleteBudget(0));
    REQUIRE(app.controller.loadSaved());
    REQUIRE(app.model.budgets().size() == 2);

    CHECK_FALSE(app.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(app.fileIsUntouched());
    CHECK(app.model.budgets().size() == 2);
    CHECK(app.model.expenses().size() == 1);
}

TEST_CASE("a pending delete and the duplicate-budget question clear each other with a store wired") {
    App app(seedTwins);
    app.markFile();
    REQUIRE(app.controller.requestDeleteBudget(1));

    // This clashes with both twins; the question replaces the pending delete.
    CHECK_FALSE(app.controller.createBudget("Meal plan", "300.00", kNov2, kNov3));
    CHECK_FALSE(app.controller.resolveDelete(DeleteChoice::Delete));

    CHECK(app.fileIsUntouched());
    CHECK(app.model.budgets().size() == 2);
}
