// Author: Huy Tran
// The controller saves after every successful action and loads saved data on start-up.
// A fake display records what the user would see,
// and a second store plus a raw SQL look at the file prove what was really written.
#include "doctest.h"

#include "BudgetController.h"
#include "BudgetModel.h"
#include "BudgetStore.h"
#include "persistence_test_support.h"

#include <memory>
#include <vector>

using namespace persistence_test;

namespace {
const QString kBlocked =
    QStringLiteral("Saved data could not be loaded, so changes are not being saved.");
const QString kNotSaved = QStringLiteral("Your change was not saved: ");
const QString kLoadFailed = QStringLiteral("Saved data could not be loaded: ");
const QString kLocked = QStringLiteral("The budget is already open in another window.");
const QDate kJan1(2026, 1, 1);
const QDate kJan31(2026, 1, 31);

// The app's wiring order: store first, then the display, then loadSaved().
struct App {
    TempDir tmp;
    std::unique_ptr<BudgetStore> store = std::make_unique<BudgetStore>(tmp.path());
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    App() {
        controller.setStore(store.get());
        controller.setDisplay(&display);
        REQUIRE(controller.loadSaved());
    }

    // Ends this app's hold on the file (the store's lock), as closing its window would, so
    // a test can open the same file again.
    void closeStore() {
        controller.setStore(nullptr);
        store.reset();
    }
};

// Marks the saved budget row so a later save would visibly overwrite it.
void markFile(const QString &path) {
    REQUIRE(rawExec(path, {QStringLiteral("UPDATE budgets SET category = 'Marker'")}));
}

bool isMarked(const QString &path) {
    return rawInt(path, "SELECT COUNT(*) FROM budgets WHERE category = 'Marker'") == 1;
}

// A trigger that makes the next INSERT fail, so a save fails after a successful load.
void blockInserts(const QString &path, const QString &table) {
    REQUIRE(rawExec(path, {QStringLiteral("CREATE TRIGGER block_%1 BEFORE INSERT ON %1 "
                                          "BEGIN SELECT RAISE(ABORT, 'disk is full'); END")
                               .arg(table)}));
}
}

// ---- Successful actions save ---------------------------------------------------------

TEST_CASE("creating a budget saves it so a reopened app has it") {
    App app;

    REQUIRE(app.controller.createBudget("Groceries", "250.00", kJan1, kJan31));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());
    REQUIRE(reopened.budgets().size() == 1);
    CHECK(reopened.budgets().at(0).category == QStringLiteral("Groceries"));
    CHECK(reopened.budgets().at(0).amountCents == 25000);
    CHECK(reopened.budgets().at(0).startDate == kJan1);
    CHECK(reopened.budgets().at(0).endDate == kJan31);
}

TEST_CASE("adding an expense saves its exact amount and date") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));

    REQUIRE(app.controller.addExpense("12.345", QDate(2026, 1, 9)));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());
    REQUIRE(reopened.expenses().size() == 1);
    CHECK(reopened.expenses().at(0).amountCents.toString() == QStringLiteral("1234.5"));
    CHECK(reopened.expenses().at(0).date == QDate(2026, 1, 9));
    CHECK(reopened.totalSpendingCents().toString() == QStringLiteral("1234.5"));
}

TEST_CASE("updating a budget amount saves the new amount") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    REQUIRE(app.controller.createBudget("Fun", "50", kJan1, kJan31));

    REQUIRE(app.controller.updateBudgetAmount(0, "300"));
    app.closeStore();

    const BudgetModel reopened = reopen(app.tmp.path());
    REQUIRE(reopened.budgets().size() == 2);
    CHECK(reopened.budgets().at(0).amountCents == 30000);
    CHECK(reopened.budgets().at(1).amountCents == 5000);
}

