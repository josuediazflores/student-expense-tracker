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

    // Accept fractional cents without implicit truncation, and reject missing/text input.
    // Signed integers up to 64 bits (including literal 0) keep using the API above.
    template <typename Amount,
              std::enable_if_t<!std::is_integral_v<std::decay_t<Amount>> ||
                                   !std::is_signed_v<std::decay_t<Amount>> ||
                                   (sizeof(std::decay_t<Amount>) > sizeof(qint64)), int> = 0>
    bool addExpense(const Amount &amountCents, QString *error = nullptr) {
        using Value = std::decay_t<Amount>;
        if constexpr (std::is_floating_point_v<Value>) {
            return addRoundedExpense(static_cast<long double>(amountCents), error);
        } else if constexpr (std::is_integral_v<Value> && !std::is_same_v<Value, bool>) {
            if constexpr (sizeof(Value) > sizeof(qint64)) {
                if (error) *error = QStringLiteral("Expense integer type exceeds 64 bits.");
                return false;
            } else {
                return addUnsignedExpense(static_cast<quint64>(amountCents), error);
            }
        }

        if (error) {
            if constexpr (std::is_same_v<Value, std::nullptr_t>)
                *error = QStringLiteral("Expense amount is required.");
            else
                *error = QStringLiteral("Expense amount must be numeric.");
        }
        return false;
    }

    qint64 totalSpendingCents() const { return spendingCents_; }
    qint64 remainingBudgetCents() const;

private:
    bool addRoundedExpense(long double amountCents, QString *error);
    bool addUnsignedExpense(quint64 amountCents, QString *error);
    QVector<Budget> budgets_;
    qint64 spendingCents_ = 0;
};
