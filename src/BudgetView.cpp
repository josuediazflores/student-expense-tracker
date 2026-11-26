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
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QShortcut>
#include <QStyle>
#include <QStyledItemDelegate>
#include <QStyleOptionTab>
#include <QStylePainter>
#include <QTabBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include <cmath>
#include <utility>

namespace {
const QString kDateFormat = QStringLiteral("MMM d, yyyy");
constexpr int kExpenseHistoryRowHeight = 48;
constexpr int kVisibleExpenseHistoryRows = 6;

class ExpenseHistoryDelegate : public QStyledItemDelegate {
public:
    using QStyledItemDelegate::QStyledItemDelegate;

    void paint(QPainter *painter, const QStyleOptionViewItem &option,
               const QModelIndex &index) const override {
        QStyleOptionViewItem cell(option);
        initStyleOption(&cell, index);
        const QString category = std::exchange(cell.text, QString());
        const QStyle *style = cell.widget ? cell.widget->style() : QApplication::style();
        style->drawControl(QStyle::CE_ItemViewItem, &cell, painter, cell.widget);

        QFont categoryFont = cell.font;
        categoryFont.setBold(true);
        QFont dateFont = cell.font;
        dateFont.setPointSizeF(qMax<qreal>(9, dateFont.pointSizeF() - 1));
        const QFontMetrics categoryMetrics(categoryFont);
        const QFontMetrics dateMetrics(dateFont);
        const auto bounds = cell.rect.adjusted(6, 0, -6, 0);
        const int top = bounds.top() + (bounds.height() - categoryMetrics.height() - dateMetrics.height()) / 2;
        const QRect categoryLine(bounds.left(), top, bounds.width(), categoryMetrics.height());
        const QRect dateLine(bounds.left(), top + categoryMetrics.height(), bounds.width(), dateMetrics.height());

        painter->save();
        painter->setClipRect(cell.rect, Qt::IntersectClip);
        painter->setFont(categoryFont);
        painter->setPen(cell.palette.color(QPalette::Text));
        painter->drawText(categoryLine, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          categoryMetrics.elidedText(category, Qt::ElideRight, bounds.width()));
        painter->setFont(dateFont);
        painter->setPen(cell.palette.color(QPalette::PlaceholderText));
        painter->drawText(dateLine, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                          dateMetrics.elidedText(index.data(Qt::UserRole).toString(), Qt::ElideRight, bounds.width()));
        painter->restore();
    }
};

class NavigationTabBar : public QTabBar {
protected:
    bool event(QEvent *event) override {
        // The side tabs paint beyond their hit rectangles, so repaint the joins on hover.
        switch (event->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverMove:
        case QEvent::HoverLeave:
            update();
            break;
        default:
            break;
        }
        return QTabBar::event(event);
    }

