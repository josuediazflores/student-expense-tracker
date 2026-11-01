#include "BudgetStore.h"

#include "BudgetModel.h"
#include "DecimalCents.h"

#include <QDate>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <functional>
#include <optional>

namespace {
const QString kDateFormat = QStringLiteral("yyyy-MM-dd");
constexpr int kSchemaVersion = 1;

bool fail(QString *error, const QString &text) {
    if (error) *error = text;
    return false;
}

bool ensureOpen(QSqlDatabase &db, QString *error) {
    return db.isOpen() || db.open() || fail(error, db.lastError().text());
}

// Runs one statement that takes no bound values.
bool run(QSqlDatabase &db, const QString &sql, QString *error) {
    QSqlQuery query(db);
    return query.exec(sql) || fail(error, query.lastError().text());
}

// Runs `work` as one transaction and rolls back if any step or the commit fails.
bool inTransaction(QSqlDatabase &db, QString *error, const std::function<bool()> &work) {
    if (!db.transaction()) return fail(error, db.lastError().text());
    if (work() && (db.commit() || fail(error, db.lastError().text()))) return true;
    // The first failure is the one worth reporting; SQLite also discards an open
    // transaction when the connection closes.
    db.rollback();
    return false;
}

// Version 0 is a new file: create the tables. Plain CREATE TABLE (not IF NOT EXISTS) fails
// on an unversioned file that already has tables, instead of guessing what it holds.
bool ensureSchema(QSqlDatabase &db, QString *error) {
    int version = 0;
    {
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next())
            return fail(error, query.lastError().text());
        version = query.value(0).toInt();
    }
    if (version == kSchemaVersion) return true;
    if (version != 0) return fail(error, QStringLiteral("Saved data is from a newer version."));

    // Columns are exactly INTEGER or TEXT: NUMERIC affinity would turn "12.345" into a float.
    return inTransaction(db, error, [&] {
        return run(db,
                   QStringLiteral("CREATE TABLE budgets("
                                  "position INTEGER PRIMARY KEY,"
                                  " category TEXT NOT NULL CHECK(length(category) > 0),"
                                  " amount_cents INTEGER NOT NULL CHECK(amount_cents >= 0),"
                                  " start_date TEXT NOT NULL,"
                                  " end_date TEXT NOT NULL)"),
                   error) &&
               run(db,
                   QStringLiteral("CREATE TABLE expenses("
                                  "position INTEGER PRIMARY KEY,"
                                  " amount_dollars TEXT NOT NULL"
                                  " CHECK(typeof(amount_dollars) = 'text'),"
                                  " date TEXT NOT NULL)"),
                   error) &&
               run(db, QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion), error);
    });
}

// An invalid QDate for text that is not exactly yyyy-MM-dd; the Model rejects it.
QDate parseDate(const QVariant &value) {
    return QDate::fromString(value.toString(), kDateFormat);
}

// Years outside 0..9999 would not read back as the same date.
std::optional<QString> dateText(const QDate &date) {
    const QString text = date.toString(kDateFormat);
    if (!date.isValid() || QDate::fromString(text, kDateFormat) != date) return std::nullopt;
    return text;
}

bool readBudgets(QSqlDatabase &db, BudgetModel &model, QString *error) {
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT category, amount_cents, start_date, end_date"
                                   " FROM budgets ORDER BY position")))
        return fail(error, query.lastError().text());
    for (int row = 1; query.next(); ++row) {
        const QString rejected = QStringLiteral("Saved budget %1 was rejected. ").arg(row);
        // A REAL here would be silently truncated by toLongLong, so only integers pass.
        const QVariant cents = query.value(1);
        if (cents.typeId() != QMetaType::LongLong)
            return fail(error, rejected + QStringLiteral("Its amount is not whole cents."));
        const Budget budget{query.value(0).toString(), cents.toLongLong(),
                            parseDate(query.value(2)), parseDate(query.value(3))};
        QString modelError;
        if (!model.addBudget(budget, &modelError)) return fail(error, rejected + modelError);
    }
    return !query.lastError().isValid() || fail(error, query.lastError().text());
}

