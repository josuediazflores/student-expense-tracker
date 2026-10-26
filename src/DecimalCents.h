#pragma once

#include <QString>

#include <iosfwd>
#include <optional>
#include <type_traits>

// An exact decimal number of cents. Fractional cents have no fixed scale;
// arithmetic operates on decimal digits rather than floating-point values.
class DecimalCents {
public:
    explicit DecimalCents(qint64 cents = 0);

    // Only signed integers up to 64 bits may use the integer constructor.
    // Other input must pass an explicit, validated conversion first.
    template <typename Input,
              std::enable_if_t<!std::is_integral_v<Input> ||
                                   !std::is_signed_v<Input> ||
                                   (sizeof(Input) > sizeof(qint64)), int> = 0>
    DecimalCents(Input) = delete;

    // Accepts ASCII decimal/scientific notation without whitespace. Scientific
    // exponents are limited to +/-10000 to bound exponent-driven allocation;
    // ordinary decimal input has no fixed fractional-precision limit.
    static std::optional<DecimalCents> fromDecimal(const QString &text);

    QString toString() const;
    bool isNegative() const { return negative_; }

    DecimalCents operator+(const DecimalCents &other) const;
    DecimalCents operator-(const DecimalCents &other) const;
    bool operator<(const DecimalCents &other) const;
    bool operator==(const DecimalCents &other) const;
    bool operator!=(const DecimalCents &other) const { return !(*this == other); }
    bool operator>(const DecimalCents &other) const { return other < *this; }
    bool operator<=(const DecimalCents &other) const { return !(other < *this); }
    bool operator>=(const DecimalCents &other) const { return !(*this < other); }

    // Existing whole-cent comparisons remain exact. In particular, a floating
    // operand cannot be silently narrowed to an integer for comparison.
    template <typename Integer,
              std::enable_if_t<std::is_integral_v<Integer> &&
                                   std::is_signed_v<Integer> &&
                                   (sizeof(Integer) <= sizeof(qint64)), int> = 0>
    bool operator==(Integer cents) const {
        return *this == DecimalCents(static_cast<qint64>(cents));
    }

    template <typename Integer,
              std::enable_if_t<std::is_integral_v<Integer> &&
                                   std::is_signed_v<Integer> &&
                                   (sizeof(Integer) <= sizeof(qint64)), int> = 0>
    bool operator!=(Integer cents) const {
        return !(*this == cents);
    }

private:
    DecimalCents(QString digits, qsizetype scale, bool negative);
    void normalize();
    static int compareMagnitude(const DecimalCents &left, const DecimalCents &right);

    QString digits_;
    qsizetype scale_ = 0;
    bool negative_ = false;
};

template <typename Integer,
          std::enable_if_t<std::is_integral_v<Integer> &&
                               std::is_signed_v<Integer> &&
                               (sizeof(Integer) <= sizeof(qint64)), int> = 0>
bool operator==(Integer cents, const DecimalCents &value) {
    return value == cents;
}

template <typename Integer,
          std::enable_if_t<std::is_integral_v<Integer> &&
                               std::is_signed_v<Integer> &&
                               (sizeof(Integer) <= sizeof(qint64)), int> = 0>
bool operator!=(Integer cents, const DecimalCents &value) {
    return !(value == cents);
}

std::ostream &operator<<(std::ostream &stream, const DecimalCents &value);
