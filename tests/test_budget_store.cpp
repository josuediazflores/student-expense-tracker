// Author: Huy Tran
// BudgetStore saves a BudgetModel to a SQLite file and loads it back. These tests pin the
// contract from docs/plans/mvc-mvp.md (Part 2): an exact round trip, a missing file as an
// empty start, and above all that a failed load can never let a later save overwrite the
// student's saved file.
#include "doctest.h"

#include "BudgetModel.h"
#include "BudgetStore.h"
#include "persistence_test_support.h"

#include <QFile>

using namespace persistence_test;

namespace {
const QString kBlocked =
    QStringLiteral("Saved data could not be loaded, so changes are not being saved.");
const QString kNewer = QStringLiteral("Saved data is from a newer version.");
const QString kLocked = QStringLiteral("The budget is already open in another window.");

// Two budgets (the later-sorting name first) and two expenses (the newer date first), so
// order bugs show up. 12.345 dollars is 1234.5 cents: a fraction of a cent.
void fillModel(BudgetModel &model) {
    REQUIRE(model.addBudget({"Zebra", 12345, QDate(2026, 1, 1), QDate(2026, 1, 31)}));
    REQUIRE(model.addBudget({"Apple", 500, QDate(2026, 11, 1), QDate(2026, 11, 30)}));
    REQUIRE(model.addExpense("12.345", QDate(2026, 3, 2)));
    REQUIRE(model.addExpense("0.5", QDate(2026, 1, 10)));
}

void checkFilled(const BudgetModel &model) {
    REQUIRE(model.budgets().size() == 2);
    CHECK(model.budgets().at(0).category == QStringLiteral("Zebra"));
    CHECK(model.budgets().at(0).amountCents == 12345);
    CHECK(model.budgets().at(0).startDate == QDate(2026, 1, 1));
    CHECK(model.budgets().at(0).endDate == QDate(2026, 1, 31));
    CHECK(model.budgets().at(1).category == QStringLiteral("Apple"));
    CHECK(model.budgets().at(1).amountCents == 500);
    CHECK(model.budgets().at(1).startDate == QDate(2026, 11, 1));
    CHECK(model.budgets().at(1).endDate == QDate(2026, 11, 30));

    REQUIRE(model.expenses().size() == 2);
    CHECK(model.expenses().at(0).amountCents.toString() == QStringLiteral("1234.5"));
    CHECK(model.expenses().at(0).date == QDate(2026, 3, 2));
    CHECK(model.expenses().at(1).amountCents.toString() == QStringLiteral("50"));
    CHECK(model.expenses().at(1).date == QDate(2026, 1, 10));

    CHECK(model.totalCents() == 12845);
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1284.5"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("11560.5"));
}

// Saves a filled model to `path` through a store that loaded first, as the app does.
void saveFilled(const QString &path) {
    BudgetStore store(path);
    BudgetModel model;
    REQUIRE(store.load(model));
    fillModel(model);
    REQUIRE(store.save(model));
}

// The three kinds of file the store must refuse to load. Each leaves the good rows
// "Food"/12.5 in place so a test can see whether they survive.
void writeNegativeBudgetFile(const QString &path) {
    REQUIRE(writeRawDatabase(path, 1,
                             {QStringLiteral("INSERT INTO budgets VALUES(1, 'Bad', -5,"
                                             " '2026-02-01', '2026-02-28')")}));
}
void writeBadExpenseFile(const QString &path) {
    REQUIRE(writeRawDatabase(path, 1,
                             {QStringLiteral("INSERT INTO expenses VALUES(1, 'abc', '2026-02-01')")}));
}

void checkOriginalRowsIntact(const QString &path, int budgetRows, int expenseRows) {
    CHECK(rawInt(path, "SELECT COUNT(*) FROM budgets") == budgetRows);
    CHECK(rawInt(path, "SELECT COUNT(*) FROM expenses") == expenseRows);
    const QVector<QVariantList> food =
        rawRows(path, "SELECT category, amount_cents FROM budgets WHERE position = 0");
    REQUIRE(food.size() == 1);
    CHECK(food.at(0).at(0).toString() == QStringLiteral("Food"));
    CHECK(food.at(0).at(1).toLongLong() == 10000);
    const QVector<QVariantList> twelve =
        rawRows(path, "SELECT amount_dollars FROM expenses WHERE position = 0");
    REQUIRE(twelve.size() == 1);
    CHECK(twelve.at(0).at(0).toString() == QStringLiteral("12.5"));
}
}

// ---- Round trip ----------------------------------------------------------------------

