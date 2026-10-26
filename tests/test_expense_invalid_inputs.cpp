// Author: Xiaochao Zhou
#include "doctest.h"

#include "BudgetModel.h"

#include <cmath>
#include <limits>

namespace {
BudgetModel modelWithExistingExpense() {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(1250.25));
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
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(1250.25));
    QString error;

    CHECK_FALSE(model.addExpense(nullptr, &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents().toString() == QStringLiteral("1250.25"));
    CHECK(model.remainingBudgetCents().toString() == QStringLiteral("8749.75"));
    CHECK(model.totalCents() == 10000);
}

TEST_CASE("a zero expense amount remains valid") {
    BudgetModel model;
    REQUIRE(model.addBudget({"Food", 10000, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(1250));
    QString error = QStringLiteral("Previous error");

    REQUIRE(model.addExpense(0, &error));
    CHECK(error.isEmpty());
    CHECK(model.totalSpendingCents() == 1250);
    CHECK(model.remainingBudgetCents() == 8750);
    CHECK(model.totalCents() == 10000);
}

TEST_CASE("a nonnumeric text expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(QStringLiteral("abc"), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a numeric-looking string expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    // Parsing text into an amount is an explicit input-layer responsibility.
    CHECK_FALSE(model.addExpense("2375.5", &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a negative integer expense amount is rejected") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(-1, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("a negative fractional expense is rejected without changing balances") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    CHECK_FALSE(model.addExpense(-0.1, &error));
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

TEST_CASE("a floating-point expense at the integer limit is rejected safely") {
    BudgetModel model = modelWithExistingExpense();
    QString error;

    // 2^63 is one cent beyond the largest supported integer-cent amount.
    CHECK_FALSE(model.addExpense(std::ldexp(1.0, 63), &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an exact fractional expense cannot overflow cumulative spending") {
    BudgetModel model;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    REQUIRE(model.addBudget({"Food", maximum, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(maximum - 1));
    QString error;

    // Only one cent remains; adding 1.5 cents would exceed the exact limit.
    CHECK_FALSE(model.addExpense(1.5, &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents() == maximum - 1);
    CHECK(model.remainingBudgetCents() == 1);
    CHECK(model.totalCents() == maximum);
}

TEST_CASE("a fraction cannot exceed the exact maximum cumulative spending") {
    BudgetModel model;
    const qint64 maximum = std::numeric_limits<qint64>::max();
    REQUIRE(model.addBudget({"Food", maximum, QDate(2026, 10, 1), QDate(2026, 10, 31)}));
    REQUIRE(model.addExpense(maximum));
    QString error;

    CHECK_FALSE(model.addExpense(0.1, &error));
    CHECK_FALSE(error.isEmpty());
    CHECK(model.totalSpendingCents() == maximum);
    CHECK(model.remainingBudgetCents() == 0);
    CHECK(model.totalCents() == maximum);
}

TEST_CASE("an oversized unsigned expense amount is rejected before conversion") {
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

#ifdef __SIZEOF_INT128__
TEST_CASE("an extended unsigned expense cannot narrow to zero") {
    BudgetModel model = modelWithExistingExpense();
    QString error;
    const unsigned __int128 amount = static_cast<unsigned __int128>(1) << 64;

    CHECK_FALSE(model.addExpense(amount, &error));
    checkRejectedInputPreservesBalances(model, error);
}

TEST_CASE("an extended signed expense cannot narrow to zero") {
    BudgetModel model = modelWithExistingExpense();
    QString error;
    const __int128 amount = static_cast<__int128>(1) << 64;

    CHECK_FALSE(model.addExpense(amount, &error));
    checkRejectedInputPreservesBalances(model, error);
}
#endif
