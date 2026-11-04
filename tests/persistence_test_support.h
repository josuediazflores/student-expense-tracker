// Author: Huy Tran
// Shared helpers for the persistence tests: a QCoreApplication (the SQLite driver plugin
// needs one), raw SQL access to a database file that bypasses BudgetStore, and a fake
// display. Everything is inline so several test files can include this header.
#pragma once

#include "BudgetDisplay.h"
#include "BudgetModel.h"
#include "BudgetStore.h"
#include "doctest.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>
#include <QVariantList>
#include <QVector>

#include <functional>
#include <utility>
#include <vector>

namespace persistence_test {

// One application object for the whole test binary, created on first use.
inline void ensureApp() {
    static int argc = 1;
    static char name[] = "test_build";
    static char *argv[] = {name, nullptr};
    static QCoreApplication app(argc, argv);
    (void)app;
}

// A temporary directory that holds database files.
struct TempDir {
    QTemporaryDir dir;
    TempDir() { ensureApp(); }
    QString path(const QString &file = QStringLiteral("budget.sqlite")) const {
        return dir.filePath(file);
    }
};

// Runs `work` on a direct connection to the file, then closes and removes it.
inline void withRawDb(const QString &path, const std::function<void(QSqlDatabase &)> &work) {
    ensureApp();
    static int counter = 0;
    const QString name = QStringLiteral("raw_test_connection_%1").arg(++counter);
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
        db.setDatabaseName(path);
        if (db.open()) work(db);
        db.close();
    }
    QSqlDatabase::removeDatabase(name);
}

// Executes statements; returns false if the file could not be opened or one failed.
inline bool rawExec(const QString &path, const QStringList &statements) {
    bool ok = true;
    bool opened = false;
    withRawDb(path, [&](QSqlDatabase &db) {
        opened = true;
        for (const QString &sql : statements) {
            QSqlQuery query(db);
            if (!query.exec(sql)) ok = false;
        }
    });
    return opened && ok;
}

// All rows of a SELECT, each as a list of column values.
inline QVector<QVariantList> rawRows(const QString &path, const QString &sql) {
    QVector<QVariantList> rows;
    withRawDb(path, [&](QSqlDatabase &db) {
        QSqlQuery query(db);
        if (!query.exec(sql)) return;
        const int columns = query.record().count();
        while (query.next()) {
            QVariantList row;
            for (int i = 0; i < columns; ++i) row.append(query.value(i));
            rows.append(row);
        }
    });
    return rows;
}

inline int rawInt(const QString &path, const QString &sql) {
    const QVector<QVariantList> rows = rawRows(path, sql);
    return rows.isEmpty() ? -1 : rows.at(0).at(0).toInt();
}

// A database a student's earlier run could have left behind: the real schema, one good
// budget ("Food", 10000 cents), one good expense ("12.5"), and then whatever `extra`
// adds. The CHECK constraints are left out so that bad rows can be written.
inline bool writeRawDatabase(const QString &path, int userVersion, const QStringList &extra) {
    QStringList statements = {
        QStringLiteral("CREATE TABLE budgets(position INTEGER PRIMARY KEY, category TEXT NOT NULL,"
                       " amount_cents INTEGER NOT NULL, start_date TEXT NOT NULL,"
                       " end_date TEXT NOT NULL)"),
        QStringLiteral("CREATE TABLE expenses(position INTEGER PRIMARY KEY,"
                       " amount_dollars TEXT NOT NULL, date TEXT NOT NULL)"),
        QStringLiteral("INSERT INTO budgets VALUES(0, 'Food', 10000, '2026-01-01', '2026-01-31')"),
        QStringLiteral("INSERT INTO expenses VALUES(0, '12.5', '2026-01-05')"),
    };
    statements += extra;
    statements.append(QStringLiteral("PRAGMA user_version = %1").arg(userVersion));
    return rawExec(path, statements);
}

// Fake display: records everything the controller pushes, in order.
class FakeDisplay : public BudgetDisplay {
public:
    void showScreen(const BudgetScreen &screen) override {
        screens.push_back(screen);
        events.push_back('S');
    }
    void showMessage(const QString &text, bool isError) override {
        messages.emplace_back(text, isError);
        events.push_back('M');
    }
    void askBudgetConflict(const QString &message) override {
        conflicts.push_back(message);
        events.push_back('Q');
    }
    void askDeleteConfirmation(const QString &message) override {
        deletions.push_back(message);
        events.push_back('D');
    }

    std::vector<BudgetScreen> screens;
    std::vector<std::pair<QString, bool>> messages;
    std::vector<QString> conflicts;  // the text of each duplicate-budget question
    std::vector<QString> deletions;  // the text of each delete confirmation question
    // 'S' for a screen, 'M' for a message, 'Q' for a duplicate-budget question, 'D' for a
    // delete confirmation question, in call order
    std::vector<char> events;
};

// Reads the file with a fresh store and model, like reopening the application.
inline BudgetModel reopen(const QString &path) {
    BudgetStore store(path);
    BudgetModel model;
    REQUIRE(store.load(model));
    return model;
}

}  // namespace persistence_test
