#include "BudgetModel.h"

#include <QRegularExpression>

#include <limits>

namespace {
// Plain decimal dollars only: no currency sign, grouping, exponent or leading '+'.
bool isDecimalDollars(const QString &text) {
    static const QRegularExpression decimalDollars(QStringLiteral("^-?[0-9]+(?:[.][0-9]+)?$"));
    return decimalDollars.match(text).hasMatch();
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

bool BudgetModel::updateBudgetAmount(qsizetype index, qint64 amountCents, QString *error) {
    auto reject = [error](const QString &message) {
        if (error) *error = message;
        return false;
    };
    if (index < 0 || index >= budgets_.size())
        return reject(QStringLiteral("Choose a budget to update."));
    if (amountCents < 0)
        return reject(QStringLiteral("Budget amount cannot be negative."));
    // Compare against the other budgets' total so the check itself cannot overflow.
    const qint64 otherBudgetsCents = totalCents() - budgets_.at(index).amountCents;
    if (amountCents > std::numeric_limits<qint64>::max() - otherBudgetsCents)
        return reject(QStringLiteral("Total budget is too large."));

    budgets_[index].amountCents = amountCents;
    if (error) error->clear();
    return true;
}

std::optional<qint64> BudgetModel::parseBudgetAmount(const QString &amountDollars, QString *error) {
    auto reject = [error](const QString &message) -> std::optional<qint64> {
        if (error) *error = message;
        return std::nullopt;
    };
    const QString text = amountDollars.trimmed();
    if (text.isEmpty()) return reject(QStringLiteral("Budget amount is required."));
    if (!isDecimalDollars(text))
        return reject(QStringLiteral("Enter a decimal dollar amount such as 12.99."));

    // Validated decimal text always parses; value() only guards that invariant.
    const QString cents = DecimalCents::fromDecimal(text + QStringLiteral("e2")).value().toString();
    // Judged on the normalized value, so "12.340" is whole cents and "12.345" is not.
    if (cents.contains(QLatin1Char('.')))
        return reject(QStringLiteral("Budget amount must be in whole cents."));
    bool ok = false;
    const qint64 parsed = cents.toLongLong(&ok);
    if (!ok) {
        return reject(cents.startsWith(QLatin1Char('-'))
                          ? QStringLiteral("Budget amount cannot be negative.")
                          : QStringLiteral("Budget amount is too large."));
    }
    if (error) error->clear();
    return parsed;
}

bool BudgetModel::addExpense(const QString &amountDollars, QString *error) {
    return addExpense(amountDollars, QDate::currentDate(), error);
}

bool BudgetModel::addExpense(const char *amountDollars, QString *error) {
    return addExpense(amountDollars, QDate::currentDate(), error);
}

bool BudgetModel::addExpense(std::nullptr_t, QString *error) {
    return addExpense(nullptr, QDate::currentDate(), error);
}

bool BudgetModel::addExpense(const QString &amountDollars, const QDate &date, QString *error) {
    const QString text = amountDollars.trimmed();
    if (text.isEmpty()) return addExpense(nullptr, date, error);

    if (!isDecimalDollars(text)) {
        if (error) *error = QStringLiteral("Enter a decimal dollar amount such as 12.99.");
        return false;
    }

    // Shift dollar digits into cents: "12.345" -> 1234.5 cents, without rounding.
    const auto cents = DecimalCents::fromDecimal(text + QStringLiteral("e2"));
    if (!cents) {
        if (error) *error = QStringLiteral("Expense amount cannot be represented.");
        return false;
    }
    return addExpenseCents(*cents, date, error);
}

bool BudgetModel::addExpense(const char *amountDollars, const QDate &date, QString *error) {
    if (!amountDollars) return addExpense(nullptr, date, error);
    return addExpense(QString::fromUtf8(amountDollars), date, error);
}

bool BudgetModel::addExpense(std::nullptr_t, const QDate &, QString *error) {
    if (error) *error = QStringLiteral("Expense amount is required.");
    return false;
}

bool BudgetModel::addExpenseCents(const DecimalCents &amountCents, const QDate &date,
                                  QString *error) {
    if (amountCents.isNegative()) {
        if (error) *error = QStringLiteral("Expense amount cannot be negative.");
        return false;
    }
    // Check a candidate total first so rejected expenses leave balances unchanged.
    const DecimalCents updatedSpending = spendingCents_ + amountCents;
    if (DecimalCents(std::numeric_limits<qint64>::max()) < updatedSpending) {
        if (error) *error = QStringLiteral("Total spending is too large.");
        return false;
    }
    // Checked after the amount rules, so a bad amount is always reported first.
    if (!date.isValid()) {
        if (error) *error = QStringLiteral("Choose a valid expense date.");
        return false;
    }

    expenses_.append(Expense{amountCents, date});
    spendingCents_ = updatedSpending;
    if (error) error->clear();
    return true;
}

DecimalCents BudgetModel::remainingBudgetCents() const {
    return DecimalCents(totalCents()) - spendingCents_;
}
