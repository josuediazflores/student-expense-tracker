#include "BudgetView.h"

#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QEvent>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {
const QString kDateFormat = QStringLiteral("MMM d, yyyy");

QString textOnAccent(const QColor &accent) {
    const auto linear = [](qreal channel) {
        return channel <= 0.04045 ? channel / 12.92
                                  : std::pow((channel + 0.055) / 1.055, 2.4);
    };
    const qreal luminance = 0.2126 * linear(accent.redF()) +
                            0.7152 * linear(accent.greenF()) +
                            0.0722 * linear(accent.blueF());
    // Choose the stronger contrast for the user's accent, including pale colors.
    return luminance > 0.179 ? QStringLiteral("#000000") : QStringLiteral("#ffffff");
}

QLabel *makeLabel(const QString &text, int size, bool bold = false, bool muted = false) {
    auto *label = new QLabel(text);
    QFont font = label->font();
    font.setPointSize(size);
    font.setBold(bold);
    label->setFont(font);
    label->setProperty("muted", muted);
    return label;
}

QWidget *makeSummary(const QString &caption, QLabel *&value, bool prominent = false) {
    auto *widget = new QWidget;
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(3);
    layout->addWidget(makeLabel(caption, 11, false, true));
    value = makeLabel(QStringLiteral("$0.00"), prominent ? 26 : 22, prominent);
    value->setAccessibleDescription(caption);
    layout->addWidget(value);
    return widget;
}

QFormLayout *makeForm() {
    auto *form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(10);
    form->setVerticalSpacing(8);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    return form;
}

void addField(QFormLayout *form, const QString &text, QWidget *field) {
    auto *label = new QLabel(text);
    label->setFixedWidth(76);
    label->setBuddy(field);
    field->setFixedHeight(32);
    field->setAccessibleName(text);
    form->addRow(label, field);
}

QPushButton *makeButton(const QString &text, bool primary = false) {
    auto *button = new QPushButton(text);
    button->setFixedHeight(32);
    button->setProperty("primary", primary);
    return button;
}

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
    table->horizontalHeader()->setMinimumSectionSize(100);
    table->horizontalHeader()->setHighlightSections(false);
    table->verticalHeader()->hide();
    table->verticalHeader()->setDefaultSectionSize(34);
    table->setShowGrid(false);
    table->setMinimumHeight(108);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    return table;
}

// The hint lives in the empty viewport, so it stays centered when the window resizes.
QWidget *makeEmptyState(QTableWidget *table, const QString &title, const QString &hint) {
    auto *empty = new QWidget(table->viewport());
    empty->setAttribute(Qt::WA_TransparentForMouseEvents);
    auto *content = new QVBoxLayout(empty);
    content->setSpacing(4);
    content->setAlignment(Qt::AlignCenter);
    auto *titleLabel = makeLabel(title, 12, true);
    auto *hintLabel = makeLabel(hint, 11, false, true);
    titleLabel->setAlignment(Qt::AlignCenter);
    hintLabel->setAlignment(Qt::AlignCenter);
    content->addWidget(titleLabel);
    content->addWidget(hintLabel);
    auto *viewportLayout = new QVBoxLayout(table->viewport());
    viewportLayout->setContentsMargins(0, 0, 0, 0);
    viewportLayout->addWidget(empty);
    return empty;
}

// The highlighted row, or -1. currentRow() can be set by keyboard focus or a Ctrl+click
// deselect with nothing highlighted, so a destructive action must use the selection instead.
int selectedRow(const QTableWidget *table) {
    const QModelIndexList rows = table->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

void fillRow(QTableWidget *table, int row, const QStringList &values) {
    for (int col = 0; col < values.size(); ++col) {
        auto *item = new QTableWidgetItem(values.at(col));
        item->setTextAlignment(Qt::Alignment(table->horizontalHeaderItem(col)->textAlignment()));
        table->setItem(row, col, item);
    }
}

void alignColumns(QTableWidget *table, const QList<int> &moneyColumns) {
    for (int col = 0; col < table->columnCount(); ++col) {
        const auto alignment = moneyColumns.contains(col) ? Qt::AlignRight : Qt::AlignLeft;
        table->horizontalHeaderItem(col)->setTextAlignment(alignment | Qt::AlignVCenter);
    }
}
}  // namespace

