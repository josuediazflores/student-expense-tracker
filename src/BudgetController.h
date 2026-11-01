#pragma once

#include "BudgetDisplay.h"

#include <QDate>
#include <QString>

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
    // Carries out the answer to the last question; false if nothing was pending.
    bool resolveBudgetConflict(ConflictChoice choice);
    bool updateBudgetAmount(int row, const QString &amountText);
    bool addExpense(const QString &amountText, const QDate &date,
                    const QString &category = QString());

private:
    bool report(bool succeeded, const QString &error, const QString &successMessage);

    // The budget createBudget was asked for, and the index of the budget it clashes with.
    // Plain fields because this header may not include the Model's Budget type.
    struct PendingBudget {
        qsizetype conflictIndex;
        QString category;
        qint64 amountCents;
        QDate startDate;
        QDate endDate;
    };

    BudgetModel &model_;
    BudgetDisplay *display_ = nullptr;
    BudgetStore *store_ = nullptr;
    std::optional<PendingBudget> pending_;
};
