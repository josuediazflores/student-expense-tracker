#include "BudgetModel.h"

#include <QRegularExpression>

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

bool BudgetModel::addExpense(const QString &amountDollars, QString *error) {
    const QString text = amountDollars.trimmed();
    if (text.isEmpty()) return addExpense(nullptr, error);

    static const QRegularExpression decimalDollars(QStringLiteral("^-?[0-9]+(?:[.][0-9]+)?$"));
    if (!decimalDollars.match(text).hasMatch()) {
        if (error) *error = QStringLiteral("Enter a decimal dollar amount such as 12.99.");
        return false;
    }

    // Moving the decimal point by two places is exact digit processing.
    const auto cents = DecimalCents::fromDecimal(text + QStringLiteral("e2"));
    if (!cents) {
        if (error) *error = QStringLiteral("Expense amount cannot be represented.");
        return false;
    }
    return addExpenseCents(*cents, error);
}

bool BudgetModel::addExpense(const char *amountDollars, QString *error) {
    if (!amountDollars) return addExpense(nullptr, error);
    return addExpense(QString::fromUtf8(amountDollars), error);
}

bool BudgetModel::addExpense(std::nullptr_t, QString *error) {
    if (error) *error = QStringLiteral("Expense amount is required.");
    return false;
}

bool BudgetModel::addExpenseCents(const DecimalCents &amountCents, QString *error) {
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

DecimalCents BudgetModel::remainingBudgetCents() const {
    return DecimalCents(totalCents()) - spendingCents_;
}