BudgetView::BudgetView(BudgetController &controller, QWidget *parent)
    : QWidget(parent), controller_(controller) {
    setWindowTitle(QStringLiteral("SCU Student Budget"));
    resize(840, 700);
    QFont bodyFont = font();
    bodyFont.setPointSize(12);
    setFont(bodyFont);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);
    auto *intro = new QVBoxLayout;
    intro->setSpacing(4);
    intro->addWidget(makeLabel(QStringLiteral("Student budgets"), 20, true));
    intro->addWidget(makeLabel(QStringLiteral("Set an amount and date range for each spending category."),
                              12, false, true));
    layout->addLayout(intro);

    // Summary first: the three figures a student checks most.
    auto *summary = new QHBoxLayout;
    summary->setSpacing(24);
    summary->addWidget(makeSummary(QStringLiteral("Total budget"), totalBudget_), 1, Qt::AlignTop);
    summary->addWidget(makeSummary(QStringLiteral("Total spending"), totalSpending_), 1, Qt::AlignTop);
    summary->addWidget(makeSummary(QStringLiteral("Remaining"), remaining_, true), 1, Qt::AlignTop);
    layout->addLayout(summary);

    // Budget form and expense form sit side by side, each in its own labelled group.
    category_ = makeCategoryPicker();
    budgetAmount_ = new QLineEdit;
    budgetAmount_->setProperty("amountField", true);
    budgetAmount_->setPlaceholderText(QStringLiteral("250.00"));
    startDate_ = makeDateEdit(QDate::currentDate());
    endDate_ = makeDateEdit(QDate::currentDate().addMonths(1));

    auto *budgetBox = new QGroupBox(QStringLiteral("Budget"));
    auto *budgetLayout = new QVBoxLayout(budgetBox);
    budgetLayout->setContentsMargins(14, 22, 14, 14);
    budgetLayout->setSpacing(12);
    auto *budgetForm = makeForm();
    addField(budgetForm, QStringLiteral("Category"), category_);
    addField(budgetForm, QStringLiteral("Amount ($)"), budgetAmount_);
    addField(budgetForm, QStringLiteral("Start date"), startDate_);
    addField(budgetForm, QStringLiteral("End date"), endDate_);
    auto *addBudget = makeButton(QStringLiteral("Add budget"), true);
    auto *updateBudget = makeButton(QStringLiteral("Update budget"));
    updateBudget->setToolTip(QStringLiteral("Apply the amount above to the selected budget."));
    updateBudget->setEnabled(false);
    auto *deleteBudget = makeButton(QStringLiteral("Delete budget"));
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
    budgetButtons->setSpacing(8);
    budgetLayout->addLayout(budgetForm);
    budgetLayout->addStretch();
    budgetLayout->addLayout(budgetButtons);

    expenseCategory_ = makeCategoryPicker();
    expenseAmount_ = new QLineEdit;
    expenseAmount_->setProperty("amountField", true);
    expenseAmount_->setPlaceholderText(QStringLiteral("12.50"));
    expenseDate_ = makeDateEdit(QDate::currentDate());

    auto *expenseBox = new QGroupBox(QStringLiteral("Expense"));
    auto *expenseLayout = new QVBoxLayout(expenseBox);
    expenseLayout->setContentsMargins(14, 22, 14, 14);
    expenseLayout->setSpacing(12);
    auto *expenseForm = makeForm();
    addField(expenseForm, QStringLiteral("Category"), expenseCategory_);
    addField(expenseForm, QStringLiteral("Amount ($)"), expenseAmount_);
    addField(expenseForm, QStringLiteral("Date"), expenseDate_);
    auto *saveExpenseButton = makeButton(QStringLiteral("Save expense"), true);
    auto *deleteExpense = makeButton(QStringLiteral("Delete expense"));
    deleteExpense->setEnabled(false);
    connect(saveExpenseButton, &QPushButton::clicked, this, [this] { saveExpense(); });
    connect(deleteExpense, &QPushButton::clicked, this, [this] {
        controller_.requestDeleteExpense(selectedRow(expensesTable_));
    });
    auto *expenseButtons = new QHBoxLayout;
    expenseButtons->addWidget(saveExpenseButton);
    expenseButtons->addWidget(deleteExpense);
    expenseButtons->setSpacing(8);
    expenseLayout->addLayout(expenseForm);
    expenseLayout->addStretch();
    expenseLayout->addLayout(expenseButtons);

    auto *forms = new QHBoxLayout;
    forms->setSpacing(16);
    forms->addWidget(budgetBox, 1);
    forms->addWidget(expenseBox, 1);
    layout->addLayout(forms);

    feedback_ = new QLabel;
    feedback_->setAccessibleDescription(QStringLiteral("Budget status"));
    feedback_->setWordWrap(true);
    feedback_->hide();
    layout->addWidget(feedback_);

    auto *budgetSection = new QVBoxLayout;
    budgetSection->setSpacing(6);
    budgetSection->addWidget(makeLabel(QStringLiteral("Budgets"), 12, true));
    budgetsTable_ = makeTable({QStringLiteral("Category"), QStringLiteral("Total"),
                               QStringLiteral("Period"), QStringLiteral("Per day")});
    budgetsTable_->setAccessibleName(QStringLiteral("Budgets"));
    alignColumns(budgetsTable_, {1, 3});
    for (int col : {0, 1, 3})
        budgetsTable_->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    budgetsEmpty_ = makeEmptyState(budgetsTable_, QStringLiteral("No budgets yet"),
                                   QStringLiteral("Add your first budget above."));
    budgetSection->addWidget(budgetsTable_);
    layout->addLayout(budgetSection, 1);

    auto *expenseSection = new QVBoxLayout;
    expenseSection->setSpacing(6);
    auto *expenseHeading = new QHBoxLayout;
    expenseHeading->addWidget(makeLabel(QStringLiteral("Expenses"), 12, true));
    expenseHeading->addStretch();
    expenseHeading->addWidget(makeLabel(QStringLiteral("Newest first"), 11, false, true));
    expenseSection->addLayout(expenseHeading);
    expensesTable_ = makeTable({QStringLiteral("Date"), QStringLiteral("Category"),
                                 QStringLiteral("Amount")});
    expensesTable_->setAccessibleName(QStringLiteral("Expenses"));
    alignColumns(expensesTable_, {2});
    expensesTable_->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    expensesEmpty_ = makeEmptyState(expensesTable_, QStringLiteral("No expenses yet"),
                                    QStringLiteral("Log your first expense above."));
    expenseSection->addWidget(expensesTable_);
    layout->addLayout(expenseSection, 1);

    // Row actions work only on a highlighted row.
    connect(budgetsTable_, &QTableWidget::itemSelectionChanged, this,
            [this, updateBudget, deleteBudget] {
                const bool selected = selectedRow(budgetsTable_) >= 0;
                updateBudget->setEnabled(selected);
                deleteBudget->setEnabled(selected);
            });
    connect(expensesTable_, &QTableWidget::itemSelectionChanged, this,
            [this, deleteExpense] { deleteExpense->setEnabled(selectedRow(expensesTable_) >= 0); });
    applyAppearance();
}