TEST_CASE("a saved model loads back into a fresh store and model with the same values") {
    TempDir tmp;
    saveFilled(tmp.path());

    BudgetStore freshStore(tmp.path());
    BudgetModel fresh;
    QString error;
    REQUIRE(freshStore.load(fresh, &error));

    CHECK(error.isEmpty());
    checkFilled(fresh);
}

TEST_CASE("a fractional-cent expense survives the round trip exactly") {
    TempDir tmp;
    saveFilled(tmp.path());

    BudgetStore store(tmp.path());
    BudgetModel loaded;
    REQUIRE(store.load(loaded));

    // 12.345 dollars is exactly 1234.5 cents, with nothing rounded to a double.
    REQUIRE(DecimalCents::fromDecimal(QStringLiteral("1234.5")).has_value());
    CHECK(loaded.expenses().at(0).amountCents ==
          DecimalCents::fromDecimal(QStringLiteral("1234.5")).value());
}

TEST_CASE("digits beyond what a double can hold survive the round trip") {
    TempDir tmp;
    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        REQUIRE(model.addBudget({"Big", 9223372036854775807LL, QDate(2026, 1, 1), QDate(2026, 1, 1)}));
        REQUIRE(model.addExpense("1234567890123456.123456789", QDate(2026, 1, 1)));
        REQUIRE(store.save(model));
    }

    BudgetStore store(tmp.path());
    BudgetModel loaded;
    REQUIRE(store.load(loaded));
    REQUIRE(loaded.budgets().size() == 1);
    CHECK(loaded.budgets().at(0).amountCents == 9223372036854775807LL);
    REQUIRE(loaded.expenses().size() == 1);
    CHECK(loaded.expenses().at(0).amountCents.toString() ==
          QStringLiteral("123456789012345612.3456789"));
}

TEST_CASE("budgets and expenses come back in the order they were added") {
    TempDir tmp;
    saveFilled(tmp.path());

    BudgetStore store(tmp.path());
    BudgetModel loaded;
    REQUIRE(store.load(loaded));

    REQUIRE(loaded.budgets().size() == 2);
    CHECK(loaded.budgets().at(0).category == QStringLiteral("Zebra"));
    CHECK(loaded.budgets().at(1).category == QStringLiteral("Apple"));
    REQUIRE(loaded.expenses().size() == 2);
    CHECK(loaded.expenses().at(0).date == QDate(2026, 3, 2));
    CHECK(loaded.expenses().at(1).date == QDate(2026, 1, 10));
}

TEST_CASE("saving twice replaces the saved rows instead of adding to them") {
    TempDir tmp;
    BudgetStore store(tmp.path());
    BudgetModel model;
    REQUIRE(store.load(model));
    fillModel(model);
    REQUIRE(store.save(model));
    REQUIRE(store.save(model));

    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets") == 2);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM expenses") == 2);
}

TEST_CASE("a smaller model saved over a larger one leaves only the smaller one") {
    TempDir tmp;
    saveFilled(tmp.path());

    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        BudgetModel smaller;
        REQUIRE(smaller.addBudget({"Only", 100, QDate(2026, 5, 1), QDate(2026, 5, 2)}));
        REQUIRE(store.save(smaller));
    }

    BudgetStore reader(tmp.path());
    BudgetModel loaded;
    REQUIRE(reader.load(loaded));
    REQUIRE(loaded.budgets().size() == 1);
    CHECK(loaded.budgets().at(0).category == QStringLiteral("Only"));
    CHECK(loaded.expenses().isEmpty());
}

TEST_CASE("loading replaces whatever was in the model") {
    TempDir tmp;
    saveFilled(tmp.path());

    BudgetStore store(tmp.path());
    BudgetModel model;
    REQUIRE(model.addBudget({"Stale", 1, QDate(2026, 1, 1), QDate(2026, 1, 1)}));
    REQUIRE(store.load(model));

    checkFilled(model);
}

// ---- How values are stored -----------------------------------------------------------

TEST_CASE("the stored expense amount is the exact text 12.345, not a number") {
    TempDir tmp;
    saveFilled(tmp.path());

    const QVector<QVariantList> rows = rawRows(
        tmp.path(), "SELECT amount_dollars, typeof(amount_dollars) FROM expenses ORDER BY position");
    REQUIRE(rows.size() == 2);
    CHECK(rows.at(0).at(0).toString() == QStringLiteral("12.345"));
    CHECK(rows.at(0).at(1).toString() == QStringLiteral("text"));
}