    void paintEvent(QPaintEvent *) override {
        constexpr int overlap = 5;
        QStylePainter painter(this);
        for (int index : {0, 2}) {
            QStyleOptionTab label;
            initStyleOption(&label, index);
            QStyleOptionTab shape(label);
            // Extend the backgrounds toward the center without moving labels or click targets.
            const bool extendsRight = (index == 0) != isRightToLeft();
            if (extendsRight)
                shape.rect.setRight(shape.rect.right() + overlap);
            else
                shape.rect.setLeft(shape.rect.left() - overlap);
            painter.drawControl(QStyle::CE_TabBarTabShape, shape);
            painter.drawControl(QStyle::CE_TabBarTabLabel, label);
        }

        // Keep the expense circle in front when either of the side tabs is selected.
        QStyleOptionTab expense;
        initStyleOption(&expense, 1);
        painter.drawControl(QStyle::CE_TabBarTab, expense);
    }
};

class NavigationTabs : public QTabWidget {
public:
    NavigationTabs() {
        auto *bar = new NavigationTabBar;
        bar->setDrawBase(false);
        setTabBar(bar);
    }
};

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

QProgressBar *makeSpendingProgress(const QString &accessibleName, int height) {
    auto *progress = new QProgressBar;
    progress->setObjectName(QStringLiteral("homeSpendingProgress"));
    progress->setAccessibleName(accessibleName);
    progress->setTextVisible(false);
    progress->setFixedHeight(height);
    progress->setValue(0);
    return progress;
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

QVBoxLayout *makePageLayout(QWidget *page) {
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    return layout;
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

    tabs_ = new NavigationTabs;
    tabs_->setObjectName(QStringLiteral("navigationTabs"));
    tabs_->setAccessibleName(QStringLiteral("Main navigation"));
    tabs_->tabBar()->setObjectName(QStringLiteral("mainNavigation"));
    const int homeTab = tabs_->addTab(makeHomePage(), QStringLiteral("Home"));
    const int logTab = tabs_->addTab(makeExpensesPage(), QStringLiteral("+"));
    connect(recentExpensesAction_, &QPushButton::clicked, this, [this, logTab] {
        tabs_->setCurrentIndex(logTab);
        if (expensesTable_->rowCount() > 0) expensesTable_->setFocus();
        else expenseAmount_->setFocus();
    });
    const QString logExpense = QStringLiteral("Log expense");
    tabs_->setTabToolTip(logTab, logExpense);
#if QT_CONFIG(accessibility)
    tabs_->tabBar()->setAccessibleTabName(logTab, logExpense);
#endif
    budgetsTab_ = tabs_->addTab(makeBudgetsPage(), QStringLiteral("Budgets"));
    connect(addBudgetAction_, &QPushButton::clicked, this, [this] {
        tabs_->setCurrentIndex(budgetsTab_);
        budgetAmount_->setFocus();
    });
    connect(tabs_, &QTabWidget::currentChanged, this, [this, homeTab, logTab](int tab) {
        if (tab == homeTab) showHome(controller_.screen());
        if (tab == logTab && !tabs_->tabBar()->hasFocus()) expenseAmount_->setFocus();
    });
    connect(tabs_->tabBar(), &QTabBar::tabBarClicked, this, [this, logTab](int tab) {
        if (tab != logTab) return;
        QMetaObject::invokeMethod(expenseAmount_, [this, logTab] {
            if (tabs_->currentIndex() == logTab) expenseAmount_->setFocus();
        }, Qt::QueuedConnection);
    });
    layout->addWidget(tabs_, 1);

    feedback_ = new QLabel;
    feedback_->setAccessibleDescription(QStringLiteral("Status"));
    feedback_->setWordWrap(true);
    feedback_->hide();
    layout->addWidget(feedback_);
    applyAppearance();
}

QWidget *BudgetView::makeHomePage() {
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("homeScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("homePage"));
    auto *layout = makePageLayout(page);

    auto *balanceCard = new QWidget;
    balanceCard->setObjectName(QStringLiteral("balanceCard"));
    homeSummary_ = new QHBoxLayout(balanceCard);
    homeSummary_->setContentsMargins(20, 16, 20, 16);
    homeSummary_->setSpacing(12);

    auto *balance = new QVBoxLayout;
    balance->setSpacing(3);
    balance->addWidget(makeLabel(QStringLiteral("Remaining budget"), 12, true));
    remaining_ = makeLabel(QStringLiteral("$0.00"), 38, true);
    remaining_->setAccessibleDescription(QStringLiteral("Remaining budget"));
    balance->addWidget(remaining_);
    homeSummary_->addLayout(balance, 1);

    auto *spending = new QVBoxLayout;
    spending->setSpacing(7);
    spending->addStretch();
    auto *totals = new QHBoxLayout;
    totalSpending_ = makeLabel(QString(), 11, true);
    totalBudget_ = makeLabel(QString(), 11, true);
    totals->addWidget(totalSpending_);
    totals->addStretch();
    totals->addWidget(totalBudget_);
    spending->addLayout(totals);
    spendingProgress_ = makeSpendingProgress(QStringLiteral("Overall budget used"), 6);
    spending->addWidget(spendingProgress_);
    spending->addSpacing(5);
    homeSummary_->addLayout(spending, 1);
    layout->addWidget(balanceCard);

    homeColumns_ = new QHBoxLayout;
    homeColumns_->setSpacing(12);
    auto *budgetPanel = new QWidget;
    budgetPanel->setProperty("pageCard", true);
    auto *budgets = new QVBoxLayout(budgetPanel);
    budgets->setContentsMargins(15, 12, 15, 12);
    budgets->setSpacing(6);
    budgets->addWidget(makeLabel(QStringLiteral("Active budgets"), 15, true));
    activeBudgetRows_ = new QVBoxLayout;
    activeBudgetRows_->setSpacing(0);
    budgets->addLayout(activeBudgetRows_);

    activeBudgetsEmpty_ = new QWidget;
    auto *empty = new QHBoxLayout(activeBudgetsEmpty_);
    empty->setContentsMargins(0, 5, 0, 4);
    empty->setSpacing(10);
    homeMascot_ = new QLabel;
    homeMascot_->setPixmap(QPixmap(QStringLiteral(":/ui/student-mascot.png")));
    homeMascot_->setScaledContents(true);
    homeMascot_->setFixedSize(54, 54);
    empty->addWidget(homeMascot_);
    auto *emptyContent = new QVBoxLayout;
    emptyContent->setSpacing(5);
    activeBudgetsEmptyTitle_ = makeLabel(QString(), 12, true);
    emptyContent->addWidget(activeBudgetsEmptyTitle_);
    addBudgetAction_ = makeButton(QStringLiteral("Add a budget"), true);
    emptyContent->addWidget(addBudgetAction_, 0, Qt::AlignLeft);
    empty->addLayout(emptyContent);
    empty->addStretch();
    budgets->addWidget(activeBudgetsEmpty_);
    homeColumns_->addWidget(budgetPanel, 6, Qt::AlignTop);

    auto *activityPanel = new QWidget;
    activityPanel->setProperty("pageCard", true);
    auto *activity = new QVBoxLayout(activityPanel);
    activity->setContentsMargins(15, 12, 15, 12);
    activity->setSpacing(0);
    auto *heading = new QHBoxLayout;
    heading->addWidget(makeLabel(QStringLiteral("Recent activity"), 15, true));
    heading->addStretch();
    recentExpensesAction_ = new QPushButton;
    recentExpensesAction_->setObjectName(QStringLiteral("recentExpensesAction"));
    recentExpensesAction_->setFlat(true);
    heading->addWidget(recentExpensesAction_);
    activity->addLayout(heading);

    for (auto &expense : recentExpenses_) {
        expense.container = new QWidget;
        expense.container->setProperty("recentExpenseRow", true);
        auto *row = new QHBoxLayout(expense.container);
        row->setContentsMargins(0, 7, 0, 7);
        expense.category = makeLabel(QString(), 12);
        expense.category->setTextFormat(Qt::PlainText);
        expense.date = makeLabel(QString(), 11, false, true);
        expense.amount = makeLabel(QString(), 12, true);
        auto *details = new QVBoxLayout;
        details->setSpacing(2);
        details->addWidget(expense.category);
        details->addWidget(expense.date);
        row->addLayout(details);
        row->addStretch();
        row->addWidget(expense.amount);
        activity->addWidget(expense.container);
    }
    recentExpensesEmpty_ = makeLabel(QStringLiteral("No expenses yet."), 12, false, true);
    activity->addWidget(recentExpensesEmpty_);
    homeColumns_->addWidget(activityPanel, 5, Qt::AlignTop);
    layout->addLayout(homeColumns_);
    layout->addStretch(1);
    scroll->setWidget(page);
    return scroll;
}

QWidget *BudgetView::makeActiveBudgetRow(const ActiveBudgetRow &budget) {
    auto *widget = new QWidget;
    widget->setProperty("homeBudgetRow", true);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 6, 0, 8);
    layout->setSpacing(4);

    auto *heading = new QHBoxLayout;
    QString categoryText = budget.category;
    categoryText.replace(QStringLiteral("&"), QStringLiteral("&&"));
    auto *category = new QPushButton(categoryText);
    category->setObjectName(QStringLiteral("homeBudgetLink"));
    category->setFlat(true);
    category->setAccessibleName(QStringLiteral("View expenses for %1").arg(budget.category));
    connect(category, &QPushButton::clicked, this, [this, row = budget.shownRow] {
        tabs_->setCurrentIndex(budgetsTab_);
        budgetsTable_->selectRow(row);
        if (expandedBudgetRow_ != row) toggleBudgetDetails(row);
    });
    heading->addWidget(category, 0, Qt::AlignLeft);
    heading->addStretch();
    auto *amount = makeLabel(budget.balance, 12, true);
    amount->setProperty("negative", budget.overBudget);
    heading->addWidget(amount);
    layout->addLayout(heading);

    auto *details = new QHBoxLayout;
    details->addWidget(makeLabel(budget.endDate, 11, false, true));
    details->addStretch();
    details->addWidget(makeLabel(budget.spending, 11, false, true));
    layout->addLayout(details);

    auto *progress = makeSpendingProgress(QStringLiteral("%1 budget used").arg(budget.category), 4);
    progress->setProperty("negative", budget.overBudget);
    progress->setValue(budget.spendingPercent);
    layout->addWidget(progress);
    return widget;
}

QWidget *BudgetView::makeBudgetsPage() {
    auto *page = new QWidget;
    auto *layout = makePageLayout(page);
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
    budgetLayout->addLayout(budgetButtons);
    budgetBox->setMaximumWidth(520);
    layout->addWidget(budgetBox);

    auto *heading = new QHBoxLayout;
    heading->addWidget(makeLabel(QStringLiteral("Budgets"), 12, true));
    heading->addStretch();
    heading->addWidget(makeLabel(QStringLiteral("Double-click a budget to see its expenses"), 11, false, true));
    layout->addLayout(heading);
    budgetsTable_ = makeTable({QStringLiteral("Category"), QStringLiteral("Total"),
                               QStringLiteral("Period"), QStringLiteral("Per day")});
    budgetsTable_->setAccessibleName(QStringLiteral("Budgets"));
    alignColumns(budgetsTable_, {1, 3});
    for (int col : {0, 1, 3})
        budgetsTable_->horizontalHeader()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    budgetsEmpty_ = makeEmptyState(budgetsTable_, QStringLiteral("No budgets yet"),
                                   QStringLiteral("Add your first budget above."));
    layout->addWidget(budgetsTable_, 1);
    layout->addWidget(makeBudgetDetailsPanel(), 2);

    connect(budgetsTable_, &QTableWidget::cellDoubleClicked, this,
            [this](int row, int) { toggleBudgetDetails(row); });
    auto *showDetails = new QShortcut(budgetsTable_);
    showDetails->setKeys({QKeySequence(Qt::Key_Return), QKeySequence(Qt::Key_Enter)});
    showDetails->setContext(Qt::WidgetShortcut);
    connect(showDetails, &QShortcut::activated, this,
            [this] { toggleBudgetDetails(selectedRow(budgetsTable_)); });

    connect(budgetsTable_, &QTableWidget::itemSelectionChanged, this,
            [this, updateBudget, deleteBudget] {
                const bool selected = selectedRow(budgetsTable_) >= 0;
                updateBudget->setEnabled(selected);
                deleteBudget->setEnabled(selected);
            });
    return page;
}

QWidget *BudgetView::makeBudgetDetailsPanel() {
    budgetDetailsPanel_ = new QGroupBox;
    auto *layout = new QVBoxLayout(budgetDetailsPanel_);
    layout->setContentsMargins(14, 22, 14, 14);

    auto *header = new QHBoxLayout;
    auto *summary = new QVBoxLayout;
    budgetDetailsPeriod_ = makeLabel(QString(), 11, false, true);
    budgetDetailsTotal_ = makeLabel(QString(), 12, true);
    summary->addWidget(budgetDetailsPeriod_);
    summary->addWidget(budgetDetailsTotal_);
    header->addLayout(summary, 1);
    auto *hideDetails = makeButton(QStringLiteral("Hide expenses"));
    connect(hideDetails, &QPushButton::clicked, this, [this] {
        hideBudgetDetails();
        budgetsTable_->setFocus();
    });
    header->addWidget(hideDetails);
    layout->addLayout(header);

    budgetDetailsTable_ = makeTable({QStringLiteral("Date"), QStringLiteral("Amount")});
    budgetDetailsTable_->setAccessibleName(QStringLiteral("Expenses for this budget"));
    budgetDetailsTable_->setAccessibleDescription(QStringLiteral("Newest first."));
    alignColumns(budgetDetailsTable_, {1});
    budgetDetailsEmpty_ = makeEmptyState(budgetDetailsTable_, QStringLiteral("No matching expenses"),
                                         QStringLiteral("Expenses must match this category and date range."));
    layout->addWidget(budgetDetailsTable_, 1);
    budgetDetailsPanel_->hide();
    return budgetDetailsPanel_;
}

void BudgetView::toggleBudgetDetails(int row) {
    if (row < 0) return;
    if (row == expandedBudgetRow_) {
        hideBudgetDetails();
        return;
    }

    const auto details = controller_.budgetDetails(row);
    if (!details) return;
    expandedBudgetRow_ = row;
    budgetDetailsPanel_->setTitle(QStringLiteral("Expenses for %1").arg(details->category));
    budgetDetailsPeriod_->setText(details->period);
    budgetDetailsTotal_->setText(QStringLiteral("Total spent: %1").arg(details->totalSpent));
    budgetDetailsTable_->setRowCount(details->expenses.size());
    for (int index = 0; index < details->expenses.size(); ++index) {
        const auto &expense = details->expenses.at(index);
        fillRow(budgetDetailsTable_, index, {expense.date, expense.amount});
    }
    budgetDetailsTable_->selectionModel()->clear();
    budgetDetailsTable_->scrollToTop();
    budgetDetailsEmpty_->setVisible(details->expenses.isEmpty());
    budgetDetailsPanel_->show();
    // Keep the expanded row visible when the details panel reduces the table height.
    budgetDetailsPanel_->parentWidget()->layout()->activate();
    budgetsTable_->scrollToItem(budgetsTable_->item(row, 0));
}

void BudgetView::hideBudgetDetails() {
    expandedBudgetRow_ = -1;
    budgetDetailsPanel_->hide();
}

QWidget *BudgetView::makeExpensesPage() {
    auto *scroll = new QScrollArea;
    scroll->setObjectName(QStringLiteral("expenseScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    auto *page = new QWidget;
    page->setObjectName(QStringLiteral("expensePage"));
    auto *layout = makePageLayout(page);
    expenseColumns_ = new QHBoxLayout;
    expenseColumns_->setSpacing(12);

    auto *expenseBox = new QWidget;
    expenseBox->setProperty("pageCard", true);
    auto *expenseLayout = new QVBoxLayout(expenseBox);
    expenseLayout->setContentsMargins(15, 12, 15, 12);
    expenseLayout->setSpacing(10);
    expenseLayout->addWidget(makeLabel(QStringLiteral("Log an expense"), 15, true));

    auto *amountCard = new QWidget;
    amountCard->setObjectName(QStringLiteral("quickExpenseAmountCard"));
    auto *amountLayout = new QVBoxLayout(amountCard);
    amountLayout->setContentsMargins(12, 9, 12, 9);
    amountLayout->setSpacing(3);
    auto *amountLabel = makeLabel(QStringLiteral("Amount"), 11, false, true);
    amountLayout->addWidget(amountLabel);
    auto *amountRow = new QHBoxLayout;
    amountRow->setSpacing(4);
    amountRow->addWidget(makeLabel(QStringLiteral("$"), 29, true, true));
    expenseAmount_ = new QLineEdit;
    expenseAmount_->setObjectName(QStringLiteral("quickExpenseAmount"));
    expenseAmount_->setAccessibleName(QStringLiteral("Amount ($)"));
    expenseAmount_->setPlaceholderText(QStringLiteral("0.00"));
    expenseAmount_->setFixedHeight(44);
    QFont amountFont = expenseAmount_->font();
    amountFont.setPointSize(29);
    amountFont.setBold(true);
    expenseAmount_->setFont(amountFont);
    amountLabel->setBuddy(expenseAmount_);
    amountRow->addWidget(expenseAmount_, 1);
    amountLayout->addLayout(amountRow);
    expenseLayout->addWidget(amountCard);

    expenseCategory_ = makeCategoryPicker();
    expenseDate_ = makeDateEdit(QDate::currentDate());
    auto *fields = new QHBoxLayout;
    fields->setSpacing(10);
    const auto addColumn = [fields](const QString &name, QWidget *input) {
        auto *column = new QVBoxLayout;
        column->setSpacing(4);
        auto *label = makeLabel(name, 11, false, true);
        label->setBuddy(input);
        input->setAccessibleName(name);
        input->setFixedHeight(32);
        column->addWidget(label);
        column->addWidget(input);
        fields->addLayout(column, 1);
    };
    addColumn(QStringLiteral("Category"), expenseCategory_);
    addColumn(QStringLiteral("Date"), expenseDate_);
    expenseLayout->addLayout(fields);
    auto *saveExpenseButton = makeButton(QStringLiteral("Save expense"), true);
    connect(saveExpenseButton, &QPushButton::clicked, this, &BudgetView::saveExpense);
    connect(expenseAmount_, &QLineEdit::returnPressed, this, &BudgetView::saveExpense);
    expenseLayout->addWidget(saveExpenseButton);
    expenseColumns_->addWidget(expenseBox, 5, Qt::AlignTop);

    auto *historyBox = new QWidget;
    historyBox->setProperty("pageCard", true);
    auto *history = new QVBoxLayout(historyBox);
    history->setContentsMargins(15, 12, 15, 12);
    history->setSpacing(6);
    auto *historyHeading = new QHBoxLayout;
    auto *historyTitle = new QVBoxLayout;
    historyTitle->setSpacing(3);
    historyTitle->addWidget(makeLabel(QStringLiteral("Expense history"), 15, true));
    historyTitle->addWidget(makeLabel(QStringLiteral("Newest first"), 11, false, true));
    historyHeading->addLayout(historyTitle);
    historyHeading->addStretch();
    auto *deleteExpense = makeButton(QStringLiteral("Delete expense"));
    deleteExpense->setObjectName(QStringLiteral("deleteExpenseAction"));
    deleteExpense->hide();
    historyHeading->addWidget(deleteExpense);
    history->addLayout(historyHeading);
    connect(deleteExpense, &QPushButton::clicked, this, [this] {
        controller_.requestDeleteExpense(selectedRow(expensesTable_));
    });

    expensesTable_ = makeTable({QStringLiteral("Category and date"), QStringLiteral("Amount")});
    expensesTable_->setObjectName(QStringLiteral("expenseHistory"));
    expensesTable_->setAccessibleName(QStringLiteral("Expense history"));
    expensesTable_->horizontalHeader()->hide();
    expensesTable_->horizontalHeader()->setMinimumSectionSize(0);
    expensesTable_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    expensesTable_->verticalHeader()->setDefaultSectionSize(kExpenseHistoryRowHeight);
    alignColumns(expensesTable_, {1});
    expensesTable_->setItemDelegateForColumn(0, new ExpenseHistoryDelegate(expensesTable_));
    expensesEmpty_ = makeEmptyState(expensesTable_, QStringLiteral("No expenses yet"),
                                    QStringLiteral("Save an expense to see it here."));
    history->addWidget(expensesTable_);
    connect(expensesTable_, &QTableWidget::itemSelectionChanged, this,
            [this, deleteExpense] { deleteExpense->setVisible(selectedRow(expensesTable_) >= 0); });
    expenseColumns_->addWidget(historyBox, 6, Qt::AlignTop);
    layout->addLayout(expenseColumns_);
    layout->addStretch(1);
    scroll->setWidget(page);
    QWidget::setTabOrder(expenseAmount_, expenseCategory_);
    QWidget::setTabOrder(expenseCategory_, expenseDate_);
    QWidget::setTabOrder(expenseDate_, saveExpenseButton);
    QWidget::setTabOrder(saveExpenseButton, expensesTable_);
    QWidget::setTabOrder(expensesTable_, deleteExpense);
    return scroll;
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
        QTabWidget#navigationTabs::pane {
            top: -11px; background: %10; border: 1px solid %3; border-radius: 11px;
        }
        QTabWidget#navigationTabs::tab-bar { alignment: center; }
        QScrollArea#homeScroll, QWidget#homePage,
        QScrollArea#expenseScroll, QWidget#expensePage { background: %10; border: none; }
        QWidget#balanceCard, QWidget#quickExpenseAmountCard { background: %11; border-radius: 9px; }
        QWidget[pageCard="true"] { background: %12; border-radius: 9px; }
        QWidget[recentExpenseRow="true"], QWidget[homeBudgetRow="true"] { border-bottom: 1px solid %3; }
        QProgressBar#homeSpendingProgress { background: %3; border: none; border-radius: 2px; }
        QProgressBar#homeSpendingProgress::chunk { background: %4; border-radius: 2px; }
        QProgressBar#homeSpendingProgress[negative="true"]::chunk { background: %2; }
        QTabBar#mainNavigation::tab {
            height: 30px; padding: 0 12px; margin: 4px 0;
            background: palette(button); color: palette(button-text);
            border: 1px solid %3; border-radius: 6px;
        }
        QTabBar#mainNavigation::tab:middle {
            width: 38px; height: 38px; padding: 0; margin: 0;
            border-radius: 20px; font-weight: 600;
        }
        QTabBar#mainNavigation::tab:hover { border-color: %1; }
        QTabBar#mainNavigation::tab:selected {
            background: %4; color: %5; border-color: %4;
        }
        QTabBar#mainNavigation::tab:selected:focus { border-color: palette(text); }
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
        QLineEdit#quickExpenseAmount {
            background: %11; color: palette(text); border: 1px solid %11;
            border-radius: 5px; padding: 0 3px;
        }
        QLineEdit#quickExpenseAmount:focus { border-color: %4; }
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
        QPushButton#recentExpensesAction, QPushButton#homeBudgetLink {
            min-height: 24px; color: %9; background: none; border: 1px solid transparent; padding: 0 2px;
        }
        QPushButton#homeBudgetLink { font-size: 12pt; font-weight: bold; text-align: left; }
        QPushButton#recentExpensesAction:hover, QPushButton#homeBudgetLink:hover { text-decoration: underline; }
        QPushButton#recentExpensesAction:focus, QPushButton#homeBudgetLink:focus { border-color: %4; }
        QPushButton#deleteExpenseAction { color: %2; }
        QTableWidget {
            border: 1px solid %3; border-radius: 6px; background: palette(base);
            selection-background-color: %4; selection-color: %5;
        }
        QTableWidget::item { padding: 5px 9px; border: none; }
        QTableWidget#expenseHistory {
            border: none; background: %12;
            selection-background-color: %11; selection-color: palette(text);
        }
        QTableWidget#expenseHistory::item { padding: 0 6px; border-bottom: 1px solid %3; }
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
             dark ? QStringLiteral("#7e8289") : QStringLiteral("#7d828a"),
             dark ? QStringLiteral("#92baff") : QStringLiteral("#1758b5"),
             dark ? QStringLiteral("#272727") : QStringLiteral("#f6f6f6"),
             dark ? QStringLiteral("#203757") : QStringLiteral("#e9f2ff"),
             dark ? QStringLiteral("#2c2c2f") : QStringLiteral("#ffffff")));
}

