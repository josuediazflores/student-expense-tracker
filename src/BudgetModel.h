#pragma once

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
    bool addExpense(qint64 amountCents, QString *error = nullptr);

    // Intercept invalid argument types before C++ can truncate floating-point cents.
    // Constrain this overload so integer 0 still selects the valid integer API.
    template <typename Amount,
              std::enable_if_t<std::is_floating_point_v<Amount> ||
                                   std::is_same_v<Amount, std::nullptr_t>, int> = 0>
    bool addExpense(Amount, QString *error = nullptr) {
        if (error) {
            if constexpr (std::is_same_v<Amount, std::nullptr_t>)
                *error = QStringLiteral("Expense amount is required.");
            else
                *error = QStringLiteral("Expense amount must be an integer number of cents.");
        }
        return false;
    }

    qint64 totalSpendingCents() const { return spendingCents_; }
    qint64 remainingBudgetCents() const;

private:
    QVector<Budget> budgets_;
    qint64 spendingCents_ = 0;
};
