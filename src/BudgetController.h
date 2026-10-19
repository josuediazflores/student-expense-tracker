#pragma once

#include "BudgetModel.h"

class BudgetController {
public:
    explicit BudgetController(BudgetModel &model) : model_(model) {}
    bool createBudget(const QString &category, qint64 amountCents,
                      const QDate &startDate, const QDate &endDate,
                      QString *error = nullptr);
    const QVector<Budget> &budgets() const { return model_.budgets(); }
    qint64 totalCents() const { return model_.totalCents(); }

private:
    BudgetModel &model_;
};