TEST_CASE("budget cents are stored as integers and dates as ISO text") {
    TempDir tmp;
    saveFilled(tmp.path());

    const QVector<QVariantList> rows = rawRows(
        tmp.path(),
        "SELECT category, amount_cents, typeof(amount_cents), start_date, end_date"
        " FROM budgets ORDER BY position");
    REQUIRE(rows.size() == 2);
    CHECK(rows.at(0).at(0).toString() == QStringLiteral("Zebra"));
    CHECK(rows.at(0).at(1).toLongLong() == 12345);
    CHECK(rows.at(0).at(2).toString() == QStringLiteral("integer"));
    CHECK(rows.at(0).at(3).toString() == QStringLiteral("2026-01-01"));
    CHECK(rows.at(0).at(4).toString() == QStringLiteral("2026-01-31"));

    const QVector<QVariantList> expenseDates =
        rawRows(tmp.path(), "SELECT date FROM expenses ORDER BY position");
    REQUIRE(expenseDates.size() == 2);
    CHECK(expenseDates.at(0).at(0).toString() == QStringLiteral("2026-03-02"));
    CHECK(expenseDates.at(1).at(0).toString() == QStringLiteral("2026-01-10"));
}

TEST_CASE("a new database is marked schema version 1") {
    TempDir tmp;
    saveFilled(tmp.path());

    CHECK(rawInt(tmp.path(), "PRAGMA user_version") == 1);
}

// ---- Missing file --------------------------------------------------------------------

TEST_CASE("a missing file loads as an empty model and succeeds") {
    TempDir tmp;
    BudgetStore store(tmp.path());
    BudgetModel model;
    QString error;

    CHECK(store.load(model, &error));

    CHECK(error.isEmpty());
    CHECK(model.budgets().isEmpty());
    CHECK(model.expenses().isEmpty());
    CHECK(model.totalCents() == 0);
}

TEST_CASE("after loading a missing file, saving works") {
    TempDir tmp;
    {
        BudgetStore store(tmp.path());
        BudgetModel model;
        REQUIRE(store.load(model));
        fillModel(model);

        QString error;
        CHECK(store.save(model, &error));
        CHECK(error.isEmpty());
    }

    BudgetStore reader(tmp.path());
    BudgetModel loaded;
    REQUIRE(reader.load(loaded));
    checkFilled(loaded);
}

TEST_CASE("a file that cannot be opened fails the load with the driver's text") {
    TempDir tmp;
    BudgetStore store(tmp.path("no/such/folder/budget.sqlite"));
    BudgetModel model;
    QString error;

    CHECK_FALSE(store.load(model, &error));

    CHECK_FALSE(error.isEmpty());
}

// ---- A failed load changes nothing ---------------------------------------------------

TEST_CASE("a failed load leaves the model unchanged") {
    TempDir tmp;
    writeNegativeBudgetFile(tmp.path());
    BudgetStore store(tmp.path());
    BudgetModel model;
    REQUIRE(model.addBudget({"Mine", 777, QDate(2026, 4, 1), QDate(2026, 4, 30)}));
    REQUIRE(model.addExpense("3.21", QDate(2026, 4, 2)));
    QString error;

    CHECK_FALSE(store.load(model, &error));

    CHECK_FALSE(error.isEmpty());
    REQUIRE(model.budgets().size() == 1);
    CHECK(model.budgets().at(0).category == QStringLiteral("Mine"));
    CHECK(model.budgets().at(0).amountCents == 777);
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).amountCents.toString() == QStringLiteral("321"));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("321"));
}

TEST_CASE("a bad row after good rows loads nothing at all, not the good rows") {
    TempDir tmp;
    writeBadExpenseFile(tmp.path());  // Food and 12.5 are valid, the third row is "abc"
    BudgetStore store(tmp.path());
    BudgetModel model;

    CHECK_FALSE(store.load(model));

    CHECK(model.budgets().isEmpty());
    CHECK(model.expenses().isEmpty());
}

TEST_CASE("a file that is not a database fails the load and is not changed") {
    TempDir tmp;
    QFile garbage(tmp.path());
    REQUIRE(garbage.open(QIODevice::WriteOnly));
    garbage.write("this is a student's notes, not a SQLite file, and it is long enough to check");
    garbage.close();
    BudgetStore store(tmp.path());
    BudgetModel model;
    QString error;

    CHECK_FALSE(store.load(model, &error));
    CHECK_FALSE(error.isEmpty());

    BudgetModel filled;
    fillModel(filled);
    QString saveError;
    CHECK_FALSE(store.save(filled, &saveError));
    CHECK(saveError == kBlocked);

    QFile again(tmp.path());
    REQUIRE(again.open(QIODevice::ReadOnly));
    CHECK(again.readAll() ==
          QByteArray("this is a student's notes, not a SQLite file, and it is long enough to check"));
}