void BudgetView::changeEvent(QEvent *event) {
    QWidget::changeEvent(event);
    if ((event->type() == QEvent::PaletteChange ||
         event->type() == QEvent::ApplicationPaletteChange) &&
        appearancePalette_ != QApplication::palette())
        applyAppearance();
}

void BudgetView::resizeEvent(QResizeEvent *event) {
    QWidget::resizeEvent(event);
    if (!expenseColumns_) return;

    const bool compact = event->size().width() < 660;
    const auto direction = compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;
    if (homeColumns_->direction() == direction) return;
    homeSummary_->setDirection(direction);
    homeSummary_->setStretch(0, compact ? 0 : 1);
    homeSummary_->setStretch(1, compact ? 0 : 1);
    homeColumns_->setDirection(direction);
    homeColumns_->setStretch(0, compact ? 0 : 6);
    homeColumns_->setStretch(1, compact ? 0 : 5);
    expenseColumns_->setDirection(direction);
    expenseColumns_->setStretch(0, compact ? 0 : 5);
    expenseColumns_->setStretch(1, compact ? 0 : 6);
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
    expenseAmount_->setFocus();
}

void BudgetView::showHome(const BudgetScreen &screen) {
    totalBudget_->setText(QStringLiteral("%1 budgeted").arg(screen.totalBudget));
    totalSpending_->setText(QStringLiteral("%1 spent").arg(screen.totalSpending));
    remaining_->setText(screen.remaining);
    const bool overBudget = screen.remaining.startsWith(QLatin1Char('-'));
    for (QWidget *widget : std::array<QWidget *, 2>{remaining_, spendingProgress_}) {
        widget->setProperty("negative", overBudget);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    spendingProgress_->setValue(screen.spendingPercent);

    while (auto *item = activeBudgetRows_->takeAt(0)) {
        delete item->widget();
        delete item;
    }
    for (const ActiveBudgetRow &budget : screen.activeBudgets)
        activeBudgetRows_->addWidget(makeActiveBudgetRow(budget));
    const bool noBudgets = screen.budgets.isEmpty();
    activeBudgetsEmpty_->setVisible(screen.activeBudgets.isEmpty());
    activeBudgetsEmptyTitle_->setText(noBudgets ? QStringLiteral("No budgets yet.")
                                              : QStringLiteral("No active budgets."));
    homeMascot_->setVisible(noBudgets);

    const bool noExpenses = screen.expenses.isEmpty();
    recentExpensesEmpty_->setVisible(noExpenses);
    recentExpensesAction_->setText(noExpenses ? QStringLiteral("Log your first expense")
                                              : QStringLiteral("View all expenses"));
    for (qsizetype index = 0; index < static_cast<qsizetype>(recentExpenses_.size()); ++index) {
        auto &row = recentExpenses_[index];
        const bool hasExpense = index < screen.expenses.size();
        row.container->setVisible(hasExpense);
        if (hasExpense) {
            const auto &expense = screen.expenses.at(index);
            row.category->setText(expense.category);
            row.date->setText(expense.date);
            row.amount->setText(expense.amount);
        }
    }
}

void BudgetView::showScreen(const BudgetScreen &screen) {
    // A refresh can change the budget at a row index.
    hideBudgetDetails();
    showHome(screen);
    budgetsTable_->setRowCount(screen.budgets.size());
    for (int row = 0; row < screen.budgets.size(); ++row) {
        const BudgetRow &budget = screen.budgets.at(row);
        fillRow(budgetsTable_, row, {budget.category, budget.total, budget.period, budget.perDay});
    }
    expensesTable_->setRowCount(screen.expenses.size());
    const int visibleExpenseRows = qMin(expensesTable_->rowCount(), kVisibleExpenseHistoryRows);
    expensesTable_->setFixedHeight(visibleExpenseRows > 0
        ? kExpenseHistoryRowHeight * visibleExpenseRows : 96);
    QFont amountFont = expensesTable_->font();
    amountFont.setBold(true);
    for (int row = 0; row < screen.expenses.size(); ++row) {
        const ExpenseRow &expense = screen.expenses.at(row);
        fillRow(expensesTable_, row, {expense.category, expense.amount});
        auto *category = expensesTable_->item(row, 0);
        category->setData(Qt::UserRole, expense.date);
        const QString description = QStringLiteral("%1, %2").arg(expense.category, expense.date);
        category->setData(Qt::AccessibleTextRole, description);
        category->setToolTip(description);
        auto *amount = expensesTable_->item(row, 1);
        amount->setFont(amountFont);
        amount->setToolTip(expense.amount);
    }

    // Rows may have moved or gone, so a delete must not act on a stale highlight. This runs after
    // the refill because shrinking a table makes Qt re-select a row near the current one; clear()
    // drops that current index too, and the selection signals reset the delete actions.
    budgetsTable_->selectionModel()->clear();
    expensesTable_->selectionModel()->clear();
    budgetsEmpty_->setVisible(screen.budgets.isEmpty());
    expensesEmpty_->setVisible(screen.expenses.isEmpty());
    budgetsTable_->setAccessibleDescription(screen.budgets.isEmpty()
        ? QStringLiteral("No budgets yet. Add your first budget above.")
        : QStringLiteral("Double-click a budget or press Return to see its expenses."));
    expensesTable_->setAccessibleDescription(screen.expenses.isEmpty()
        ? QStringLiteral("No expenses yet. Use the entry form to log one.")
        : QStringLiteral("Newest first. Select an expense to delete it."));
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
