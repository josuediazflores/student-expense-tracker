#include "BudgetController.h"
#include "BudgetView.h"

#include <QtTest>
#include <QComboBox>
#include <QDateEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>

class BudgetTests : public QObject {
    Q_OBJECT

private slots:
    void acceptsValidBudget();
    void rejectsNegativeAmount();
    void rejectsReversedDates();
    void countsBothEndpoints();
    void totalsCategories();
    void viewAddsAndDisplaysBudget();
};

void BudgetTests::acceptsValidBudget() {
    BudgetModel model;
    BudgetController controller(model);
    QString error;
    QVERIFY(controller.createBudget("Meal plan", 12000, QDate(2026, 10, 1), QDate(2026, 10, 30), &error));
    QCOMPARE(error, QString());
    QCOMPARE(controller.budgets().size(), 1);
    QCOMPARE(controller.budgets().first().category, QString("Meal plan"));
    QCOMPARE(controller.totalCents(), qint64(12000));
}

void BudgetTests::rejectsNegativeAmount() {
    BudgetModel model;
    QString error;
    QVERIFY(!model.addBudget({"Food", -1, QDate(2026, 10, 1), QDate(2026, 10, 2)}, &error));
    QVERIFY(error.contains("negative"));
    QCOMPARE(model.budgets().size(), 0);
}

void BudgetTests::rejectsReversedDates() {
    BudgetModel model;
    QString error;
    QVERIFY(!model.addBudget({"Food", 100, QDate(2026, 10, 2), QDate(2026, 10, 1)}, &error));
    QVERIFY(error.contains("before start"));
    QCOMPARE(model.budgets().size(), 0);
}

void BudgetTests::countsBothEndpoints() {
    Budget budget{"Food", 1000, QDate(2026, 10, 1), QDate(2026, 10, 1)};
    QCOMPARE(budget.amountPerDay(), 10.0);
    budget.endDate = QDate(2026, 10, 2);
    QCOMPARE(budget.amountPerDay(), 5.0);
}

void BudgetTests::totalsCategories() {
    BudgetModel model;
    QVERIFY(model.addBudget({"Meal plan", 12000, QDate(2026, 10, 1), QDate(2026, 10, 30)}));
    QVERIFY(model.addBudget({"Transport", 2500, QDate(2026, 10, 1), QDate(2026, 10, 30)}));
    QCOMPARE(model.totalCents(), qint64(14500));
    QCOMPARE(model.budgets().size(), 2);
}

void BudgetTests::viewAddsAndDisplaysBudget() {
    BudgetModel model;
    BudgetController controller(model);
    BudgetView view(controller);
    auto *amount = view.findChild<QDoubleSpinBox *>();
    const auto dates = view.findChildren<QDateEdit *>();
    auto *table = view.findChild<QTableWidget *>();
    auto *button = view.findChild<QPushButton *>();
    QVERIFY(amount && table && button);
    QCOMPARE(dates.size(), 2);

    amount->setValue(30.00);
    dates.at(0)->setDate(QDate(2026, 10, 1));
    dates.at(1)->setDate(QDate(2026, 10, 3));
    button->click();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(table->item(0, 0)->text(), QString("Meal plan"));
    QCOMPARE(table->item(0, 1)->text(), QString("$30.00"));
    QCOMPARE(table->item(0, 3)->text(), QString("$10.00"));

    dates.at(1)->setDate(QDate(2026, 9, 30));
    button->click();
    QCOMPARE(table->rowCount(), 1);
    QCOMPARE(controller.totalCents(), qint64(3000));
}

QTEST_MAIN(BudgetTests)
#include "BudgetTests.moc"
