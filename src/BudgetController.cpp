#include "BudgetController.h"

#include "BudgetModel.h"
#include "BudgetStore.h"
#include "DecimalCents.h"

#include <QLocale>

#include <algorithm>
#include <numeric>
#include <optional>
#include <utility>

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

void BudgetController::setStore(BudgetStore *store) { store_ = store; }

bool BudgetController::loadSaved() {
    // The pending index points into the model this load replaces.
    pending_.reset();
    if (!store_) return true;
    QString error;
    if (!store_->load(model_, &error)) {
        if (display_)
            display_->showMessage(QStringLiteral("Saved data could not be loaded: ") + error, true);
        return false;
    }
    if (display_) display_->showScreen(screen());
    return true;
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
        result.expenses.append({formatDate(expense.date), formatMoney(expense.amountCents),
                                expense.category.isEmpty() ? QStringLiteral("Uncategorized")
                                                           : expense.category});
    }
    return result;
}

bool BudgetController::createBudget(const QString &category, const QString &amountText,
                                    const QDate &startDate, const QDate &endDate) {
    QString error;
    qsizetype conflictIndex = -1;
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(amountText, &error);
    const bool added =
        cents && model_.addBudget({category, *cents, startDate, endDate}, &error, &conflictIndex);
    if (conflictIndex < 0) return report(added, error, QStringLiteral("Budget added."));

    // Nothing changed, so nothing is saved; the display's answer decides what happens next.
    pending_ = PendingBudget{conflictIndex, category, *cents, startDate, endDate};
    const Budget &existing = model_.budgets().at(conflictIndex);
    if (display_) {
        display_->askBudgetConflict(QStringLiteral("You already have a %1 budget of %2 for %3 to %4.")
                                        .arg(existing.category,
                                             formatMoney(DecimalCents(existing.amountCents)),
                                             formatDate(existing.startDate),
                                             formatDate(existing.endDate)));
    }
    return false;
}

bool BudgetController::resolveBudgetConflict(ConflictChoice choice) {
    const std::optional<PendingBudget> pending = std::exchange(pending_, std::nullopt);
    if (!pending) return false;
    switch (choice) {
    case ConflictChoice::UpdateExisting: {
        QString error;
        const bool replaced = model_.replaceBudget(
            pending->conflictIndex,
            {pending->category, pending->amountCents, pending->startDate, pending->endDate},
            &error);
        return report(replaced, error, QStringLiteral("Budget updated."));
    }
    case ConflictChoice::ChooseAnotherCategory:
        if (display_)
            display_->showMessage(QStringLiteral("Choose another category for this budget."), false);
        return true;
    case ConflictChoice::Cancel:
        if (display_) display_->showMessage(QStringLiteral("Budget not added."), false);
        return true;
    }
    return false;
}

bool BudgetController::updateBudgetAmount(int row, const QString &amountText) {
    QString error;
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(amountText, &error);
    const bool updated = cents && model_.updateBudgetAmount(row, *cents, &error);
    return report(updated, error, QStringLiteral("Budget amount updated."));
}

bool BudgetController::addExpense(const QString &amountText, const QDate &date,
                                  const QString &category) {
    QString error;
    const bool saved = model_.addExpense(amountText, date, category, &error);
    return report(saved, error, QStringLiteral("Expense saved."));
}

// On success, saves the change, then shows the new screen and a confirmation, or the save
// error. A failed save does not undo the change, so the action still succeeds. On failure,
// nothing is saved and only the error is shown.
bool BudgetController::report(bool succeeded, const QString &error,
                              const QString &successMessage) {
    // Every action ends here, and any action ends a pending duplicate question.
    pending_.reset();
    if (!succeeded) {
        if (display_) display_->showMessage(error, true);
        return false;
    }
    QString saveError;
    const bool saved = !store_ || store_->save(model_, &saveError);
    if (display_) {
        display_->showScreen(screen());
        if (saved)
            display_->showMessage(successMessage, false);
        else
            display_->showMessage(QStringLiteral("Your change was not saved: ") + saveError, true);
    }
    return true;
}
