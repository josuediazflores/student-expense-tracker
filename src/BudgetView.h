#pragma once

#include "BudgetController.h"

#include <QWidget>

class QComboBox;
class QDateEdit;
class QDoubleSpinBox;
class QLabel;
class QTableWidget;

class BudgetView : public QWidget {
public:
    explicit BudgetView(BudgetController &controller, QWidget *parent = nullptr);

private:
    void saveBudget();
    void refresh();

    BudgetController &controller_;
    QComboBox *category_;
    QDoubleSpinBox *amount_;
    QDateEdit *startDate_;
    QDateEdit *endDate_;
    QLabel *total_;
    QLabel *feedback_;
    QTableWidget *table_;
};