bool readExpenses(QSqlDatabase &db, BudgetModel &model, QString *error) {
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT amount_dollars, date FROM expenses ORDER BY position")))
        return fail(error, query.lastError().text());
    for (int row = 1; query.next(); ++row) {
        QString modelError;
        if (!model.addExpense(query.value(0).toString(), parseDate(query.value(1)), &modelError))
            return fail(error,
                        QStringLiteral("Saved expense %1 was rejected. ").arg(row) + modelError);
    }
    return !query.lastError().isValid() || fail(error, query.lastError().text());
}

bool writeBudgets(QSqlDatabase &db, const QVector<Budget> &budgets, QString *error) {
    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral("INSERT INTO budgets(position, category, amount_cents,"
                                       " start_date, end_date) VALUES(?, ?, ?, ?, ?)")))
        return fail(error, insert.lastError().text());
    for (qsizetype i = 0; i < budgets.size(); ++i) {
        const Budget &budget = budgets.at(i);
        const std::optional<QString> start = dateText(budget.startDate);
        const std::optional<QString> end = dateText(budget.endDate);
        if (!start || !end)
            return fail(error, QStringLiteral("A budget date cannot be saved as yyyy-MM-dd."));
        insert.bindValue(0, static_cast<qint64>(i));
        insert.bindValue(1, budget.category);
        insert.bindValue(2, budget.amountCents);
        insert.bindValue(3, *start);
        insert.bindValue(4, *end);
        if (!insert.exec()) return fail(error, insert.lastError().text());
    }
    return true;
}

bool writeExpenses(QSqlDatabase &db, const QVector<Expense> &expenses, QString *error) {
    QSqlQuery insert(db);
    if (!insert.prepare(QStringLiteral(
            "INSERT INTO expenses(position, amount_dollars, date) VALUES(?, ?, ?)")))
        return fail(error, insert.lastError().text());
    for (qsizetype i = 0; i < expenses.size(); ++i) {
        const Expense &expense = expenses.at(i);
        const std::optional<QString> date = dateText(expense.date);
        if (!date)
            return fail(error, QStringLiteral("An expense date cannot be saved as yyyy-MM-dd."));
        // Exact dollar text, so 1234.5 cents is stored as "12.345". A DecimalCents string
        // is always valid decimal text, so value() cannot throw.
        const QString dollars =
            DecimalCents::fromDecimal(expense.amountCents.toString() + QStringLiteral("e-2"))
                .value()
                .toString();
        insert.bindValue(0, static_cast<qint64>(i));
        insert.bindValue(1, dollars);
        insert.bindValue(2, *date);
        if (!insert.exec()) return fail(error, insert.lastError().text());
    }
    return true;
}
}

BudgetStore::BudgetStore(const QString &databasePath) {
    static int connectionCount = 0;
    connectionName_ = QStringLiteral("budget_store_%1").arg(++connectionCount);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    db.setDatabaseName(databasePath);
}

BudgetStore::~BudgetStore() {
    {
        // removeDatabase needs every handle to the connection gone first.
        QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
        db.close();
    }
    QSqlDatabase::removeDatabase(connectionName_);
}

bool BudgetStore::load(BudgetModel &model, QString *error) {
    loaded_ = false;
    QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
    if (!ensureOpen(db, error) || !ensureSchema(db, error)) return false;

    // Replay into a fresh model so a rejected row leaves the caller's model untouched.
    BudgetModel saved;
    if (!readBudgets(db, saved, error) || !readExpenses(db, saved, error)) return false;
    model = saved;
    loaded_ = true;
    if (error) error->clear();
    return true;
}

bool BudgetStore::save(const BudgetModel &model, QString *error) {
    if (!loaded_) {
        return fail(error, QStringLiteral(
                               "Saved data could not be loaded, so changes are not being saved."));
    }
    QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
    if (!ensureOpen(db, error)) return false;
    const bool saved = inTransaction(db, error, [&] {
        return run(db, QStringLiteral("DELETE FROM budgets"), error) &&
               run(db, QStringLiteral("DELETE FROM expenses"), error) &&
               writeBudgets(db, model.budgets(), error) &&
               writeExpenses(db, model.expenses(), error);
    });
    if (saved && error) error->clear();
    return saved;
}
