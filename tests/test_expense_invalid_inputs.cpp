// Author: Xiaochao Zhou
#include "doctest.h"

#include "BudgetModel.h"

#include <limits>

namespace {
BudgetModel modelWithExistingExpense() {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense("12.5025"));
    return model;
}

void checkRejectedInputPreservesBalances(const BudgetModel &model, const QString &error) {
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8749.75"));
    CHECK(model.totalCents() == 10000);
}
}

TEST_CASE("a null expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(nullptr, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a null character pointer expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    const char *missingAmount = nullptr;
    QString error;

    CHECK_FALSE(model.addExpense(missingAmount, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an empty expense string is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a whitespace-only expense string is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("   ", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a nonnumeric expense string is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(QStringLiteral("abc"), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a negative whole-dollar expense string is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("-1", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a negative fractional expense string is rejected without rounding") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("-0.000001", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("scientific notation is rejected as user expense input") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("1e2", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a currency symbol in an expense string is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("$1.00", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("grouping commas in an expense string are rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("1,000.00", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an expense string without an integer part is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(".5", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an expense string with an empty fractional part is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("5.", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a leading plus sign in an expense string is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("+1.00", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("internal whitespace in an expense string is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("1 2.50", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("multiple decimal points in an expense string are rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense("1.2.3", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("non-ASCII digits in an expense string are rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(QString::fromUtf8("１２.５０"), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a native signed integer expense is rejected by the text-only API") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(1250, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a native integer zero is rejected by the text-only API") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(0, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a native double near 12.50 is rejected before precision can be lost") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(12.499999999999, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a native float expense is rejected by the text-only API") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(12.99f, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a native long double expense is rejected by the text-only API") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(12.99L, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a NaN expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(std::numeric_limits<double>::quiet_NaN(), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an infinite expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(std::numeric_limits<double>::infinity(), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an unsigned native expense is rejected without narrowing") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(std::numeric_limits<quint64>::max(), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a boolean expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(true, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a typed cents value cannot bypass the dollar-string input contract") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(DecimalCents(1250), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a dollar string above the exact supported maximum is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    // This is 0.1 cent above the maximum, without any floating conversion.
    CHECK_FALSE(model.addExpense("92233720368547758.071", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a fractional dollar string cannot overflow cumulative spending") {
    BudgetModel model;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    REQUIRE(model.addBudget({"Food", maximum, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense("92233720368547758.06"));
    QString error;

    // One cent of capacity remains; this input is exactly 1.5 cents.
    CHECK_FALSE(model.addExpense("0.015", &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("9223372036854775806"));
    CHECK(model.remainingBudgetCents() == 1);
    CHECK(model.totalCents() == maximum);
}

TEST_CASE("a fractional dollar string cannot exceed maximum cumulative spending") {
    BudgetModel model;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    REQUIRE(model.addBudget({"Food", maximum, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense("92233720368547758.07"));
    QString error;

    CHECK_FALSE(model.addExpense("0.001", &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents() == maximum);
    CHECK(model.remainingBudgetCents() == 0);
    CHECK(model.totalCents() == maximum);
}

#ifdef __SIZEOF_INT128__
TEST_CASE("an extended unsigned native expense is rejected without narrowing") {
    BudgetModel model = modelWithExistingExpense();
    const unsigned __int128 amount = static_cast<unsigned __int128>(1) << 64;
    QString error;

    CHECK_FALSE(model.addExpense(amount, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an extended signed native expense is rejected without narrowing") {
    BudgetModel model = modelWithExistingExpense();
    const __int128 amount = static_cast<__int128>(1) << 64;
    QString error;

    CHECK_FALSE(model.addExpense(amount, &error));
    checkRejectedInputPreservesBalances(model, error);
}
#endif
