#pragma once

#include "BudgetDisplay.h"

#include <QDate>
#include <QString>

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

    bool createBudget(const QString &category, const QString &amountText,
                      const QDate &startDate, const QDate &endDate);
    bool updateBudgetAmount(int row, const QString &amountText);
    bool addExpense(const QString &amountText, const QDate &date);

private:
    bool report(bool succeeded, const QString &error, const QString &successMessage);

    BudgetModel &model_;
    BudgetDisplay *display_ = nullptr;
    BudgetStore *store_ = nullptr;
};
