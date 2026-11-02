#pragma once

#include "BudgetDisplay.h"

#include <QDate>
#include <QString>
#include <QVector>

#include <optional>

// Forward-declared so the View, which includes this header, cannot reach Model types.
class BudgetModel;
class BudgetStore;

class BudgetController {
public:
    explicit BudgetController(BudgetModel &model);
    // Non-owning. A non-null display is shown the current screen at once.
    void setDisplay(BudgetDisplay *display);
    // Non-owning and nullable. With a store set, every successful action is saved.
    void setStore(BudgetStore *store);
    // Replaces the model with the saved data and shows it, or shows why it could not.
    bool loadSaved();
    BudgetScreen screen() const;

    // A budget that clashes with an existing one is not added: the display is asked what
    // to do, the request is kept, and this returns false.
    bool createBudget(const QString &category, const QString &amountText,
                      const QDate &startDate, const QDate &endDate);
    // Carries out the answer to the last question. True when the choice was carried out;
    // false when nothing was pending or the update was rejected.
    bool resolveBudgetConflict(ConflictChoice choice);
    bool updateBudgetAmount(int row, const QString &amountText);
    bool addExpense(const QString &amountText, const QDate &date,
                    const QString &category = QString());
    // shownRow counts rows as the screen shows them. A valid row asks the display to confirm
    // and returns true; a bad row is an error. While a delete is waiting for its answer,
    // another request is refused: it shows nothing and returns false.
    bool requestDeleteBudget(int shownRow);
    bool requestDeleteExpense(int shownRow);
    // Carries out the answer to the pending delete question. True when the choice was carried
    // out; false when nothing was pending or the named item is no longer where it was.
    bool resolveDelete(DeleteChoice choice);

private:
    bool commitAndShow(bool succeeded, const QString &error, const QString &successMessage);
    // Model indices in the order the screen shows them, so a shown row maps to its item.
    QVector<qsizetype> shownBudgetOrder() const;
    QVector<qsizetype> shownExpenseOrder() const;

    // The budget createBudget was asked for, and the index of the budget it clashes with.
    // Plain fields because this header may not include the Model's Budget type.
    struct PendingBudget {
        qsizetype conflictIndex;
        QString category;
        qint64 amountCents;
        QDate startDate;
        QDate endDate;
    };

    // A copy of the item a delete question named, so resolveDelete removes it only if it is
    // still at index. Plain fields for the same reason as PendingBudget.
    struct PendingDelete {
        enum class Kind { Budget, Expense } kind;
        qsizetype index;
        QString category;
        QString amountCents;  // exact DecimalCents text
        QDate startDate;      // an expense's date
        QDate endDate;        // budgets only
    };
    // The item at a Model index, or nothing when the index is out of range.
    std::optional<PendingDelete> deleteCandidate(PendingDelete::Kind kind, qsizetype index) const;
    bool askDelete(const PendingDelete &candidate, const QString &question);

    BudgetModel &model_;
    BudgetDisplay *display_ = nullptr;
    BudgetStore *store_ = nullptr;
    std::optional<PendingBudget> pending_;
    std::optional<PendingDelete> pendingDelete_;
};
