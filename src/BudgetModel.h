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

private:
    QVector<Budget> budgets_;
};
