// Author: Huy Tran
// Verify that expenses are stored as records with an amount and a date.
#include "doctest.h"

#include "BudgetModel.h"

#include <limits>

namespace {
const QDate kDate(2026, 3, 2);

// A budget and one dated, fractional expense, so a rejected expense that
// changes anything (list, total or remaining) is caught.
BudgetModel modelWithOneExpense() {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 3, 1), QDate(2026, 3, 31)}));
    REQUIRE(model.addExpense("12.5025", QDate(2026, 3, 1)));
    return model;
}

void checkRejectedExpenseChangedNothing(const BudgetModel &model, const QString &error,
                                        const QString &expectedMessage) {
    CHECK(error == expectedMessage);
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).amountCents.toString() == QStringLiteral("1250.25"));
    CHECK(model.expenses().at(0).date == QDate(2026, 3, 1));
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8749.75"));
    CHECK(model.totalCents() == 10000);
    CHECK(model.budgets().size() == 1);
}
}

TEST_CASE("a new model has no expenses") {
    BudgetModel model;
    CHECK(model.expenses().isEmpty());
}

TEST_CASE("a dated expense is stored with its amount and date") {
    BudgetModel model;
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense(QStringLiteral("12.50"), kDate, &error));
    CHECK(error.isEmpty());
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).amountCents == 1250);
    CHECK(model.expenses().at(0).date == kDate);
    CHECK(model.totalSpendingCents() == 1250);
}

TEST_CASE("a dated expense accepts a C string amount") {
    BudgetModel model;

    REQUIRE(model.addExpense("3.07", kDate));
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).amountCents == 307);
    CHECK(model.expenses().at(0).date == kDate);
}

TEST_CASE("expenses are listed in the order they were entered") {
    BudgetModel model;
    REQUIRE(model.addExpense("1", QDate(2026, 3, 5)));
    REQUIRE(model.addExpense("2", QDate(2026, 3, 1)));
    REQUIRE(model.addExpense("3", QDate(2026, 3, 9)));

    REQUIRE(model.expenses().size() == 3);
    CHECK(model.expenses().at(0).amountCents == 100);
    CHECK(model.expenses().at(0).date == QDate(2026, 3, 5));
    CHECK(model.expenses().at(1).amountCents == 200);
    CHECK(model.expenses().at(1).date == QDate(2026, 3, 1));
    CHECK(model.expenses().at(2).amountCents == 300);
    CHECK(model.expenses().at(2).date == QDate(2026, 3, 9));
    CHECK(model.totalSpendingCents() == 600);
}

TEST_CASE("a fractional-cent dollar amount is stored exactly") {
    BudgetModel model;

    REQUIRE(model.addExpense("0.125", kDate));
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).amountCents.toString() == QStringLiteral("12.5"));
}

TEST_CASE("an amount with surrounding whitespace is accepted") {
    BudgetModel model;

    REQUIRE(model.addExpense(QStringLiteral("  4.20 "), kDate));
    CHECK(model.expenses().at(0).amountCents == 420);
}

TEST_CASE("a zero expense is recorded") {
    BudgetModel model;

    REQUIRE(model.addExpense("0", kDate));
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).amountCents == 0);
    CHECK(model.totalSpendingCents() == 0);
}

TEST_CASE("a future date is accepted") {
    BudgetModel model;
    const QDate future = QDate::currentDate().addYears(5);

    REQUIRE(model.addExpense("9.99", future));
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).date == future);
}

TEST_CASE("the no-date overload records today's date") {
    BudgetModel model;

    const QDate before = QDate::currentDate();
    REQUIRE(model.addExpense("5.00"));
    const QDate after = QDate::currentDate();

    REQUIRE(model.expenses().size() == 1);
    // Accept either side of a midnight rollover during the call.
    CHECK(model.expenses().at(0).date >= before);
    CHECK(model.expenses().at(0).date <= after);
    CHECK(model.expenses().at(0).amountCents == 500);
}

