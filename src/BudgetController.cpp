#include "BudgetController.h"

#include "BudgetModel.h"
#include "DecimalCents.h"

#include <QLocale>

#include <algorithm>
#include <numeric>
#include <optional>

namespace {
// Exact dollars from exact cents: "$1,234.50", "$12.345", "-$0.005". Shifting the
// decimal point in digits keeps fractions of a cent that a double would round.
QString formatMoney(const DecimalCents &cents) {
    // A DecimalCents string is always valid decimal text, so value() cannot throw.
    QString dollars =
        DecimalCents::fromDecimal(cents.toString() + QStringLiteral("e-2")).value().toString();
    const bool negative = dollars.startsWith(QLatin1Char('-'));
    if (negative) dollars.remove(0, 1);

    const qsizetype point = dollars.indexOf(QLatin1Char('.'));
    QString whole = point < 0 ? dollars : dollars.left(point);
    const QString fraction = point < 0 ? QString() : dollars.mid(point + 1);
    for (qsizetype i = whole.size() - 3; i > 0; i -= 3) whole.insert(i, QLatin1Char(','));

    return (negative ? QStringLiteral("-$") : QStringLiteral("$")) + whole + QLatin1Char('.') +
           fraction.leftJustified(2, QLatin1Char('0'));
}

QString formatDate(const QDate &date) {
    return date.toString(QStringLiteral("MMM d, yyyy"));
}
}

BudgetController::BudgetController(BudgetModel &model) : model_(model) {}

void BudgetController::setDisplay(BudgetDisplay *display) {
    display_ = display;
    if (display_) display_->showScreen(screen());
}

BudgetScreen BudgetController::screen() const {
    BudgetScreen result;
    result.totalBudget = formatMoney(DecimalCents(model_.totalCents()));
    result.totalSpending = formatMoney(model_.totalSpendingCents());
    result.remaining = formatMoney(model_.remainingBudgetCents());

    // Per day is display-only and keeps the existing rounded-currency behaviour.
    const QLocale usEnglish(QLocale::English, QLocale::UnitedStates);
    for (const Budget &budget : model_.budgets()) {
        result.budgets.append({budget.category, formatMoney(DecimalCents(budget.amountCents)),
                               formatDate(budget.startDate) + QStringLiteral(" to ") +
                                   formatDate(budget.endDate),
                               usEnglish.toCurrencyString(budget.amountPerDay())});
    }

    // Newest date first; on the same date the later entry (higher index) comes first.
    const QVector<Expense> &expenses = model_.expenses();
    QVector<qsizetype> order(expenses.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&expenses](qsizetype a, qsizetype b) {
        const QDate &dateA = expenses.at(a).date;
        const QDate &dateB = expenses.at(b).date;
        return dateA != dateB ? dateA > dateB : a > b;
    });
    for (qsizetype index : order) {
        const Expense &expense = expenses.at(index);
        result.expenses.append({formatDate(expense.date), formatMoney(expense.amountCents)});
    }
    return result;
}

bool BudgetController::createBudget(const QString &category, const QString &amountText,
                                    const QDate &startDate, const QDate &endDate) {
    QString error;
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(amountText, &error);
    const bool added = cents && model_.addBudget({category, *cents, startDate, endDate}, &error);
    return report(added, error, QStringLiteral("Budget added."));
}

bool BudgetController::updateBudgetAmount(int row, const QString &amountText) {
    QString error;
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(amountText, &error);
    const bool updated = cents && model_.updateBudgetAmount(row, *cents, &error);
    return report(updated, error, QStringLiteral("Budget amount updated."));
}

bool BudgetController::addExpense(const QString &amountText, const QDate &date) {
    QString error;
    const bool saved = model_.addExpense(amountText, date, &error);
    return report(saved, error, QStringLiteral("Expense saved."));
}

// Shows the new screen and a confirmation on success, or only the error on failure.
bool BudgetController::report(bool succeeded, const QString &error,
                              const QString &successMessage) {
    if (!display_) return succeeded;
    if (succeeded) {
        display_->showScreen(screen());
        display_->showMessage(successMessage, false);
    } else {
        display_->showMessage(error, true);
    }
    return succeeded;
}
