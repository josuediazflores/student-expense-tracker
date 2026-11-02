#pragma once

#include "BudgetController.h"

#include <QWidget>

class QComboBox;
class QDateEdit;
class QLabel;
class QLineEdit;
class QTableWidget;

// Draws the screens the controller pushes and forwards raw input to it. It holds no
// business rules and sees no Model types.
class BudgetView : public QWidget, public BudgetDisplay {
public:
    explicit BudgetView(BudgetController &controller, QWidget *parent = nullptr);

    void showScreen(const BudgetScreen &screen) override;
    void showMessage(const QString &text, bool isError) override;
    // Opens the question after the controller call that raised it has returned.
    void askBudgetConflict(const QString &message) override;
    void askDeleteConfirmation(const QString &message) override;

private:
    void saveBudget();
    void updateSelectedBudget();
    void saveExpense();
    void showBudgetConflict(const QString &message);
    void showDeleteConfirmation(const QString &message);

    BudgetController &controller_;
    QComboBox *category_;
    QLineEdit *budgetAmount_;
    QDateEdit *startDate_;
    QDateEdit *endDate_;
    QComboBox *expenseCategory_;
    QLineEdit *expenseAmount_;
    QDateEdit *expenseDate_;
    QLabel *totalBudget_;
    QLabel *totalSpending_;
    QLabel *remaining_;
    QLabel *feedback_;
    QTableWidget *budgetsTable_;
    QTableWidget *expensesTable_;
};