// ---- A failed load blocks saving (the data-loss guard) -------------------------------

TEST_CASE("after a failed load, save is refused and the file's original rows are still there") {
    TempDir tmp;
    writeNegativeBudgetFile(tmp.path());  // Food, 12.5 and a bad -5 budget
    BudgetStore store(tmp.path());
    BudgetModel model;
    REQUIRE_FALSE(store.load(model));

    // The student keeps using the app: the empty model gets new entries.
    REQUIRE(model.addBudget({"New", 100, QDate(2026, 6, 1), QDate(2026, 6, 2)}));
    REQUIRE(model.addExpense("1", QDate(2026, 6, 1)));
    QString error;
    CHECK_FALSE(store.save(model, &error));

    CHECK(error == kBlocked);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets") == 2);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM expenses") == 1);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets WHERE category = 'Bad' AND amount_cents = -5") == 1);
    checkOriginalRowsIntact(tmp.path(), 2, 1);
}

TEST_CASE("a bad expense row also blocks saving and keeps the saved rows") {
    TempDir tmp;
    writeBadExpenseFile(tmp.path());
    BudgetStore store(tmp.path());
    BudgetModel model;
    REQUIRE_FALSE(store.load(model));

    BudgetModel empty;
    QString error;
    CHECK_FALSE(store.save(empty, &error));

    CHECK(error == kBlocked);
    checkOriginalRowsIntact(tmp.path(), 1, 2);
}

TEST_CASE("a file from a newer version fails the load with the version message") {
    TempDir tmp;
    REQUIRE(writeRawDatabase(tmp.path(), 2, {}));
    BudgetStore store(tmp.path());
    BudgetModel model;
    QString error;

    CHECK_FALSE(store.load(model, &error));

    CHECK(error == kNewer);
    CHECK(model.budgets().isEmpty());
}

TEST_CASE("saving stays blocked after a newer-version load and the file is untouched") {
    TempDir tmp;
    REQUIRE(writeRawDatabase(tmp.path(), 2, {}));
    BudgetStore store(tmp.path());
    BudgetModel model;
    REQUIRE_FALSE(store.load(model));
    fillModel(model);
    QString error;

    CHECK_FALSE(store.save(model, &error));

    CHECK(error == kBlocked);
    CHECK(rawInt(tmp.path(), "PRAGMA user_version") == 2);
    checkOriginalRowsIntact(tmp.path(), 1, 1);
}

TEST_CASE("save before any load is refused and writes nothing") {
    TempDir tmp;
    saveFilled(tmp.path());
    BudgetStore store(tmp.path());  // never loaded
    BudgetModel empty;
    QString error;

    CHECK_FALSE(store.save(empty, &error));

    CHECK(error == kBlocked);
    BudgetStore reader(tmp.path());
    BudgetModel loaded;
    REQUIRE(reader.load(loaded));
    checkFilled(loaded);
}

TEST_CASE("save before any load is refused on a path with no file") {
    TempDir tmp;
    BudgetStore store(tmp.path());
    BudgetModel model;
    fillModel(model);
    QString error;

    CHECK_FALSE(store.save(model, &error));

    CHECK(error == kBlocked);
    BudgetStore reader(tmp.path());
    BudgetModel loaded;
    REQUIRE(reader.load(loaded));
    CHECK(loaded.budgets().isEmpty());
}

TEST_CASE("a successful load after a failed one allows saving again") {
    TempDir tmp;
    writeNegativeBudgetFile(tmp.path());
    BudgetStore store(tmp.path());
    BudgetModel model;
    REQUIRE_FALSE(store.load(model));
    // The student (or support) repairs the file.
    REQUIRE(rawExec(tmp.path(), {QStringLiteral("DELETE FROM budgets WHERE amount_cents < 0")}));
    REQUIRE(store.load(model));
    REQUIRE(model.addExpense("2", QDate(2026, 6, 1)));

    CHECK(store.save(model));

    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM expenses") == 2);
}

// ---- Several stores in one process ---------------------------------------------------

