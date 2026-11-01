// Part 3, task 12: saving the expense category as schema version 2, and upgrading a version 1
// file safely (docs/plans/mvc-mvp.md, D22 and Part 3 plan review items 1 to 4). The version 1
// file here is built with raw SQL, the way the app wrote it before this change, and holds the
// two identical "Meal plan" rows that Huy's real file has.
#include "doctest.h"

#include "BudgetController.h"
#include "BudgetModel.h"
#include "BudgetStore.h"
#include "persistence_test_support.h"

#include <QFile>

#include <memory>

using namespace persistence_test;

namespace {
const QString kNewer = QStringLiteral("Saved data is from a newer version.");
const QString kBlocked =
    QStringLiteral("Saved data could not be loaded, so changes are not being saved.");
const QString kQuestion = QStringLiteral(
    "You already have a Meal plan budget of $250.00 for Nov 1, 2026 to Dec 1, 2026.");
const QDate kNov1(2026, 11, 1);
const QDate kNov15(2026, 11, 15);
const QDate kDec1(2026, 12, 1);
const QDate kDec15(2026, 12, 15);
const QDate kMar2(2026, 3, 2);

const QString kBudgetRows =
    QStringLiteral("SELECT position, category, amount_cents, start_date, end_date FROM budgets"
                   " ORDER BY position");
const QString kExpenseRows =
    QStringLiteral("SELECT position, amount_dollars, date FROM expenses ORDER BY position");

QString backupPath(const QString &path) { return path + QStringLiteral(".v1-backup.sqlite"); }

// The version 1 schema exactly as BudgetStore wrote it before this change.
QStringList v1Schema() {
    return {QStringLiteral("CREATE TABLE budgets("
                           "position INTEGER PRIMARY KEY,"
                           " category TEXT NOT NULL CHECK(length(category) > 0),"
                           " amount_cents INTEGER NOT NULL CHECK(amount_cents >= 0),"
                           " start_date TEXT NOT NULL,"
                           " end_date TEXT NOT NULL)"),
            QStringLiteral("CREATE TABLE expenses("
                           "position INTEGER PRIMARY KEY,"
                           " amount_dollars TEXT NOT NULL CHECK(typeof(amount_dollars) = 'text'),"
                           " date TEXT NOT NULL)")};
}

// Two identical Meal plan rows and one fractional expense, plus whatever `extra` adds.
bool writeV1(const QString &path, const QStringList &extra = {}) {
    QStringList statements = v1Schema();
    statements += {
        QStringLiteral("INSERT INTO budgets VALUES(0, 'Meal plan', 25000, '2026-11-01', '2026-12-01')"),
        QStringLiteral("INSERT INTO budgets VALUES(1, 'Meal plan', 25000, '2026-11-01', '2026-12-01')"),
        QStringLiteral("INSERT INTO expenses VALUES(0, '12.345', '2026-03-02')"),
    };
    statements += extra;
    statements.append(QStringLiteral("PRAGMA user_version = 1"));
    return rawExec(path, statements);
}

int userVersion(const QString &path) { return rawInt(path, "PRAGMA user_version"); }

// The app's wiring order: store, display, then loadSaved().
struct App {
    TempDir tmp;
    std::unique_ptr<BudgetStore> store;
    BudgetModel model;
    BudgetController controller{model};
    FakeDisplay display;

    explicit App(bool seedVersion1 = false) {
        if (seedVersion1) REQUIRE(writeV1(tmp.path()));
        store = std::make_unique<BudgetStore>(tmp.path());
        controller.setStore(store.get());
        controller.setDisplay(&display);
        REQUIRE(controller.loadSaved());
    }

    void closeStore() {
        controller.setStore(nullptr);
        store.reset();
    }
};

BudgetModel reopen(const QString &path) {
    BudgetStore store(path);
    BudgetModel model;
    REQUIRE(store.load(model));
    return model;
}
}

// ---- A new file is version 2 ---------------------------------------------------------

TEST_CASE("a new file is version 2, has the category column, and makes no backup") {
    TempDir tmp;
    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        REQUIRE(store.save(model));
    }

    CHECK(userVersion(tmp.path()) == 2);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM pragma_table_info('expenses')"
                             " WHERE name = 'category'") == 1);
    CHECK_FALSE(QFile::exists(backupPath(tmp.path())));
}

