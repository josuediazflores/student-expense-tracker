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
#include <QMessageBox>
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

// One category list for both pickers, so a budget and an expense always offer the same names.
QComboBox *makeCategoryPicker() {
    auto *picker = new QComboBox;
    picker->addItems({QStringLiteral("Meal plan"), QStringLiteral("Groceries"),
                      QStringLiteral("Dining out"), QStringLiteral("Transport"),
                      QStringLiteral("School supplies"), QStringLiteral("Other")});
    return picker;
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

// The highlighted row, or -1. currentRow() can be set by keyboard focus or a Ctrl+click
// deselect with nothing highlighted, so a destructive action must use the selection instead.
int selectedRow(const QTableWidget *table) {
    const QModelIndexList rows = table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
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
    category_ = makeCategoryPicker();
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
    auto *deleteBudget = new QPushButton(QStringLiteral("Delete budget"));
    deleteBudget->setEnabled(false);
    connect(addBudget, &QPushButton::clicked, this, [this] { saveBudget(); });
    connect(updateBudget, &QPushButton::clicked, this, [this] { updateSelectedBudget(); });
    connect(deleteBudget, &QPushButton::clicked, this, [this] {
        controller_.requestDeleteBudget(selectedRow(budgetsTable_));
    });
    auto *budgetButtons = new QHBoxLayout;
    budgetButtons->addWidget(addBudget);
    budgetButtons->addWidget(updateBudget);
    budgetButtons->addWidget(deleteBudget);
    budgetForm->addRow(budgetButtons);

    expenseCategory_ = makeCategoryPicker();
    expenseAmount_ = new QLineEdit;
    expenseAmount_->setPlaceholderText(QStringLiteral("12.50"));
    expenseDate_ = makeDateEdit(QDate::currentDate());

    auto *expenseBox = new QGroupBox(QStringLiteral("Expense"));
    auto *expenseForm = new QFormLayout(expenseBox);
    expenseForm->addRow(QStringLiteral("Category"), expenseCategory_);
    expenseForm->addRow(QStringLiteral("Amount ($)"), expenseAmount_);
    expenseForm->addRow(QStringLiteral("Date"), expenseDate_);
    auto *saveExpenseButton = new QPushButton(QStringLiteral("Save expense"));
    auto *deleteExpense = new QPushButton(QStringLiteral("Delete expense"));
    deleteExpense->setEnabled(false);
    connect(saveExpenseButton, &QPushButton::clicked, this, [this] { saveExpense(); });
    connect(deleteExpense, &QPushButton::clicked, this, [this] {
        controller_.requestDeleteExpense(selectedRow(expensesTable_));
    });
    auto *expenseButtons = new QHBoxLayout;
    expenseButtons->addWidget(saveExpenseButton);
    expenseButtons->addWidget(deleteExpense);
    expenseForm->addRow(expenseButtons);

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
    expensesTable_ = makeTable({QStringLiteral("Date"), QStringLiteral("Category"),
                                 QStringLiteral("Amount")});
    layout->addWidget(expensesTable_);

    // A delete button works only on a highlighted row.
    connect(budgetsTable_, &QTableWidget::itemSelectionChanged, this,
            [this, deleteBudget] { deleteBudget->setEnabled(selectedRow(budgetsTable_) >= 0); });
    connect(expensesTable_, &QTableWidget::itemSelectionChanged, this,
            [this, deleteExpense] { deleteExpense->setEnabled(selectedRow(expensesTable_) >= 0); });
}

void BudgetView::saveBudget() {
    if (controller_.createBudget(category_->currentText(), budgetAmount_->text(),
                                 startDate_->date(), endDate_->date()))
        budgetAmount_->clear();
}

void BudgetView::updateSelectedBudget() {
    // Not currentRow(): see selectedRow.
    controller_.updateBudgetAmount(selectedRow(budgetsTable_), budgetAmount_->text());
}

void BudgetView::saveExpense() {
    if (controller_.addExpense(expenseAmount_->text(), expenseDate_->date(),
                               expenseCategory_->currentText()))
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
        fillRow(expensesTable_, row, {expense.date, expense.category, expense.amount});
    }

    // Rows may have moved or gone, so a delete must not act on a stale highlight. This runs after
    // the refill because shrinking a table makes Qt re-select a row near the current one; clear()
    // drops that current index too, and the selection signals disable the delete buttons.
    budgetsTable_->selectionModel()->clear();
    expensesTable_->selectionModel()->clear();
}

void BudgetView::showMessage(const QString &text, bool isError) {
    feedback_->setStyleSheet(isError ? QStringLiteral("color: #b3261e;") : QString());
    feedback_->setText(text);
}

void BudgetView::askBudgetConflict(const QString &message) {
    // Queued so the controller's createBudget has returned before the box's event loop starts.
    QMetaObject::invokeMethod(this, [this, message] { showBudgetConflict(message); },
                              Qt::QueuedConnection);
}

void BudgetView::showBudgetConflict(const QString &message) {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Budget already exists"));
    box.setText(message);
    QPushButton *update = box.addButton(QStringLiteral("Update existing budget"), QMessageBox::AcceptRole);
    QPushButton *another = box.addButton(QStringLiteral("Choose another category"), QMessageBox::ActionRole);
    QPushButton *cancel = box.addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(update);
    box.setEscapeButton(cancel);
    box.exec();

    // Closing the box without a button counts as Cancel.
    const QAbstractButton *clicked = box.clickedButton();
    if (clicked == update) {
        if (controller_.resolveBudgetConflict(ConflictChoice::UpdateExisting))
            budgetAmount_->clear();
    } else if (clicked == another) {
        controller_.resolveBudgetConflict(ConflictChoice::ChooseAnotherCategory);
        category_->setFocus();
    } else {
        controller_.resolveBudgetConflict(ConflictChoice::Cancel);
    }
}

void BudgetView::askDeleteConfirmation(const QString &message) {
    // Queued so the controller's requestDelete has returned before the box's event loop starts.
    QMetaObject::invokeMethod(this, [this, message] { showDeleteConfirmation(message); },
                              Qt::QueuedConnection);
}

void BudgetView::showDeleteConfirmation(const QString &message) {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm delete"));
    box.setText(message);
    QPushButton *del = box.addButton(QStringLiteral("Delete"), QMessageBox::DestructiveRole);
    QPushButton *cancel = box.addButton(QStringLiteral("Cancel"), QMessageBox::RejectRole);
    // Cancel is the default so a stray Enter cannot delete; there is no undo.
    box.setDefaultButton(cancel);
    box.setEscapeButton(cancel);
    box.exec();

    // Closing the box without a button counts as Cancel.
    controller_.resolveDelete(box.clickedButton() == del ? DeleteChoice::Delete
                                                         : DeleteChoice::Cancel);
}