TEST_CASE("a reopened controller shows the same screen the first one ended with") {
    App first;
    REQUIRE(first.controller.createBudget("Zebra", "10", kJan1, kJan31));
    REQUIRE(first.controller.createBudget("Apple", "20", kJan1, kJan31));
    REQUIRE(first.controller.addExpense("12.345", QDate(2026, 1, 9)));
    REQUIRE(first.controller.addExpense("1", QDate(2026, 1, 3)));
    const BudgetScreen expected = first.controller.screen();
    first.closeStore();

    BudgetStore store(first.tmp.path());
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setStore(&store);
    controller.setDisplay(&display);
    REQUIRE(controller.loadSaved());

    const BudgetScreen actual = controller.screen();
    CHECK(actual.totalBudget == expected.totalBudget);
    CHECK(actual.totalSpending == QStringLiteral("$13.345"));
    CHECK(actual.totalSpending == expected.totalSpending);
    CHECK(actual.remaining == expected.remaining);
    REQUIRE(actual.budgets.size() == 2);
    CHECK(actual.budgets.at(0).category == QStringLiteral("Zebra"));
    CHECK(actual.budgets.at(1).category == QStringLiteral("Apple"));
    REQUIRE(actual.expenses.size() == 2);
    CHECK(actual.expenses.at(0).amount == QStringLiteral("$12.345"));
    CHECK(actual.expenses.at(1).amount == QStringLiteral("$1.00"));
}

TEST_CASE("a successful action with a store still shows the screen then the success message") {
    App app;
    app.display.events.clear();
    app.display.messages.clear();

    REQUIRE(app.controller.createBudget("Groceries", "250", kJan1, kJan31));

    CHECK(app.display.events == std::vector<char>{'S', 'M'});
    REQUIRE(app.display.messages.size() == 1);
    CHECK(app.display.messages.at(0).first == QStringLiteral("Budget added."));
    CHECK(app.display.messages.at(0).second == false);
}

TEST_CASE("actions are saved even when no display is attached") {
    TempDir tmp;
    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        BudgetController controller(model);
        controller.setStore(&store);
        REQUIRE(controller.loadSaved());

        CHECK(controller.createBudget("Food", "100", kJan1, kJan31));
    }

    const BudgetModel reopened = reopen(tmp.path());
    REQUIRE(reopened.budgets().size() == 1);
    CHECK(reopened.budgets().at(0).category == QStringLiteral("Food"));
}

// ---- Failed actions do not save ------------------------------------------------------

TEST_CASE("a failed create does not save") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    markFile(app.tmp.path());

    CHECK_FALSE(app.controller.createBudget("Fun", "12.345", kJan1, kJan31));

    CHECK(isMarked(app.tmp.path()));
}

TEST_CASE("a failed update does not save") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    markFile(app.tmp.path());

    CHECK_FALSE(app.controller.updateBudgetAmount(5, "300"));

    CHECK(isMarked(app.tmp.path()));
}

TEST_CASE("a failed expense does not save") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    markFile(app.tmp.path());

    CHECK_FALSE(app.controller.addExpense("abc", QDate(2026, 1, 9)));

    CHECK(isMarked(app.tmp.path()));
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM expenses") == 0);
}

TEST_CASE("after setStore(nullptr) actions no longer save") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    markFile(app.tmp.path());
    app.controller.setStore(nullptr);

    CHECK(app.controller.createBudget("Fun", "5", kJan1, kJan31));

    CHECK(isMarked(app.tmp.path()));
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM budgets") == 1);
}

// ---- loadSaved -----------------------------------------------------------------------

TEST_CASE("loadSaved pushes the saved data as one screen and no message") {
    TempDir tmp;
    {
        BudgetStore seed(tmp.path());
        BudgetModel seeded;
        REQUIRE(seed.load(seeded));
        REQUIRE(seeded.addBudget({"Food", 10000, kJan1, kJan31}));
        REQUIRE(seeded.addExpense("12.5", QDate(2026, 1, 5)));
        REQUIRE(seed.save(seeded));
    }
    BudgetStore store(tmp.path());
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setStore(&store);
    controller.setDisplay(&display);
    const size_t screensBefore = display.screens.size();

    CHECK(controller.loadSaved());

    CHECK(display.screens.size() == screensBefore + 1);
    CHECK(display.messages.empty());
    const BudgetScreen &screen = display.screens.back();
    CHECK(screen.totalBudget == QStringLiteral("$100.00"));
    CHECK(screen.totalSpending == QStringLiteral("$12.50"));
    REQUIRE(screen.budgets.size() == 1);
    CHECK(screen.budgets.at(0).category == QStringLiteral("Food"));
    REQUIRE(screen.expenses.size() == 1);
    CHECK(screen.expenses.at(0).date == QStringLiteral("Jan 5, 2026"));
}