TEST_CASE("stores on different files in one process do not interfere") {
    TempDir tmp;
    const QString pathA = tmp.path("a.sqlite");
    const QString pathB = tmp.path("b.sqlite");
    BudgetStore storeA(pathA);
    BudgetStore storeB(pathB);
    BudgetModel modelA;
    BudgetModel modelB;
    REQUIRE(storeA.load(modelA));
    REQUIRE(storeB.load(modelB));
    REQUIRE(modelA.addBudget({"OnlyA", 111, QDate(2026, 1, 1), QDate(2026, 1, 2)}));
    REQUIRE(modelB.addBudget({"OnlyB", 222, QDate(2026, 2, 1), QDate(2026, 2, 2)}));
    REQUIRE(modelB.addExpense("9.99", QDate(2026, 2, 1)));

    REQUIRE(storeA.save(modelA));
    REQUIRE(storeB.save(modelB));

    BudgetStore readerA(pathA);
    BudgetStore readerB(pathB);
    BudgetModel loadedA;
    BudgetModel loadedB;
    REQUIRE(readerA.load(loadedA));
    REQUIRE(readerB.load(loadedB));
    REQUIRE(loadedA.budgets().size() == 1);
    CHECK(loadedA.budgets().at(0).category == QStringLiteral("OnlyA"));
    CHECK(loadedA.expenses().isEmpty());
    REQUIRE(loadedB.budgets().size() == 1);
    CHECK(loadedB.budgets().at(0).category == QStringLiteral("OnlyB"));
    CHECK(loadedB.expenses().size() == 1);
}

TEST_CASE("a store keeps working after another store is destroyed") {
    TempDir tmp;
    {
        BudgetStore survivor(tmp.path("survivor.sqlite"));
        BudgetModel model;
        REQUIRE(survivor.load(model));
        {
            BudgetStore shortLived(tmp.path("short.sqlite"));
            BudgetModel other;
            REQUIRE(shortLived.load(other));
            REQUIRE(shortLived.save(other));
        }
        fillModel(model);

        CHECK(survivor.save(model));
    }

    BudgetStore reader(tmp.path("survivor.sqlite"));
    BudgetModel loaded;
    REQUIRE(reader.load(loaded));
    checkFilled(loaded);
}

TEST_CASE("a load blocked on one store does not block another store's save") {
    TempDir tmp;
    writeNegativeBudgetFile(tmp.path("bad.sqlite"));
    BudgetStore bad(tmp.path("bad.sqlite"));
    BudgetStore good(tmp.path("good.sqlite"));
    BudgetModel badModel;
    BudgetModel goodModel;
    REQUIRE_FALSE(bad.load(badModel));
    REQUIRE(good.load(goodModel));
    fillModel(goodModel);

    CHECK(good.save(goodModel));
    CHECK_FALSE(bad.save(goodModel));
}

// ---- One window per file (D14) -------------------------------------------------------

TEST_CASE("a second store on a file that is already open fails to load with the lock message") {
    TempDir tmp;
    BudgetStore first(tmp.path());
    BudgetModel firstModel;
    REQUIRE(first.load(firstModel));
    fillModel(firstModel);
    REQUIRE(first.save(firstModel));

    BudgetStore second(tmp.path());
    BudgetModel secondModel;
    QString error;
    CHECK_FALSE(second.load(secondModel, &error));
    CHECK(error == kLocked);
    CHECK(secondModel.budgets().isEmpty());
    CHECK(secondModel.expenses().isEmpty());
}

TEST_CASE("a store refused by the lock cannot save and the first store's rows stay intact") {
    TempDir tmp;
    BudgetStore first(tmp.path());
    BudgetModel firstModel;
    REQUIRE(first.load(firstModel));
    fillModel(firstModel);
    REQUIRE(first.save(firstModel));

    BudgetStore second(tmp.path());
    BudgetModel secondModel;
    CHECK_FALSE(second.load(secondModel));
    // The second window's student keeps entering data on a blank screen.
    REQUIRE(secondModel.addBudget({"Intruder", 1, QDate(2026, 7, 1), QDate(2026, 7, 2)}));
    QString error;
    CHECK_FALSE(second.save(secondModel, &error));
    CHECK(error == kBlocked);

    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets") == 2);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM expenses") == 2);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets WHERE category = 'Intruder'") == 0);
    CHECK(rawInt(tmp.path(), "SELECT COUNT(*) FROM budgets WHERE category = 'Zebra'") == 1);
}

TEST_CASE("the lock is released when the first store is destroyed") {
    TempDir tmp;
    {
        BudgetStore first(tmp.path());
        BudgetModel model;
        REQUIRE(first.load(model));
        fillModel(model);
        REQUIRE(first.save(model));
    }

    BudgetStore next(tmp.path());
    BudgetModel loaded;
    QString error;
    CHECK(next.load(loaded, &error));

    CHECK(error.isEmpty());
    checkFilled(loaded);
}
