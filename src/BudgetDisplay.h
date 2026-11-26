#pragma once

#include <QString>
#include <QVector>

// View-ready values, so a display never needs a Model type to draw the screen.
struct BudgetRow {
    QString category;
    QString total;
    QString period;
    QString perDay;
};

struct ExpenseRow {
    QString date;
    QString amount;
    QString category;  // "Uncategorized" when none was recorded
};

struct BudgetDetails {
    QString category;
    QString period;
    QString totalSpent;
    QVector<ExpenseRow> expenses;  // newest first
};

struct ActiveBudgetRow {
    int shownRow;
    QString category;
    QString endDate;
    QString spending;
    QString balance;
    int spendingPercent;
    bool overBudget;
};

struct BudgetScreen {
    QString totalBudget;
    QString totalSpending;
    QString remaining;
    QVector<BudgetRow> budgets;
    QVector<ExpenseRow> expenses;  // newest first
    int spendingPercent = 0;
    QVector<ActiveBudgetRow> activeBudgets;  // soonest end date first
};

// The user's answer to a duplicate-budget question.
enum class ConflictChoice { UpdateExisting, ChooseAnotherCategory, Cancel };

// The user's answer to a delete confirmation.
enum class DeleteChoice { Delete, Cancel };

// The controller pushes updates through this interface; the Model never sees it.
class BudgetDisplay {
public:
    virtual ~BudgetDisplay() = default;
    virtual void showScreen(const BudgetScreen &screen) = 0;
    virtual void showMessage(const QString &text, bool isError) = 0;
    // Asks how to handle a budget that clashes with an existing one. The answer comes back
    // later through BudgetController::resolveBudgetConflict.
    virtual void askBudgetConflict(const QString &message) = 0;
    // Asks the user to confirm deleting the item the message names. The answer comes back
    // later through BudgetController::resolveDelete.
    virtual void askDeleteConfirmation(const QString &message) = 0;
};
