#include "BudgetController.h"

#include "BudgetModel.h"
#include "BudgetStore.h"
#include "DecimalCents.h"

#include <QLocale>

#include <algorithm>
#include <numeric>
#include <optional>
#include <tuple>
#include <utility>

namespace {
// Exact dollars from exact cents: "$1,234.50", "$12.345", "-$0.005". Shifting the
// decimal point in digits keeps fractions of a cent that a double would round.
QString formatMoney(const DecimalCents &cents) {
    QString dollars = cents.toDollarsString();
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
    // The pending indices point into the model this load replaces.
    pending_.reset();
    pendingDelete_.reset();
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
    for (qsizetype index : shownBudgetOrder()) {
        const Budget &budget = model_.budgets().at(index);
        result.budgets.append({budget.category, formatMoney(DecimalCents(budget.amountCents)),
                               formatDate(budget.startDate) + QStringLiteral(" to ") +
                                   formatDate(budget.endDate),
                               usEnglish.toCurrencyString(budget.amountPerDay())});
    }

    for (qsizetype index : shownExpenseOrder()) {
        const Expense &expense = model_.expenses().at(index);
        result.expenses.append({formatDate(expense.date), formatMoney(expense.amountCents),
                                expense.category.isEmpty() ? QStringLiteral("Uncategorized")
                                                           : expense.category});
    }
    return result;
}

QVector<qsizetype> BudgetController::shownBudgetOrder() const {
    QVector<qsizetype> order(model_.budgets().size());
    std::iota(order.begin(), order.end(), 0);
    return order;
}

QVector<qsizetype> BudgetController::shownExpenseOrder() const {
    // Newest date first; on the same date the later entry (higher index) comes first.
    const QVector<Expense> &expenses = model_.expenses();
    QVector<qsizetype> order(expenses.size());
    std::iota(order.begin(), order.end(), 0);
    std::sort(order.begin(), order.end(), [&expenses](qsizetype a, qsizetype b) {
        const QDate &dateA = expenses.at(a).date;
        const QDate &dateB = expenses.at(b).date;
        return dateA != dateB ? dateA > dateB : a > b;
    });
    return order;
}

bool BudgetController::createBudget(const QString &category, const QString &amountText,
                                    const QDate &startDate, const QDate &endDate) {
    QString error;
    qsizetype conflictIndex = -1;
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(amountText, &error);
    const bool added =
        cents && model_.addBudget({category, *cents, startDate, endDate}, &error, &conflictIndex);
    if (conflictIndex < 0) return commitAndShow(added, error, QStringLiteral("Budget added."));

    // Nothing changed, so nothing is saved; the display's answer decides what happens next.
    // Only one question is open at a time, so this one replaces a pending delete.
    pendingDelete_.reset();
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
        return commitAndShow(replaced, error, QStringLiteral("Budget updated."));
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
    return commitAndShow(updated, error, QStringLiteral("Budget amount updated."));
}

bool BudgetController::addExpense(const QString &amountText, const QDate &date,
                                  const QString &category) {
    QString error;
    const bool saved = model_.addExpense(amountText, date, category, &error);
    return commitAndShow(saved, error, QStringLiteral("Expense saved."));
}

bool BudgetController::requestDeleteBudget(int shownRow) {
    if (pendingDelete_) return false;  // the first question stays the one being answered
    const QVector<qsizetype> order = shownBudgetOrder();
    if (shownRow < 0 || shownRow >= order.size())
        return commitAndShow(false, QStringLiteral("Choose a budget to delete."), QString());

    const qsizetype index = order.at(shownRow);
    const Budget &budget = model_.budgets().at(index);
    return askDelete(*deleteCandidate(PendingDelete::Kind::Budget, index),
                     QStringLiteral("Delete the %1 budget of %2 for %3 to %4?")
                         .arg(budget.category, formatMoney(DecimalCents(budget.amountCents)),
                              formatDate(budget.startDate), formatDate(budget.endDate)));
}

bool BudgetController::requestDeleteExpense(int shownRow) {
    if (pendingDelete_) return false;  // the first question stays the one being answered
    const QVector<qsizetype> order = shownExpenseOrder();
    if (shownRow < 0 || shownRow >= order.size())
        return commitAndShow(false, QStringLiteral("Choose an expense to delete."), QString());

    const qsizetype index = order.at(shownRow);
    const Expense &expense = model_.expenses().at(index);
    return askDelete(*deleteCandidate(PendingDelete::Kind::Expense, index),
                     QStringLiteral("Delete the %1 expense of %2 on %3?")
                         .arg(expense.category.isEmpty() ? QStringLiteral("uncategorized")
                                                         : expense.category,
                              formatMoney(expense.amountCents), formatDate(expense.date)));
}

bool BudgetController::resolveDelete(DeleteChoice choice) {
    const std::optional<PendingDelete> pending = std::exchange(pendingDelete_, std::nullopt);
    // A copy that no longer matches means the item moved, so deleting by index could
    // remove something the user never confirmed.
    const auto stillAtIndex = [this](const PendingDelete &named) {
        const auto fields = [](const PendingDelete &item) {
            return std::tie(item.category, item.amountCents, item.startDate, item.endDate);
        };
        const std::optional<PendingDelete> current = deleteCandidate(named.kind, named.index);
        return current && fields(*current) == fields(named);
    };
    const bool cancelled = pending && choice == DeleteChoice::Cancel;
    if (!pending || cancelled || !stillAtIndex(*pending)) {
        if (display_) display_->showMessage(QStringLiteral("Nothing was deleted."), false);
        return cancelled;
    }

    QString error;
    if (pending->kind == PendingDelete::Kind::Budget) {
        const bool removed = model_.removeBudget(pending->index, &error);
        return commitAndShow(removed, error, QStringLiteral("Budget deleted."));
    }
    const bool removed = model_.removeExpense(pending->index, &error);
    return commitAndShow(removed, error, QStringLiteral("Expense deleted."));
}

std::optional<BudgetController::PendingDelete>
BudgetController::deleteCandidate(PendingDelete::Kind kind, qsizetype index) const {
    if (kind == PendingDelete::Kind::Budget) {
        if (index < 0 || index >= model_.budgets().size()) return std::nullopt;
        const Budget &budget = model_.budgets().at(index);
        return PendingDelete{kind, index, budget.category,
                             DecimalCents(budget.amountCents).toString(), budget.startDate,
                             budget.endDate};
    }
    if (index < 0 || index >= model_.expenses().size()) return std::nullopt;
    const Expense &expense = model_.expenses().at(index);
    return PendingDelete{kind, index, expense.category, expense.amountCents.toString(),
                         expense.date, QDate()};
}

// Nothing changes until the display's answer comes back through resolveDelete.
bool BudgetController::askDelete(const PendingDelete &candidate, const QString &question) {
    // Only one question is open at a time, so this one replaces a pending duplicate question.
    pending_.reset();
    pendingDelete_ = candidate;
    if (display_) display_->askDeleteConfirmation(question);
    return true;
}

// On success, saves the change, then shows the new screen and a confirmation, or the save
// error. A failed save does not undo the change, so the action still succeeds. On failure,
// nothing is saved and only the error is shown.
bool BudgetController::commitAndShow(bool succeeded, const QString &error,
                              const QString &successMessage) {
    // Every action ends here, and any action ends a pending question.
    pending_.reset();
    pendingDelete_.reset();
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
