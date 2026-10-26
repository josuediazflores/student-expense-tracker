// Author: Xiaochao Zhou
#include "doctest.h"

#include "DecimalCents.h"

#include <limits>
#include <type_traits>
#include <utility>

namespace {
DecimalCents decimal(const char *text) {
    const auto value = DecimalCents::fromDecimal(QString::fromLatin1(text));
    REQUIRE(value.has_value());
    return *value;
}

template <typename Value, typename = void>
struct CanCompareToDecimal : std::false_type {};

template <typename Value>
struct CanCompareToDecimal<Value, std::void_t<decltype(
    std::declval<const DecimalCents &>() == std::declval<Value>())>> : std::true_type {};

static_assert(!std::is_convertible_v<qint64, DecimalCents>);
static_assert(!std::is_constructible_v<DecimalCents, double>);
static_assert(!std::is_constructible_v<DecimalCents, quint64>);
static_assert(!std::is_constructible_v<DecimalCents, bool>);
static_assert(std::is_constructible_v<DecimalCents, qint64>);
static_assert(std::is_copy_constructible_v<DecimalCents>);
#ifdef __SIZEOF_INT128__
static_assert(!std::is_constructible_v<DecimalCents, __int128>);
static_assert(!std::is_constructible_v<DecimalCents, unsigned __int128>);
#endif
static_assert(CanCompareToDecimal<qint64>::value);
static_assert(!CanCompareToDecimal<double>::value);
static_assert(!CanCompareToDecimal<quint64>::value);
}

TEST_CASE("decimal cents retains exact signed 64-bit integer boundaries") {
    const qint64 minimum = std::numeric_limits<qint64>::min();
    const qint64 maximum = std::numeric_limits<qint64>::max();

    CHECK(DecimalCents(minimum).toString() == QStringLiteral("-9223372036854775808"));
    CHECK(DecimalCents(maximum).toString() == QStringLiteral("9223372036854775807"));
    CHECK(DecimalCents(minimum) == minimum);
    CHECK(maximum == DecimalCents(maximum));
    CHECK((DecimalCents(minimum) - DecimalCents(1)).toString() ==
          QStringLiteral("-9223372036854775809"));
    CHECK((DecimalCents(maximum) + DecimalCents(1)).toString() ==
          QStringLiteral("9223372036854775808"));
}

TEST_CASE("fractional decimal additions preserve their combined amount") {
    CHECK((decimal("1250.5") + decimal("2375.5")).toString() == QStringLiteral("3626"));
    CHECK((decimal("0.1") + decimal("0.2")).toString() == QStringLiteral("0.3"));
    CHECK((decimal("12.5") + decimal("0.00025")).toString() == QStringLiteral("12.50025"));
}

TEST_CASE("decimal addition carries across the decimal point") {
    CHECK((decimal("999.999") + decimal("0.001")).toString() == QStringLiteral("1000"));
    CHECK((decimal("0.75") + decimal("0.25")).toString() == QStringLiteral("1"));
}

TEST_CASE("decimal subtraction borrows across the decimal point") {
    CHECK((decimal("1000") - decimal("0.001")).toString() == QStringLiteral("999.999"));
    CHECK((decimal("10000") - decimal("1250.5")).toString() == QStringLiteral("8749.5"));
    CHECK((decimal("0.001") - decimal("1000")).toString() == QStringLiteral("-999.999"));
}

TEST_CASE("decimal addition preserves the signs of positive and negative values") {
    CHECK((decimal("12.5") + decimal("-2.75")).toString() == QStringLiteral("9.75"));
    CHECK((decimal("-12.5") + decimal("2.75")).toString() == QStringLiteral("-9.75"));
    CHECK((decimal("-12.5") + decimal("-2.75")).toString() == QStringLiteral("-15.25"));
}

TEST_CASE("decimal subtraction handles negative operands") {
    CHECK((decimal("-2.75") - decimal("-12.5")).toString() == QStringLiteral("9.75"));
    CHECK((decimal("-12.5") - decimal("2.75")).toString() == QStringLiteral("-15.25"));
    CHECK((DecimalCents() - decimal("0.001")).toString() == QStringLiteral("-0.001"));
}

TEST_CASE("zero is canonical after parsing and exact cancellation") {
    const DecimalCents zero = decimal("-0.000e-5000");
    CHECK(zero.toString() == QStringLiteral("0"));
    CHECK_FALSE(zero.isNegative());
    CHECK(zero == 0);
    CHECK((zero + decimal("0.001")) == decimal("0.001"));
    CHECK((zero + decimal("-0.001")) == decimal("-0.001"));
    CHECK((decimal("-12.5") + decimal("12.5")) == 0);
    CHECK((decimal("12.5") - decimal("12.5")) == 0);
}

TEST_CASE("ordinary and scientific decimal notation normalize to the same value") {
    CHECK(decimal("-00012.34000").toString() == QStringLiteral("-12.34"));
    CHECK(decimal("1.234e3").toString() == QStringLiteral("1234"));
    CHECK(decimal("1.234E-3").toString() == QStringLiteral("0.001234"));
    CHECK(decimal("+.125").toString() == QStringLiteral("0.125"));
    CHECK(decimal("1.").toString() == QStringLiteral("1"));
    CHECK(decimal("1.200e0") == decimal("1.2"));

    const auto tiny = DecimalCents::fromDecimal(QStringLiteral("1e-5000"));
    REQUIRE(tiny.has_value());
    CHECK(tiny->toString() == QStringLiteral("0.") + QString(4999, QLatin1Char('0')) +
                                 QStringLiteral("1"));
}

TEST_CASE("decimal comparison aligns scale and accounts for negative signs") {
    CHECK(decimal("0.0001") < decimal("0.01"));
    CHECK(decimal("0.1") < decimal("1"));
    CHECK(decimal("1.2001") > decimal("1.2"));
    CHECK(decimal("-100.01") < decimal("-100.001"));
    CHECK(decimal("-0.001") < DecimalCents());
    CHECK(DecimalCents() < decimal("1e-100"));
    CHECK(decimal("9223372036854775808") >
          DecimalCents(std::numeric_limits<qint64>::max()));
}

TEST_CASE("ordinary decimal input has no fixed fractional precision cap") {
    const QString text = QStringLiteral("0.") + QString(10001, QLatin1Char('0')) +
                         QStringLiteral("1");
    const auto value = DecimalCents::fromDecimal(text);
    REQUIRE(value.has_value());
    CHECK(value->toString() == text);
    CHECK((*value + *value).toString() ==
          QStringLiteral("0.") + QString(10001, QLatin1Char('0')) + QStringLiteral("2"));
}

TEST_CASE("invalid decimal text is rejected without partial parsing") {
    const char *invalidInputs[] = {
        "", "+", "-", ".", "+.", "1e", "1e+", "1e-", "e1", "1.2.3",
        "1e2e3", "1x", " 1", "1 ", "1,25", "nan", "inf", "--1", "1e10001", "1e-10001"
    };
    for (const char *text : invalidInputs) {
        CHECK_MESSAGE(!DecimalCents::fromDecimal(QString::fromLatin1(text)).has_value(), text);
    }
    CHECK_FALSE(DecimalCents::fromDecimal(QString::fromUtf8("１２.３")).has_value());
    CHECK_FALSE(DecimalCents::fromDecimal(QString::fromUtf8("−1")).has_value());
}
