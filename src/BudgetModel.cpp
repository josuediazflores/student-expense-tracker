#include "BudgetModel.h"

#include <cmath>
#include <limits>

double Budget::amountPerDay() const {
    const qint64 days = startDate.daysTo(endDate) + 1;
    return days > 0 ? static_cast<double>(amountCents) / 100.0 / days : 0.0;
}

bool BudgetModel::addBudget(const Budget &budget, QString *error) {
    auto reject = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    if (budget.amountCents < 0)
        return reject(QStringLiteral("Budget amount cannot be negative."));
    if (!budget.startDate.isValid() || !budget.endDate.isValid())
        return reject(QStringLiteral("Choose valid start and end dates."));
    if (budget.endDate < budget.startDate)
        return reject(QStringLiteral("End date cannot be before start date."));
    if (budget.category.trimmed().isEmpty())
        return reject(QStringLiteral("Choose a category."));
    if (budget.amountCents > std::numeric_limits<qint64>::max() - totalCents())
        return reject(QStringLiteral("Total budget is too large."));

    Budget saved = budget;
    saved.category = saved.category.trimmed();
    budgets_.append(saved);
    if (error) error->clear();
    return true;
}

qint64 BudgetModel::totalCents() const {
    qint64 sum = 0;
    for (const Budget &budget : budgets_) sum += budget.amountCents;
    return sum;
}

bool BudgetModel::addExpense(qint64 amountCents, QString *error) {
    if (amountCents < 0) {
        if (error) *error = QStringLiteral("Expense amount cannot be negative.");
        return false;
    }
    if (amountCents > std::numeric_limits<qint64>::max() - spendingCents_) {
        if (error) *error = QStringLiteral("Total spending is too large.");
        return false;
    }

    spendingCents_ += amountCents;
    if (error) error->clear();
    return true;
}

bool BudgetModel::addRoundedExpense(long double amountCents, QString *error) {
    if (!std::isfinite(amountCents)) {
        if (error) *error = QStringLiteral("Expense amount must be finite.");
        return false;
    }
    if (amountCents < 0) {
        if (error) *error = QStringLiteral("Expense amount cannot be negative.");
        return false;
    }

    const long double roundedCents = std::round(amountCents);
    // 2^63 is exact even on platforms where long double has double precision.
    const long double upperLimit = -static_cast<long double>(std::numeric_limits<qint64>::min());
    if (roundedCents >= upperLimit) {
        if (error) *error = QStringLiteral("Expense amount is too large.");
        return false;
    }
    return addExpense(static_cast<qint64>(roundedCents), error);
}

bool BudgetModel::addUnsignedExpense(quint64 amountCents, QString *error) {
    if (amountCents > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        if (error) *error = QStringLiteral("Expense amount is too large.");
        return false;
    }
    return addExpense(static_cast<qint64>(amountCents), error);
}

qint64 BudgetModel::remainingBudgetCents() const {
    return totalCents() - spendingCents_;
}