TEST_CASE("loadSaved on a missing file succeeds and shows an empty screen with no message") {
    TempDir tmp;
    BudgetStore store(tmp.path());
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setStore(&store);
    controller.setDisplay(&display);
    const size_t screensBefore = display.screens.size();

    CHECK(controller.loadSaved());

    CHECK(display.screens.size() == screensBefore + 1);
    CHECK(display.messages.empty());
    CHECK(display.screens.back().budgets.isEmpty());
    CHECK(display.screens.back().totalBudget == QStringLiteral("$0.00"));
}

TEST_CASE("loadSaved on a corrupt file returns false and shows a load-failed error") {
    TempDir tmp;
    REQUIRE(writeRawDatabase(tmp.path(), 1,
                             {QStringLiteral("INSERT INTO budgets VALUES(1, 'Bad', -5,"
                                             " '2026-02-01', '2026-02-28')")}));
    BudgetStore store(tmp.path());
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setStore(&store);
    controller.setDisplay(&display);

    CHECK_FALSE(controller.loadSaved());

    REQUIRE(display.messages.size() == 1);
    CHECK(display.messages.at(0).second == true);
    CHECK(display.messages.at(0).first.startsWith(kLoadFailed));
    CHECK(display.messages.at(0).first.size() > kLoadFailed.size());
    CHECK(model.budgets().isEmpty());
}

TEST_CASE("loadSaved on a newer-version file names the version problem") {
    TempDir tmp;
    REQUIRE(writeRawDatabase(tmp.path(), 3, {}));
    BudgetStore store(tmp.path());
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setStore(&store);
    controller.setDisplay(&display);

    CHECK_FALSE(controller.loadSaved());

    REQUIRE(display.messages.size() == 1);
    CHECK(display.messages.at(0).first ==
          kLoadFailed + QStringLiteral("Saved data is from a newer version."));
    CHECK(display.messages.at(0).second == true);
}

TEST_CASE("loadSaved with no store returns true and changes nothing") {
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setDisplay(&display);
    const size_t screensBefore = display.screens.size();

    CHECK(controller.loadSaved());

    CHECK(display.screens.size() == screensBefore);
    CHECK(display.messages.empty());
}

TEST_CASE("without a store an action behaves as before") {
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setDisplay(&display);
    REQUIRE(controller.loadSaved());

    CHECK(controller.createBudget("Food", "100", kJan1, kJan31));

    CHECK(display.events == std::vector<char>{'S', 'S', 'M'});
    REQUIRE(display.messages.size() == 1);
    CHECK(display.messages.at(0).first == QStringLiteral("Budget added."));
    CHECK(display.messages.at(0).second == false);
}

// ---- A failed load can never overwrite the saved file --------------------------------

TEST_CASE("after a failed load, actions cannot overwrite the saved file") {
    TempDir tmp;
    REQUIRE(writeRawDatabase(tmp.path(), 1,
                             {QStringLiteral("INSERT INTO budgets VALUES(1, 'Bad', -5,"
                                             " '2026-02-01', '2026-02-28')")}));
    BudgetStore store(tmp.path());
    BudgetModel model;
    BudgetController controller(model);
    FakeDisplay display;
    controller.setStore(&store);
    controller.setDisplay(&display);
    REQUIRE_FALSE(controller.loadSaved());
    display.messages.clear();
    display.events.clear();

    // The student keeps going on a blank screen.
    CHECK(controller.createBudget("New", "5", kJan1, kJan31));
    CHECK(controller.addExpense("1", QDate(2026, 1, 2)));

    // The original rows, including the bad one, are all still there.
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets") == 2);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM expenses") == 1);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets WHERE category = 'Food'") == 1);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets WHERE category = 'New'") == 0);
    // And the student is told, not left thinking it was saved.
    REQUIRE(display.messages.size() == 2);
    CHECK(display.messages.at(0).first == kNotSaved + kBlocked);
    CHECK(display.messages.at(0).second == true);
    CHECK(display.messages.at(1).first == kNotSaved + kBlocked);
}