TEST_CASE("categories survive saving twice and reloading, uncategorized ones stay empty") {
    TempDir tmp;
    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        REQUIRE(model.addExpense(QStringLiteral("12.345"), kMar2, QStringLiteral("Food")));
        REQUIRE(model.addExpense(QStringLiteral("3"), kMar2));
        REQUIRE(model.addExpense(QStringLiteral("1.25"), QDate(2026, 3, 5), QStringLiteral("Fun")));
        REQUIRE(store.save(model));
        REQUIRE(store.save(model));
    }

    const BudgetModel reloaded = reopen(tmp.path());

    REQUIRE(reloaded.expenses().size() == 3);
    CHECK(reloaded.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(reloaded.expenses().at(0).amountCents.toString() == QStringLiteral("1234.5"));
    CHECK(reloaded.expenses().at(1).category.isEmpty());
    CHECK(reloaded.expenses().at(2).category == QStringLiteral("Fun"));
    const QVector<QVariantList> stored =
        rawRows(tmp.path(), "SELECT category FROM expenses ORDER BY position");
    REQUIRE(stored.size() == 3);
    CHECK(stored.at(0).at(0).toString() == QStringLiteral("Food"));
    CHECK(stored.at(1).at(0).toString() == QStringLiteral(""));
    CHECK(stored.at(2).at(0).toString() == QStringLiteral("Fun"));
}

TEST_CASE("a later save by the controller keeps the categories saved earlier") {
    App first;
    REQUIRE(first.controller.addExpense("12.50", kMar2, "Food"));
    REQUIRE(first.controller.addExpense("3", kMar2, "Fun"));
    first.closeStore();

    BudgetStore store(first.tmp.path());
    BudgetModel model;
    BudgetController controller(model);
    controller.setStore(&store);
    REQUIRE(controller.loadSaved());
    REQUIRE(controller.addExpense("1", kMar2));
    controller.setStore(nullptr);

    const QVector<QVariantList> stored =
        rawRows(first.tmp.path(), "SELECT category FROM expenses ORDER BY position");
    REQUIRE(stored.size() == 3);
    CHECK(stored.at(0).at(0).toString() == QStringLiteral("Food"));
    CHECK(stored.at(1).at(0).toString() == QStringLiteral("Fun"));
    CHECK(stored.at(2).at(0).toString() == QStringLiteral(""));
}

// ---- Upgrading a version 1 file ------------------------------------------------------

TEST_CASE("a version 1 file with duplicate budgets loads with both rows and an uncategorized expense") {
    TempDir tmp;
    REQUIRE(writeV1(tmp.path()));
    BudgetStore store(tmp.path());
    BudgetModel model;
    QString error = QStringLiteral("Previous error");

    REQUIRE(store.load(model, &error));

    CHECK(error.isEmpty());
    REQUIRE(model.budgets().size() == 2);
    for (const Budget &budget : model.budgets()) {
        CHECK(budget.category == QStringLiteral("Meal plan"));
        CHECK(budget.amountCents == 25000);
        CHECK(budget.startDate == kNov1);
        CHECK(budget.endDate == kDec1);
    }
    CHECK(model.totalCents() == 50000);
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).amountCents.toString() == QStringLiteral("1234.5"));
    CHECK(model.expenses().at(0).date == kMar2);
    CHECK(model.expenses().at(0).category.isEmpty());
}

TEST_CASE("loading a version 1 file upgrades it to 2, changes no row, and keeps a version 1 backup") {
    TempDir tmp;
    REQUIRE(writeV1(tmp.path()));
    const QVector<QVariantList> budgetsBefore = rawRows(tmp.path(), kBudgetRows);
    const QVector<QVariantList> expensesBefore = rawRows(tmp.path(), kExpenseRows);
    REQUIRE(budgetsBefore.size() == 2);
    REQUIRE(expensesBefore.size() == 1);
    BudgetStore store(tmp.path());
    BudgetModel model;

    REQUIRE(store.load(model));

    CHECK(userVersion(tmp.path()) == 2);
    CHECK(rawRows(tmp.path(), kBudgetRows) == budgetsBefore);
    CHECK(rawRows(tmp.path(), kExpenseRows) == expensesBefore);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM expenses WHERE category = ''") == 1);

    REQUIRE(QFile::exists(backupPath(tmp.path())));
    CHECK(userVersion(backupPath(tmp.path())) == 1);
    CHECK(rawRows(backupPath(tmp.path()), kBudgetRows) == budgetsBefore);
    CHECK(rawRows(backupPath(tmp.path()), kExpenseRows) == expensesBefore);
    CHECK(rawInt(backupPath(tmp.path()),
                 "SELECT COUNT(*) FROM pragma_table_info('expenses')") == 3);
}