void BudgetView::applyAppearance() {
    const QPalette colors = QApplication::palette();
    appearancePalette_ = colors;
    // Drop resolved stylesheet colors before applying a changed system palette.
    setStyleSheet(QString());
    const bool dark = colors.color(QPalette::Window).lightness() < 128;
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    const QColor accent = colors.color(QPalette::Active, QPalette::Accent);
#else
    const QColor accent = colors.color(QPalette::Active, QPalette::Highlight);
#endif
    // Opaque surfaces and system colors keep both macOS appearances readable.
    setStyleSheet(QStringLiteral(R"(
        QLabel[muted="true"] { color: %1; }
        QLabel[negative="true"], QLabel[error="true"] { color: %2; }
        QGroupBox { border: 1px solid %3; border-radius: 9px; margin-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }
        QLineEdit[amountField="true"], QComboBox, QDateEdit {
            background: palette(base); color: palette(text);
            border: 1px solid %3; border-radius: 6px; padding: 0 9px;
        }
        QComboBox, QDateEdit { padding-right: 27px; }
        QComboBox::drop-down, QDateEdit::drop-down {
            subcontrol-origin: padding; subcontrol-position: top right;
            width: 25px; border: none;
        }
        QComboBox::down-arrow, QDateEdit::down-arrow {
            image: url(:/ui/chevron-down.png); width: 12px; height: 12px;
        }
        QLineEdit[amountField="true"]:focus, QComboBox:focus, QDateEdit:focus {
            border: 1px solid %4;
        }
        QPushButton {
            background: palette(button); color: palette(button-text);
            border: 1px solid %3; border-radius: 6px; padding: 0 10px;
        }
        QPushButton:hover { border-color: %1; }
        QPushButton:focus { border: 2px solid %4; }
        QPushButton:pressed { background: palette(mid); }
        QPushButton[primary="true"] {
            background: %4; color: %5; border-color: %4; font-weight: 600;
        }
        QPushButton[primary="true"]:hover { background: %6; }
        QPushButton[primary="true"]:pressed { background: %7; }
        QPushButton[primary="true"]:focus { border: 2px solid palette(text); }
        QPushButton:disabled { color: %8; background: palette(window); border-color: %3; }
        QTableWidget {
            border: 1px solid %3; border-radius: 6px; background: palette(base);
            selection-background-color: %4; selection-color: %5;
        }
        QTableWidget::item { padding: 5px 9px; border: none; }
        QHeaderView::section {
            background: palette(window); color: %1;
            border: none; border-bottom: 1px solid %3; padding: 7px 9px;
        }
    )")
        .arg(dark ? QStringLiteral("#afb3ba") : QStringLiteral("#5b6169"),
             dark ? QStringLiteral("#ff8a80") : QStringLiteral("#b3261e"),
             dark ? QStringLiteral("#424449") : QStringLiteral("#d4d7dc"),
             accent.name(), textOnAccent(accent),
             accent.lighter(110).name(), accent.darker(115).name(),
             dark ? QStringLiteral("#7e8289") : QStringLiteral("#7d828a")));
}

