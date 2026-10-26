#include "BudgetModel.h"

#include <charconv>
#include <cmath>
#include <limits>

namespace {
template <typename Float>
std::optional<DecimalCents> decimalFromFloating(Float amountCents, QString *error) {
    auto reject = [error](const QString &message) -> std::optional<DecimalCents> {
        if (error) *error = message;
        return std::nullopt;
    };
    if (!std::isfinite(amountCents))
        return reject(QStringLiteral("Expense amount must be finite."));
    if (amountCents < 0)
        return reject(QStringLiteral("Expense amount cannot be negative."));

    const long double upperLimit = -static_cast<long double>(std::numeric_limits<qint64>::min());
    if (static_cast<long double>(amountCents) >= upperLimit)
        return reject(QStringLiteral("Expense amount is too large."));

    // An integral float near the limit must retain its actual integer value.
    if (std::trunc(amountCents) == amountCents)
        return DecimalCents(static_cast<qint64>(amountCents));

    // Format the original float type, before promotion, without a precision cutoff.
    char buffer[128];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), amountCents);
    if (result.ec != std::errc{})
        return reject(QStringLiteral("Expense amount cannot be represented."));
    auto decimal = DecimalCents::fromDecimal(
        QString::fromLatin1(buffer, static_cast<int>(result.ptr - buffer)));
    if (!decimal)
        return reject(QStringLiteral("Expense amount cannot be represented."));
    return decimal;
}
}

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
    return addExpense(DecimalCents(amountCents), error);
}

bool BudgetModel::addExpense(const DecimalCents &amountCents, QString *error) {
    if (amountCents.isNegative()) {
        if (error) *error = QStringLiteral("Expense amount cannot be negative.");
        return false;
    }
    const DecimalCents updatedSpending = spendingCents_ + amountCents;
    if (DecimalCents(std::numeric_limits<qint64>::max()) < updatedSpending) {
        if (error) *error = QStringLiteral("Total spending is too large.");
        return false;
    }

    spendingCents_ = updatedSpending;
    if (error) error->clear();
    return true;
}

bool BudgetModel::addFloatingExpense(float amountCents, QString *error) {
    auto decimal = decimalFromFloating(amountCents, error);
    return decimal && addExpense(*decimal, error);
}

bool BudgetModel::addFloatingExpense(double amountCents, QString *error) {
    auto decimal = decimalFromFloating(amountCents, error);
    return decimal && addExpense(*decimal, error);
}

bool BudgetModel::addFloatingExpense(long double amountCents, QString *error) {
    auto decimal = decimalFromFloating(amountCents, error);
    return decimal && addExpense(*decimal, error);
}

bool BudgetModel::addUnsignedExpense(quint64 amountCents, QString *error) {
    if (amountCents > static_cast<quint64>(std::numeric_limits<qint64>::max())) {
        if (error) *error = QStringLiteral("Expense amount is too large.");
        return false;
    }
    return addExpense(static_cast<qint64>(amountCents), error);
}

DecimalCents BudgetModel::remainingBudgetCents() const {
    return DecimalCents(totalCents()) - spendingCents_;
}