TEST_CASE("after the upgrade saving works, duplicates are kept, and the backup stays version 1") {
    TempDir tmp;
    REQUIRE(writeV1(tmp.path()));
    const QVector<QVariantList> expensesBefore = rawRows(tmp.path(), kExpenseRows);
    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        REQUIRE(model.addExpense(QStringLiteral("5"), QDate(2026, 3, 3), QStringLiteral("Food")));
        QString error;
        CHECK(store.save(model, &error));
        CHECK(error.isEmpty());
    }

    BudgetStore again(tmp.path());
    BudgetModel reloaded;
    REQUIRE(again.load(reloaded));

    CHECK(userVersion(tmp.path()) == 2);
    REQUIRE(reloaded.budgets().size() == 2);
    CHECK(reloaded.budgets().at(0).amountCents == 25000);
    CHECK(reloaded.budgets().at(1).amountCents == 25000);
    REQUIRE(reloaded.expenses().size() == 2);
    CHECK(reloaded.expenses().at(0).category.isEmpty());
    CHECK(reloaded.expenses().at(1).category == QStringLiteral("Food"));
    REQUIRE(QFile::exists(backupPath(tmp.path())));
    CHECK(userVersion(backupPath(tmp.path())) == 1);
    CHECK(rawRows(backupPath(tmp.path()), kExpenseRows) == expensesBefore);
}

TEST_CASE("an existing backup is never overwritten by a later upgrade") {
    TempDir tmp;
    // An earlier backup, with different contents from the file about to be upgraded.
    REQUIRE(writeRawDatabase(backupPath(tmp.path()), 1, {}));
    const QVector<QVariantList> backupBefore = rawRows(backupPath(tmp.path()), kBudgetRows);
    REQUIRE(backupBefore.size() == 1);
    REQUIRE(writeV1(tmp.path()));
    BudgetStore store(tmp.path());
    BudgetModel model;

    REQUIRE(store.load(model));

    CHECK(userVersion(tmp.path()) == 2);
    CHECK(model.budgets().size() == 2);
    CHECK(rawRows(backupPath(tmp.path()), kBudgetRows) == backupBefore);
    CHECK(rawInt(backupPath(tmp.path()), "SELECT COUNT(*) FROM budgets WHERE category = 'Food'") == 1);
}

TEST_CASE("a version 1 file with a row the Model rejects fails to load and stays version 1") {
    const QStringList badRows = {
        // End date before start date.
        QStringLiteral("INSERT INTO budgets VALUES(2, 'Meal plan', 100, '2026-12-01', '2026-11-01')"),
        // Not a number.
        QStringLiteral("INSERT INTO expenses VALUES(1, 'abc', '2026-03-03')"),
    };
    for (const QString &bad : badRows) {
        TempDir tmp;
        REQUIRE(writeV1(tmp.path(), {bad}));
        const QVector<QVariantList> budgetsBefore = rawRows(tmp.path(), kBudgetRows);
        const QVector<QVariantList> expensesBefore = rawRows(tmp.path(), kExpenseRows);
        BudgetStore store(tmp.path());
        BudgetModel model;
        QString error;

        CHECK_FALSE(store.load(model, &error));

        CHECK_FALSE(error.isEmpty());
        CHECK(model.budgets().isEmpty());
        CHECK(userVersion(tmp.path()) == 1);
        CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM pragma_table_info('expenses')") == 3);
        CHECK(rawRows(tmp.path(), kBudgetRows) == budgetsBefore);
        CHECK(rawRows(tmp.path(), kExpenseRows) == expensesBefore);

        // The failed file is protected from a later save, as for any failed load.
        QString saveError;
        CHECK_FALSE(store.save(model, &saveError));
        CHECK(saveError == kBlocked);
        CHECK(userVersion(tmp.path()) == 1);
        CHECK(rawRows(tmp.path(), kBudgetRows) == budgetsBefore);
    }
}