void BudgetView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if ((event->type() == QEvent::PaletteChange ||
         event->type() == QEvent::ApplicationPaletteChange) &&
        appearancePalette_ != QApplication::palette())
        applyAppearance();
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
    totalBudget_->setText(screen.totalBudget);
    totalSpending_->setText(screen.totalSpending);
    remaining_->setText(screen.remaining);
    remaining_->setProperty("negative", screen.remaining.startsWith(QLatin1Char('-')));
    remaining_->style()->unpolish(remaining_);
    remaining_->style()->polish(remaining_);

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
    budgetsEmpty_->setVisible(screen.budgets.isEmpty());
    expensesEmpty_->setVisible(screen.expenses.isEmpty());
    budgetsTable_->setAccessibleDescription(screen.budgets.isEmpty()
        ? QStringLiteral("No budgets yet. Add your first budget above.") : QString());
    expensesTable_->setAccessibleDescription(screen.expenses.isEmpty()
        ? QStringLiteral("No expenses yet. Log your first expense above.")
        : QStringLiteral("Newest first."));
}

void BudgetView::showMessage(const QString &text, bool isError) {
    feedback_->setProperty("error", isError);
    feedback_->style()->unpolish(feedback_);
    feedback_->style()->polish(feedback_);
    feedback_->setText(text);
    feedback_->setVisible(!text.isEmpty());
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
