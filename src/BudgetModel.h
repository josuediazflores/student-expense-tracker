#pragma once

#include "DecimalCents.h"

#include <QDate>
#include <QString>
#include <QVector>

#include <cstddef>
#include <type_traits>

struct Budget {
    QString category;
    qint64 amountCents;
    QDate startDate;
    QDate endDate;

    double amountPerDay() const;
};

class BudgetModel {
public:
    bool addBudget(const Budget &budget, QString *error = nullptr);
    const QVector<Budget> &budgets() const { return budgets_; }
    qint64 totalCents() const;
    // User-facing amounts are decimal text in dollars, never binary floats.
    bool addExpense(const QString &amountDollars, QString *error = nullptr);
    bool addExpense(const char *amountDollars, QString *error = nullptr);
    bool addExpense(std::nullptr_t, QString *error = nullptr);

    // Reject numeric inputs before an implicit conversion can lose precision or units.
    template <typename Amount,
              std::enable_if_t<!std::is_same_v<std::decay_t<Amount>, QString> &&
                                   !std::is_convertible_v<std::decay_t<Amount>, const char *> &&
                                   !std::is_same_v<std::decay_t<Amount>, std::nullptr_t>, int> = 0>
    bool addExpense(const Amount &, QString *error = nullptr) {
        if (error) *error = QStringLiteral("Enter the expense as decimal text in dollars.");
        return false;
    }

    const DecimalCents &totalSpendingCents() const { return spendingCents_; }
    DecimalCents remainingBudgetCents() const;

private:
    bool addExpenseCents(const DecimalCents &amountCents, QString *error);
    QVector<Budget> budgets_;
    DecimalCents spendingCents_;
};
