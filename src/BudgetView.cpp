#include "BudgetView.h"

#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>

BudgetView::BudgetView(BudgetController &controller, QWidget *parent)
    : QWidget(parent), controller_(controller) {
    setWindowTitle(QStringLiteral("SCU Student Budget"));
    resize(720, 440);

    auto *layout = new QVBoxLayout(this);
    auto *heading = new QLabel(QStringLiteral("Student budgets"));
    QFont headingFont = heading->font();
    headingFont.setPointSize(18);
    headingFont.setBold(true);
    heading->setFont(headingFont);
    layout->addWidget(heading);
    layout->addWidget(new QLabel(QStringLiteral("Set an amount and date range for each spending category.")));

    category_ = new QComboBox;
    category_->addItems({QStringLiteral("Meal plan"), QStringLiteral("Groceries"),
                         QStringLiteral("Dining out"), QStringLiteral("Transport"),
                         QStringLiteral("School supplies"), QStringLiteral("Other")});
    amount_ = new QDoubleSpinBox;
    amount_->setRange(0.0, 100000000.0);
    amount_->setDecimals(2);
    amount_->setPrefix(QStringLiteral("$"));
    amount_->setSingleStep(10.0);
    startDate_ = new QDateEdit(QDate::currentDate());
    endDate_ = new QDateEdit(QDate::currentDate().addMonths(1));
    startDate_->setCalendarPopup(true);
    endDate_->setCalendarPopup(true);
    startDate_->setDisplayFormat(QStringLiteral("MMM d, yyyy"));
    endDate_->setDisplayFormat(QStringLiteral("MMM d, yyyy"));

    auto *form = new QFormLayout;
    form->addRow(QStringLiteral("Category"), category_);
    form->addRow(QStringLiteral("Amount"), amount_);
    form->addRow(QStringLiteral("Start date"), startDate_);
    form->addRow(QStringLiteral("End date"), endDate_);
    layout->addLayout(form);

    auto *save = new QPushButton(QStringLiteral("Add budget"));
    connect(save, &QPushButton::clicked, this, [this] { saveBudget(); });
    layout->addWidget(save);
    feedback_ = new QLabel;
    feedback_->setAccessibleName(QStringLiteral("Budget status"));
    layout->addWidget(feedback_);

    total_ = new QLabel;
    layout->addWidget(total_);
    table_ = new QTableWidget(0, 4);
    table_->setHorizontalHeaderLabels({QStringLiteral("Category"), QStringLiteral("Total"),
                                       QStringLiteral("Period"), QStringLiteral("Per day")});
    table_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(table_);
    refresh();
}

void BudgetView::saveBudget() {
    QString error;
    const qint64 cents = std::llround(amount_->value() * 100.0);
    if (!controller_.createBudget(category_->currentText(), cents,
                                  startDate_->date(), endDate_->date(), &error)) {
        feedback_->setText(error);
        return;
    }
    feedback_->setText(QStringLiteral("Budget added."));
    refresh();
}

void BudgetView::refresh() {
    const QLocale locale(QLocale::English, QLocale::UnitedStates);
    total_->setText(QStringLiteral("Total budget: %1")
                        .arg(locale.toCurrencyString(controller_.totalCents() / 100.0)));
    const auto &budgets = controller_.budgets();
    table_->setRowCount(budgets.size());
    for (int row = 0; row < budgets.size(); ++row) {
        const Budget &budget = budgets.at(row);
        const QString period = budget.startDate.toString(QStringLiteral("MMM d, yyyy")) +
                               QStringLiteral(" to ") +
                               budget.endDate.toString(QStringLiteral("MMM d, yyyy"));
        const QStringList values = {
            budget.category,
            locale.toCurrencyString(budget.amountCents / 100.0),
            period,
            locale.toCurrencyString(budget.amountPerDay())
        };
        for (int col = 0; col < values.size(); ++col)
            table_->setItem(row, col, new QTableWidgetItem(values.at(col)));
    }
}
