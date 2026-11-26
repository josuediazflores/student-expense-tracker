#pragma once

#include "BudgetController.h"

#include <QPalette>
#include <QWidget>

#include <array>

class QComboBox;
class QDateEdit;
class QGroupBox;
class QHBoxLayout;
class QLabel;
class QLineEdit;
class QProgressBar;
class QPushButton;
class QResizeEvent;
class QTabWidget;
class QTableWidget;
class QVBoxLayout;

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
    void resizeEvent(QResizeEvent *event) override;

private:
    struct RecentExpenseRow {
        QWidget *container;
        QLabel *category;
        QLabel *date;
        QLabel *amount;
    };

    QWidget *makeHomePage();
    QWidget *makeActiveBudgetRow(const ActiveBudgetRow &budget);
    QWidget *makeBudgetsPage();
    QWidget *makeExpensesPage();
    QWidget *makeBudgetDetailsPanel();
    void toggleBudgetDetails(int row);
    void hideBudgetDetails();
    void showHome(const BudgetScreen &screen);
    void applyAppearance();
    void saveBudget();
    void updateSelectedBudget();
    void saveExpense();
    void showBudgetConflict(const QString &message);
    void showDeleteConfirmation(const QString &message);

    BudgetController &controller_;
    QTabWidget *tabs_;
    int budgetsTab_;
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
    QProgressBar *spendingProgress_;
    QHBoxLayout *homeSummary_ = nullptr;
    QHBoxLayout *homeColumns_ = nullptr;
    QHBoxLayout *expenseColumns_ = nullptr;
    QVBoxLayout *activeBudgetRows_;
    QWidget *activeBudgetsEmpty_;
    QLabel *activeBudgetsEmptyTitle_;
    QLabel *homeMascot_;
    QPushButton *addBudgetAction_;
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
