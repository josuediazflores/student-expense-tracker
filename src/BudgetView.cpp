#include "BudgetView.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

namespace {
const QString kDateFormat = QStringLiteral("MMM d, yyyy");

QDateEdit *makeDateEdit(const QDate &date) {
    auto *edit = new QDateEdit(date);
    edit->setCalendarPopup(true);
    edit->setDisplayFormat(kDateFormat);
    return edit;
}

QTableWidget *makeTable(const QStringList &headers) {
    auto *table = new QTableWidget(0, headers.size());
    table->setHorizontalHeaderLabels(headers);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    return table;
}

void fillRow(QTableWidget *table, int row, const QStringList &values) {
    for (int col = 0; col < values.size(); ++col)
        table->setItem(row, col, new QTableWidgetItem(values.at(col)));
}
}  // namespace

BudgetView::BudgetView(BudgetController &controller, QWidget *parent)
    : QWidget(parent), controller_(controller) {
    setWindowTitle(QStringLiteral("SCU Student Budget"));
    resize(820, 720);

    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(QStringLiteral("Student budgets"));
    QFont headingFont = heading->font();
    headingFont.setPointSize(18);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);
    layout->addWidget(new QLabel(QStringLiteral("Set an amount and date range for each spending category.")));

    // Summary first: the three figures a student checks most.
    auto *summary = new QHBoxLayout;
    totalBudget_ = new QLabel;
    totalSpending_ = new QLabel;
    remaining_ = new QLabel;
    summary->addWidget(totalBudget_);
    summary->addWidget(totalSpending_);
    summary->addWidget(remaining_);
    layout->addLayout(summary);

    // Budget form and expense form sit side by side, each in its own labelled group.
    category_ = new QComboBox;
    category_->addItems({QStringLiteral("Meal plan"), QStringLiteral("Groceries"),
                         QStringLiteral("Dining out"), QStringLiteral("Transport"),
                         QStringLiteral("School supplies"), QStringLiteral("Other")});
    budgetAmount_ = new QLineEdit;
    budgetAmount_->setPlaceholderText(QStringLiteral("250.00"));
    startDate_ = makeDateEdit(QDate::currentDate());
    endDate_ = makeDateEdit(QDate::currentDate().addMonths(1));

    auto *budgetBox = new QGroupBox(QStringLiteral("Budget"));
    auto *budgetForm = new QFormLayout(budgetBox);
    budgetForm->addRow(QStringLiteral("Category"), category_);
    budgetForm->addRow(QStringLiteral("Amount ($)"), budgetAmount_);
    budgetForm->addRow(QStringLiteral("Start date"), startDate_);
    budgetForm->addRow(QStringLiteral("End date"), endDate_);
    auto *addBudget = new QPushButton(QStringLiteral("Add budget"));
    auto *updateBudget = new QPushButton(QStringLiteral("Update selected budget"));
    connect(addBudget, &QPushButton::clicked, this, [this] { saveBudget(); });
    connect(updateBudget, &QPushButton::clicked, this, [this] { updateSelectedBudget(); });
    auto *budgetButtons = new QHBoxLayout;
    budgetButtons->addWidget(addBudget);
    budgetButtons->addWidget(updateBudget);
    budgetForm->addRow(budgetButtons);

    expenseAmount_ = new QLineEdit;
    expenseAmount_->setPlaceholderText(QStringLiteral("12.50"));
    expenseDate_ = makeDateEdit(QDate::currentDate());

    auto *expenseBox = new QGroupBox(QStringLiteral("Expense"));
    auto *expenseForm = new QFormLayout(expenseBox);
    expenseForm->addRow(QStringLiteral("Amount ($)"), expenseAmount_);
    expenseForm->addRow(QStringLiteral("Date"), expenseDate_);
    auto *saveExpenseButton = new QPushButton(QStringLiteral("Save expense"));
    connect(saveExpenseButton, &QPushButton::clicked, this, [this] { saveExpense(); });
    expenseForm->addRow(saveExpenseButton);

    auto *forms = new QHBoxLayout;
    forms->addWidget(budgetBox, 3);
    forms->addWidget(expenseBox, 2);
    layout->addLayout(forms);

    feedback_ = new QLabel;
    feedback_->setAccessibleName(QStringLiteral("Budget status"));
    layout->addWidget(feedback_);

    layout->addWidget(new QLabel(QStringLiteral("Budgets")));
    budgetsTable_ = makeTable({QStringLiteral("Category"), QStringLiteral("Total"),
                               QStringLiteral("Period"), QStringLiteral("Per day")});
    layout->addWidget(budgetsTable_);

    layout->addWidget(new QLabel(QStringLiteral("Expenses (newest first)")));
    expensesTable_ = makeTable({QStringLiteral("Date"), QStringLiteral("Amount")});
    layout->addWidget(expensesTable_);
}

void BudgetView::saveBudget() {
    if (controller_.createBudget(category_->currentText(), budgetAmount_->text(),
                                 startDate_->date(), endDate_->date()))
        budgetAmount_->clear();
}

void BudgetView::updateSelectedBudget() {
    // currentRow() can be set by keyboard focus or a Ctrl+click deselect with nothing highlighted,
    // so use the selected row to avoid overwriting a budget the student never picked.
    const QModelIndexList rows = budgetsTable_->selectionModel()->selectedRows();
    const int row = rows.isEmpty() ? -1 : rows.first().row();
    controller_.updateBudgetAmount(row, budgetAmount_->text());
}

void BudgetView::saveExpense() {
    if (controller_.addExpense(expenseAmount_->text(), expenseDate_->date()))
        expenseAmount_->clear();
}

void BudgetView::showScreen(const BudgetScreen &screen) {
    totalBudget_->setText(QStringLiteral("Total budget: %1").arg(screen.totalBudget));
    totalSpending_->setText(QStringLiteral("Total spending: %1").arg(screen.totalSpending));
    remaining_->setText(QStringLiteral("Remaining budget: %1").arg(screen.remaining));

    budgetsTable_->setRowCount(screen.budgets.size());
    for (int row = 0; row < screen.budgets.size(); ++row) {
        const BudgetRow &budget = screen.budgets.at(row);
        fillRow(budgetsTable_, row, {budget.category, budget.total, budget.period, budget.perDay});
    }
    expensesTable_->setRowCount(screen.expenses.size());
    for (int row = 0; row < screen.expenses.size(); ++row) {
        const ExpenseRow &expense = screen.expenses.at(row);
        fillRow(expensesTable_, row, {expense.date, expense.amount});
    }
}

void BudgetView::showMessage(const QString &text, bool isError) {
    feedback_->setStyleSheet(isError ? QStringLiteral("color: #b3261e;") : QString());
    feedback_->setText(text);
}
