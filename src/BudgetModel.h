#pragma once

#include "DecimalCents.h"

#include <QDate>
#include <QString>
#include <QVector>

#include <cstddef>
#include <optional>
#include <type_traits>

struct Budget {
    QString category;
    qint64 amountCents;
    QDate startDate;
    QDate endDate;

    double amountPerDay() const;
};

struct Expense {
    DecimalCents amountCents;  // exact, may hold a fraction of a cent
    QDate date;
    QString category;  // trimmed; empty means uncategorized (saved before categories existed)
};

class BudgetModel {
public:
    // Rejects a budget whose category (trimmed, any case) already has a budget on any of
    // the same days, and sets *conflictIndex to the first such budget; otherwise to -1.
    bool addBudget(const Budget &budget, QString *error = nullptr,
                   qsizetype *conflictIndex = nullptr);
    // Same rules as addBudget, but only rejects an overlap the old budget did not already have.
    bool replaceBudget(qsizetype index, const Budget &budget, QString *error = nullptr);
    // For the store's load only: keeps saved data that predates the overlap rule, so it
    // applies every addBudget rule except that one.
    bool restoreBudget(const Budget &budget, QString *error = nullptr);
    const QVector<Budget> &budgets() const { return budgets_; }
    qint64 totalCents() const;
    bool updateBudgetAmount(qsizetype index, qint64 amountCents, QString *error = nullptr);
    // Typed budget text becomes whole cents here so no caller needs a double.
    static std::optional<qint64> parseBudgetAmount(const QString &amountDollars,
                                                   QString *error = nullptr);

    // Dollar text avoids precision already lost by upstream floating-point math.
    bool addExpense(const QString &amountDollars, QString *error = nullptr);
    bool addExpense(const char *amountDollars, QString *error = nullptr);
    bool addExpense(std::nullptr_t, QString *error = nullptr);
    // The overloads above record today's date; these record the given one.
    bool addExpense(const QString &amountDollars, const QDate &date, QString *error = nullptr);
    bool addExpense(const char *amountDollars, const QDate &date, QString *error = nullptr);
    bool addExpense(std::nullptr_t, const QDate &date, QString *error = nullptr);
    // The overloads above record no category.
    bool addExpense(const QString &amountDollars, const QDate &date, const QString &category,
                    QString *error = nullptr);

private:
    template <typename Amount>
    static constexpr bool isNonTextAmount =
        !std::is_same_v<std::decay_t<Amount>, QString> &&
        !std::is_convertible_v<std::decay_t<Amount>, const char *> &&
        !std::is_same_v<std::decay_t<Amount>, std::nullptr_t>;

public:
    // Reject numeric inputs before an implicit conversion can lose precision or units.
    template <typename Amount, std::enable_if_t<isNonTextAmount<Amount>, int> = 0>
    bool addExpense(const Amount &amount, QString *error = nullptr) {
        return addExpense(amount, QDate::currentDate(), error);
    }

    template <typename Amount, std::enable_if_t<isNonTextAmount<Amount>, int> = 0>
    bool addExpense(const Amount &amount, const QDate &date, QString *error = nullptr) {
        return addExpense(amount, date, QString(), error);
    }

    template <typename Amount, std::enable_if_t<isNonTextAmount<Amount>, int> = 0>
    bool addExpense(const Amount &, const QDate &, const QString &, QString *error = nullptr) {
        if (error) *error = QStringLiteral("Enter the expense as decimal text in dollars.");
        return false;
    }

    const QVector<Expense> &expenses() const { return expenses_; }  // entry order
    const DecimalCents &totalSpendingCents() const { return spendingCents_; }
    DecimalCents remainingBudgetCents() const;

private:
    // The rules every budget must pass; otherBudgetsCents is the total it is added to.
    static bool checkBudget(const Budget &budget, qint64 otherBudgetsCents, QString *error);
    // Only validated text can reach the exact cents accumulator.
    bool addExpenseCents(const DecimalCents &amountCents, const QDate &date,
                         const QString &category, QString *error);
    QVector<Budget> budgets_;
    QVector<Expense> expenses_;
    DecimalCents spendingCents_;
};