// ---- A save that fails after a successful action -------------------------------------

TEST_CASE("when the save fails the action still returns true and the screen is pushed") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    blockInserts(app.tmp.path(), "budgets");
    app.display.events.clear();
    app.display.messages.clear();
    app.display.screens.clear();

    CHECK(app.controller.createBudget("Fun", "50", kJan1, kJan31));

    REQUIRE(app.display.screens.size() == 1);
    CHECK(app.display.screens.at(0).budgets.size() == 2);
    CHECK(app.model.budgets().size() == 2);
}

TEST_CASE("when the save fails the message is an error that starts with the not-saved prefix") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    blockInserts(app.tmp.path(), "budgets");
    app.display.events.clear();
    app.display.messages.clear();

    CHECK(app.controller.createBudget("Fun", "50", kJan1, kJan31));

    REQUIRE(app.display.messages.size() == 1);
    CHECK(app.display.messages.at(0).second == true);
    CHECK(app.display.messages.at(0).first.startsWith(kNotSaved));
    CHECK(app.display.messages.at(0).first.size() > kNotSaved.size());
    CHECK(app.display.messages.at(0).first != QStringLiteral("Budget added."));
    CHECK(app.display.events == std::vector<char>{'S', 'M'});
}

TEST_CASE("a save that fails part-way rolls back and leaves the earlier saved data") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    REQUIRE(app.controller.addExpense("7", QDate(2026, 1, 2)));
    blockInserts(app.tmp.path(), "expenses");

    CHECK(app.controller.addExpense("8", QDate(2026, 1, 3)));

    // Budgets were rewritten first and then the expense insert failed: nothing half-applied.
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM budgets") == 1);
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM expenses") == 1);
    CHECK(rawRows(app.tmp.path(), "SELECT amount_dollars FROM expenses").at(0).at(0).toString() ==
          QStringLiteral("7"));
}

TEST_CASE("a failed expense save reports the not-saved error too") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));
    blockInserts(app.tmp.path(), "expenses");
    app.display.messages.clear();

    CHECK(app.controller.addExpense("8", QDate(2026, 1, 3)));

    REQUIRE(app.display.messages.size() == 1);
    CHECK(app.display.messages.at(0).second == true);
    CHECK(app.display.messages.at(0).first.startsWith(kNotSaved));
}

// ---- One window per file (D14) -------------------------------------------------------

TEST_CASE("loadSaved in a second window fails with the lock message and cannot overwrite the file") {
    App app;
    REQUIRE(app.controller.createBudget("Food", "100", kJan1, kJan31));

    BudgetStore secondStore(app.tmp.path());
    BudgetModel secondModel;
    BudgetController secondController(secondModel);
    FakeDisplay secondDisplay;
    secondController.setStore(&secondStore);
    secondController.setDisplay(&secondDisplay);

    CHECK_FALSE(secondController.loadSaved());

    REQUIRE(secondDisplay.messages.size() == 1);
    CHECK(secondDisplay.messages.at(0).second == true);
    CHECK(secondDisplay.messages.at(0).first.startsWith(kLoadFailed));
    CHECK(secondDisplay.messages.at(0).first.contains(kLocked));
    CHECK(secondModel.budgets().isEmpty());

    // The second window's student keeps going; nothing they do may reach the file.
    secondController.createBudget("Intruder", "5", kJan1, kJan31);
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM budgets") == 1);
    CHECK(rawInt(app.tmp.path(), "SELECT COUNT(*) FROM budgets WHERE category = 'Food'") == 1);
}
