#pragma once

#include "BudgetController.h"

#include <QPalette>
#include <QWidget>

#include <array>

class QComboBox;
class QDateEdit;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
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

protected:
    void changeEvent(QEvent *event) override;

private:
    struct RecentExpenseRow {
        QWidget *container;
        QLabel *category;
        QLabel *date;
        QLabel *amount;
    };

    QWidget *makeHomePage();
    QWidget *makeBudgetsPage();
    QWidget *makeExpensesPage();
    QWidget *makeBudgetDetailsPanel();
    void toggleBudgetDetails(int row);
    void hideBudgetDetails();
    void applyAppearance();
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
    std::array<RecentExpenseRow, 5> recentExpenses_;
    QLabel *recentExpensesEmpty_;
    QPushButton *recentExpensesAction_;
    QLabel *feedback_;
    QTableWidget *budgetsTable_;
    QTableWidget *expensesTable_;
    QWidget *budgetsEmpty_;
    QWidget *expensesEmpty_;
    QGroupBox *budgetDetailsPanel_;
    QLabel *budgetDetailsPeriod_;
    QLabel *budgetDetailsTotal_;
    QTableWidget *budgetDetailsTable_;
    QWidget *budgetDetailsEmpty_;
    int expandedBudgetRow_ = -1;
    QPalette appearancePalette_;
};
