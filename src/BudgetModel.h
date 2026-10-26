#pragma once

#include <QDate>
#include <QString>
#include <QVector>

struct Budget {
    QString category;
    qint64 amountCents;
    QDate startDate;
    QDate endDate;

    double amountPerDay() const;
};

class BudgetModel {
public:
    bool addBudget(const Budget &budget, QString *error = nullptr);
    const QVector<Budget> &budgets() const { return budgets_; }
    qint64 totalCents() const;
    bool addExpense(qint64 amountCents, QString *error = nullptr);
    qint64 totalSpendingCents() const { return spendingCents_; }
    qint64 remainingBudgetCents() const;

private:
    QVector<Budget> budgets_;
    qint64 spendingCents_ = 0;
};
