#include "BudgetStore.h"

#include "BudgetModel.h"
#include "DecimalCents.h"

#include <QDate>
#include <QDateTime>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

#include <functional>
#include <optional>

namespace {
const QString kDateFormat = QStringLiteral("yyyy-MM-dd");
constexpr int kSchemaVersion = 2;

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

std::optional<int> schemaVersion(QSqlDatabase &db, QString *error) {
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("PRAGMA user_version")) || !query.next()) {
        fail(error, query.lastError().text());
        return std::nullopt;
    }
    return query.value(0).toInt();
}

// For a new file (version 0). Plain CREATE TABLE (not IF NOT EXISTS) fails on an
// unversioned file that already has tables, instead of guessing what it holds.
bool createSchema(QSqlDatabase &db, QString *error) {
    // Columns are exactly INTEGER or TEXT: NUMERIC affinity would turn "12.345" into a float.
    // category is last and declared like the version 1 upgrade adds it, so both files match.
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
                                  " date TEXT NOT NULL,"
                                  " category TEXT NOT NULL DEFAULT '')"),
                   error) &&
               run(db, QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion), error);
    });
}

// The first free backup name: the plain one, else one stamped with the time, else that
// with a counter, so an existing file (even an empty one from a crash) is never replaced.
QString freeBackupName(const QString &databasePath) {
    const QString plain = databasePath + QStringLiteral(".v1-backup.sqlite");
    if (!QFile::exists(plain)) return plain;
    const QString stamped =
        databasePath + QStringLiteral(".v1-backup-") +
        QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
    QString candidate = stamped + QStringLiteral(".sqlite");
    for (int counter = 2; QFile::exists(candidate); ++counter)
        candidate = stamped + QStringLiteral("-%1.sqlite").arg(counter);
    return candidate;
}

// The upgrade is one-way and older builds refuse a version 2 file, so a version 1 copy is
// kept as the way back. The copy is written to a .tmp file and renamed, so a crash can never
// leave a half-written file at a real backup name.
bool backUpVersion1(QSqlDatabase &db, QString *error) {
    const QString backup = freeBackupName(db.databaseName());
    const QString temp = backup + QStringLiteral(".tmp");
    // A leftover from an earlier crash; VACUUM INTO refuses to write over an existing file.
    QFile::remove(temp);
    QSqlQuery query(db);
    if (query.prepare(QStringLiteral("VACUUM INTO ?"))) {
        query.bindValue(0, temp);
        if (query.exec() && QFile::rename(temp, backup)) return true;
    }
    fail(error, QStringLiteral("A backup of the saved data could not be made: ") +
                    query.lastError().text());
    QFile::remove(temp);
    return false;
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
        // Saved duplicates are kept as saved; the overlap rule only stops new ones.
        if (!model.restoreBudget(budget, &modelError)) return fail(error, rejected + modelError);
    }
    return !query.lastError().isValid() || fail(error, query.lastError().text());
}

bool readExpenses(QSqlDatabase &db, BudgetModel &model, QString *error) {
    QSqlQuery query(db);
    if (!query.exec(QStringLiteral(
            "SELECT amount_dollars, date, category FROM expenses ORDER BY position")))
        return fail(error, query.lastError().text());
    for (int row = 1; query.next(); ++row) {
        QString modelError;
        if (!model.addExpense(query.value(0).toString(), parseDate(query.value(1)),
                              query.value(2).toString(), &modelError))
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
            "INSERT INTO expenses(position, amount_dollars, date, category) VALUES(?, ?, ?, ?)")))
        return fail(error, insert.lastError().text());
    for (qsizetype i = 0; i < expenses.size(); ++i) {
        const Expense &expense = expenses.at(i);
        const std::optional<QString> date = dateText(expense.date);
        if (!date)
            return fail(error, QStringLiteral("An expense date cannot be saved as yyyy-MM-dd."));
        insert.bindValue(0, static_cast<qint64>(i));
        // Exact dollar text, so 1234.5 cents is stored as "12.345".
        insert.bindValue(1, expense.amountCents.toDollarsString());
        insert.bindValue(2, *date);
        // Qt binds a null QString as SQL NULL, so "uncategorized" is written as ''.
        insert.bindValue(3, expense.category.isNull() ? QStringLiteral("") : expense.category);
        if (!insert.exec()) return fail(error, insert.lastError().text());
    }
    return true;
}
}

BudgetStore::BudgetStore(const QString &databasePath)
    : lock_(databasePath + QStringLiteral(".lock")) {
    // A lock counts as stale only when its owner process is dead, never because it is old.
    lock_.setStaleLockTime(0);
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
    if (!lock_.isLocked() && !lock_.tryLock(0)) {
        switch (lock_.error()) {
        case QLockFile::LockFailedError:
            return fail(error, QStringLiteral("The budget is already open in another window."));
        case QLockFile::PermissionError:
            return fail(error, QStringLiteral("The budget lock file could not be created or accessed."));
        case QLockFile::UnknownError:
        case QLockFile::NoError:
            return fail(error, QStringLiteral("The budget lock could not be acquired."));
        }
        return fail(error, QStringLiteral("The budget lock could not be acquired."));
    }
    QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
    if (!ensureOpen(db, error)) return false;
    const std::optional<int> version = schemaVersion(db, error);
    if (!version) return false;

    // Replay into a fresh model so a rejected row leaves the caller's model untouched.
    BudgetModel saved;
    const auto replay = [&] {
        return readBudgets(db, saved, error) && readExpenses(db, saved, error);
    };
    bool replayed = false;
    if (*version == 0) {
        replayed = createSchema(db, error) && replay();
    } else if (*version == 1) {
        // The upgrade commits only if every row is accepted; otherwise the file stays version 1.
        replayed = backUpVersion1(db, error) && inTransaction(db, error, [&] {
            return run(db,
                       QStringLiteral("ALTER TABLE expenses"
                                      " ADD COLUMN category TEXT NOT NULL DEFAULT ''"),
                       error) &&
                   run(db, QStringLiteral("PRAGMA user_version = %1").arg(kSchemaVersion),
                       error) &&
                   replay();
        });
    } else if (*version == kSchemaVersion) {
        replayed = replay();
    } else {
        return fail(error, QStringLiteral("Saved data is from a newer version."));
    }
    if (!replayed) return false;
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