TEST_CASE("the no-date QString overload also records today's date") {
    BudgetModel model;

    const QDate before = QDate::currentDate();
    REQUIRE(model.addExpense(QStringLiteral("6.00")));
    const QDate after = QDate::currentDate();

    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).date >= before);
    CHECK(model.expenses().at(0).date <= after);
}

TEST_CASE("a null date is rejected") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense("5.00", QDate(), &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Choose a valid expense date."));
}

TEST_CASE("an impossible calendar date is rejected") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense("5.00", QDate(2026, 2, 30), &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Choose a valid expense date."));
}

TEST_CASE("an invalid date is rejected for a QString amount too") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense(QStringLiteral("5.00"), QDate(2026, 13, 1), &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Choose a valid expense date."));
}

TEST_CASE("an amount error is reported before a date error") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense("abc", QDate(), &error));
    checkRejectedExpenseChangedNothing(model, error,
                                       QStringLiteral("Enter a decimal dollar amount such as 12.99."));
}

TEST_CASE("a negative amount is reported before a date error") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense("-1", QDate(), &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Expense amount cannot be negative."));
}

TEST_CASE("an empty amount is reported as required even with a valid date") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense(QStringLiteral("   "), kDate, &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Expense amount is required."));
}

TEST_CASE("a null amount with a date is rejected as required") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense(nullptr, kDate, &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Expense amount is required."));
}

TEST_CASE("a null character pointer with a date is rejected as required") {
    BudgetModel model = modelWithOneExpense();
    const char *missingAmount = nullptr;
    QString error;

    CHECK_FALSE(model.addExpense(missingAmount, kDate, &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Expense amount is required."));
}

TEST_CASE("an amount with the wrong format and a date is rejected") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense("$5", kDate, &error));
    checkRejectedExpenseChangedNothing(model, error,
                                       QStringLiteral("Enter a decimal dollar amount such as 12.99."));
}

TEST_CASE("a native double with a date is rejected by the text-only API") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense(12.5, kDate, &error));
    checkRejectedExpenseChangedNothing(model, error,
                                       QStringLiteral("Enter the expense as decimal text in dollars."));
}

TEST_CASE("a native integer with a date is rejected by the text-only API") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense(1250, kDate, &error));
    checkRejectedExpenseChangedNothing(model, error,
                                       QStringLiteral("Enter the expense as decimal text in dollars."));
}

TEST_CASE("a typed cents value with a date cannot bypass the dollar-string contract") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense(DecimalCents(1250), kDate, &error));
    checkRejectedExpenseChangedNothing(model, error,
                                       QStringLiteral("Enter the expense as decimal text in dollars."));
}

TEST_CASE("an expense that would overflow total spending is rejected and the list is unchanged") {
    BudgetModel model;
    REQUIRE(model.addExpense("92233720368547758.07", kDate));
    QString error;

    CHECK_FALSE(model.addExpense("0.01", QDate(2026, 3, 3), &error));
    CHECK(error == QStringLiteral("Total spending is too large."));
    REQUIRE(model.expenses().size() == 1);
    CHECK(model.expenses().at(0).date == kDate);
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("9223372036854775807"));
}

TEST_CASE("an overflow is reported before a date error") {
    BudgetModel model;
    REQUIRE(model.addExpense("92233720368547758.07", kDate));
    QString error;

    CHECK_FALSE(model.addExpense("0.01", QDate(), &error));
    CHECK(error == QStringLiteral("Total spending is too large."));
    CHECK(model.expenses().size() == 1);
}

TEST_CASE("a dated expense that fits exactly at the maximum is accepted") {
    BudgetModel model;
    REQUIRE(model.addExpense("92233720368547758.06", kDate));

    REQUIRE(model.addExpense("0.01", QDate(2026, 3, 3)));
    CHECK(model.expenses().size() == 2);
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("9223372036854775807"));
}

TEST_CASE("an amount above the exact maximum is rejected with a date") {
    BudgetModel model = modelWithOneExpense();
    QString error;

    CHECK_FALSE(model.addExpense("92233720368547758.071", kDate, &error));
    checkRejectedExpenseChangedNothing(model, error, QStringLiteral("Total spending is too large."));
}
