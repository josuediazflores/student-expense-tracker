// Author: Huy Tran
// Verify how typed dollar text is turned into whole cents for a budget amount.
#include "doctest.h"

#include "BudgetModel.h"

#include <limits>
#include <optional>

namespace {
const qint64 kMax = std::numeric_limits<qint64>::max();
const qint64 kMin = std::numeric_limits<qint64>::min();

void checkParses(const char *text, qint64 expectedCents) {
    CAPTURE(text);
    QString error = QStringLiteral("Previous error");
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(QString::fromUtf8(text), &error);
    REQUIRE(cents.has_value());
    CHECK(*cents == expectedCents);
    CHECK(error.isEmpty());
}

void checkRejects(const char *text, const char *expectedMessage) {
    CAPTURE(text);
    QString error;
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(QString::fromUtf8(text), &error);
    CHECK_FALSE(cents.has_value());
    CHECK(error == QString::fromUtf8(expectedMessage));
}

const char *const kRequired = "Budget amount is required.";
const char *const kFormat = "Enter a decimal dollar amount such as 12.99.";
const char *const kWholeCents = "Budget amount must be in whole cents.";
const char *const kTooLarge = "Budget amount is too large.";
const char *const kNegative = "Budget amount cannot be negative.";
}

TEST_CASE("whole dollars become cents") {
    checkParses("250", 25000);
}

TEST_CASE("dollars and cents become cents") {
    checkParses("12.34", 1234);
    checkParses("0.05", 5);
    checkParses("0.5", 50);
}

TEST_CASE("surrounding whitespace is trimmed") {
    checkParses(" 12.34 ", 1234);
    checkParses("\t7\n", 700);
}

TEST_CASE("trailing zeros past the cents are accepted because the value is whole cents") {
    checkParses("12.340", 1234);
    checkParses("12.3400000", 1234);
}

TEST_CASE("zero and negative zero both give zero") {
    checkParses("0", 0);
    checkParses("-0", 0);
    checkParses("0.00", 0);
}

TEST_CASE("a negative amount parses to negative cents so the model can reject it by name") {
    checkParses("-5", -500);
    checkParses("-0.01", -1);
}

TEST_CASE("an empty string is reported as required") {
    checkRejects("", kRequired);
}

TEST_CASE("a whitespace-only string is reported as required") {
    checkRejects("  ", kRequired);
    checkRejects("\t \n", kRequired);
}

TEST_CASE("letters are rejected with the format message") {
    checkRejects("abc", kFormat);
    checkRejects("12abc", kFormat);
}

TEST_CASE("scientific notation is rejected") {
    checkRejects("1e2", kFormat);
}

TEST_CASE("a currency symbol is rejected") {
    checkRejects("$5", kFormat);
}

TEST_CASE("grouping commas are rejected") {
    checkRejects("1,000", kFormat);
}

TEST_CASE("a leading plus sign is rejected") {
    checkRejects("+5", kFormat);
}

TEST_CASE("a missing integer part or empty fraction is rejected") {
    checkRejects(".5", kFormat);
    checkRejects("5.", kFormat);
}

TEST_CASE("multiple decimal points and internal whitespace are rejected") {
    checkRejects("1.2.3", kFormat);
    checkRejects("1 2", kFormat);
}

TEST_CASE("a fraction of a cent is rejected") {
    checkRejects("12.345", kWholeCents);
    checkRejects("0.001", kWholeCents);
    checkRejects("-12.345", kWholeCents);
}

TEST_CASE("the largest supported amount parses exactly") {
    checkParses("92233720368547758.07", kMax);
}

TEST_CASE("one cent above the largest supported amount is too large") {
    checkRejects("92233720368547758.08", kTooLarge);
}

TEST_CASE("a huge whole-dollar amount is too large") {
    checkRejects("99999999999999999999", kTooLarge);
}

TEST_CASE("the smallest supported amount parses exactly") {
    checkParses("-92233720368547758.08", kMin);
}

TEST_CASE("one cent below the smallest supported amount reports the negative message") {
    checkRejects("-92233720368547758.09", kNegative);
}

TEST_CASE("a huge negative amount reports the negative message") {
    checkRejects("-99999999999999999999", kNegative);
}

TEST_CASE("the whole-cents rule is checked before the size rule") {
    checkRejects("99999999999999999999.999", kWholeCents);
    checkRejects("92233720368547758.075", kWholeCents);
}

TEST_CASE("parsing works without an error out-parameter") {
    CHECK(BudgetModel::parseBudgetAmount(QStringLiteral("1.00")).value_or(-1) == 100);
    CHECK_FALSE(BudgetModel::parseBudgetAmount(QStringLiteral("abc")).has_value());
}

TEST_CASE("parsed budget text feeds the model's own budget rules") {
    BudgetModel model;
    QString error;
    const std::optional<qint64> cents = BudgetModel::parseBudgetAmount(QStringLiteral("-5"), &error);
    REQUIRE(cents.has_value());

    CHECK_FALSE(model.addBudget({"Food", *cents, QDate(2026, 10, 1), QDate(2026, 10, 31)}, &error));
    CHECK(error == QStringLiteral("Budget amount cannot be negative."));
    CHECK(model.budgets().isEmpty());
}