TEST_CASE("an upgraded file and a new file have the same expenses table shape") {
    TempDir upgraded;
    REQUIRE(writeV1(upgraded.path()));
    {
        BudgetStore store(upgraded.path());
        BudgetModel model;
        REQUIRE(store.load(model));
    }
    TempDir fresh;
    {
        BudgetStore store(fresh.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        REQUIRE(store.save(model));
    }

    const QVector<QVariantList> upgradedShape = rawRows(upgraded.path(), "PRAGMA table_info(expenses)");
    const QVector<QVariantList> freshShape = rawRows(fresh.path(), "PRAGMA table_info(expenses)");

    CHECK(upgradedShape == freshShape);
    REQUIRE(freshShape.size() == 4);
    // Columns are cid, name, type, notnull, dflt_value, pk; category is last.
    CHECK(freshShape.at(3).at(1).toString() == QStringLiteral("category"));
    CHECK(freshShape.at(3).at(2).toString() == QStringLiteral("TEXT"));
    CHECK(freshShape.at(3).at(3).toInt() == 1);
    CHECK(freshShape.at(3).at(4).toString() == QStringLiteral("''"));
}

TEST_CASE("a version 3 file fails to load with the newer-version message and is left alone") {
    TempDir tmp;
    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        REQUIRE(model.addBudget({"Meal plan", 25000, kNov1, kDec1}));
        REQUIRE(model.addExpense(QStringLiteral("5"), kMar2, QStringLiteral("Food")));
        REQUIRE(store.save(model));
    }
    REQUIRE(rawExec(tmp.path(), {QStringLiteral("PRAGMA user_version = 3")}));
    BudgetStore store(tmp.path());
    BudgetModel model;
    QString error;

    CHECK_FALSE(store.load(model, &error));
    CHECK(error == kNewer);
    CHECK_FALSE(store.save(model, &error));
    CHECK(error == kBlocked);

    CHECK(userVersion(tmp.path()) == 3);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets") == 1);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM expenses WHERE category = 'Food'") == 1);
    CHECK_FALSE(QFile::exists(backupPath(tmp.path())));
}

// ---- Through the controller ----------------------------------------------------------

TEST_CASE("the controller opens a version 1 file with duplicates and shows both rows") {
    App app(true);

    const BudgetScreen screen = app.controller.screen();
    REQUIRE(screen.budgets.size() == 2);
    CHECK(screen.budgets.at(0).total == QStringLiteral("$250.00"));
    CHECK(screen.budgets.at(1).total == QStringLiteral("$250.00"));
    CHECK(screen.totalBudget == QStringLiteral("$500.00"));
    REQUIRE(screen.expenses.size() == 1);
    CHECK(screen.expenses.at(0).amount == QStringLiteral("$12.345"));
    CHECK(screen.expenses.at(0).category == QStringLiteral("Uncategorized"));
    CHECK(app.display.messages.empty());
}

TEST_CASE("on a file that already holds duplicates, a new duplicate asks and Update replaces the first row") {
    App app(true);

    CHECK_FALSE(app.controller.createBudget("Meal plan", "300.00", kNov15, kDec15));
    REQUIRE(app.display.conflicts.size() == 1);
    CHECK(app.display.conflicts.at(0) == kQuestion);
    CHECK(app.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));
    app.closeStore();

    const BudgetModel reloaded = reopen(app.tmp.path());
    REQUIRE(reloaded.budgets().size() == 2);
    CHECK(reloaded.budgets().at(0).amountCents == 30000);
    CHECK(reloaded.budgets().at(0).startDate == kNov15);
    CHECK(reloaded.budgets().at(0).endDate == kDec15);
    CHECK(reloaded.budgets().at(1).amountCents == 25000);
    CHECK(reloaded.budgets().at(1).startDate == kNov1);
    CHECK(reloaded.expenses().size() == 1);
}

TEST_CASE("an update chosen from the conflict prompt survives a reload") {
    App app;
    REQUIRE(app.controller.createBudget("Meal plan", "250.00", kNov1, kDec1));
    REQUIRE_FALSE(app.controller.createBudget("Meal plan", "300.00", kNov15, kDec15));
    REQUIRE(app.controller.resolveBudgetConflict(ConflictChoice::UpdateExisting));
    app.closeStore();

    const BudgetModel reloaded = reopen(app.tmp.path());

    REQUIRE(reloaded.budgets().size() == 1);
    CHECK(reloaded.budgets().at(0).category == QStringLiteral("Meal plan"));
    CHECK(reloaded.budgets().at(0).amountCents == 30000);
    CHECK(reloaded.budgets().at(0).startDate == kNov15);
    CHECK(reloaded.budgets().at(0).endDate == kDec15);
}

TEST_CASE("an expense category added through the controller survives a reload") {
    App app;
    REQUIRE(app.controller.addExpense("12.50", kMar2, " Food "));
    REQUIRE(app.controller.addExpense("3", kMar2));
    app.closeStore();

    const BudgetModel reloaded = reopen(app.tmp.path());

    REQUIRE(reloaded.expenses().size() == 2);
    CHECK(reloaded.expenses().at(0).category == QStringLiteral("Food"));
    CHECK(reloaded.expenses().at(1).category.isEmpty());
}
