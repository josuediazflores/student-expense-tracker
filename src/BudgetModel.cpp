#include "BudgetModel.h"

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
