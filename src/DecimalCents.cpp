#include "DecimalCents.h"

#include <algorithm>
#include <limits>
#include <ostream>
#include <utility>

namespace {
constexpr int maximumExponentMagnitude = 10000;

bool isAsciiDigit(QChar character) {
    return character >= QLatin1Char('0') && character <= QLatin1Char('9');
}

QString addDigits(const QString &left, const QString &right) {
    const qsizetype width = std::max(left.size(), right.size());
    QString result(width + 1, QLatin1Char('0'));
    qsizetype leftIndex = left.size();
    qsizetype rightIndex = right.size();
    int carry = 0;
    for (qsizetype index = width; index > 0; --index) {
        const int leftDigit = leftIndex > 0 ? left.at(--leftIndex).unicode() - '0' : 0;
        const int rightDigit = rightIndex > 0 ? right.at(--rightIndex).unicode() - '0' : 0;
        const int sum = leftDigit + rightDigit + carry;
        result[index] = QChar(static_cast<ushort>('0' + sum % 10));
        carry = sum / 10;
    }
    result[0] = QChar(static_cast<ushort>('0' + carry));
    return result;
}

// The caller guarantees that left's nonnegative value is at least right's.
QString subtractDigits(const QString &left, const QString &right) {
    QString result(left.size(), QLatin1Char('0'));
    qsizetype rightIndex = right.size();
    int borrow = 0;
    for (qsizetype index = left.size(); index > 0;) {
        --index;
        const int rightDigit = rightIndex > 0 ? right.at(--rightIndex).unicode() - '0' : 0;
        int difference = left.at(index).unicode() - '0' - rightDigit - borrow;
        borrow = difference < 0 ? 1 : 0;
        if (borrow) difference += 10;
        result[index] = QChar(static_cast<ushort>('0' + difference));
    }
    return result;
}
}

DecimalCents::DecimalCents(qint64 cents) : digits_(QString::number(cents)) {
    // Formatting the signed value also handles qint64's minimum without abs().
    if (digits_.startsWith(QLatin1Char('-'))) {
        negative_ = true;
        digits_.remove(0, 1);
    }
}

DecimalCents::DecimalCents(QString digits, qsizetype scale, bool negative)
    : digits_(std::move(digits)), scale_(scale), negative_(negative) {
    normalize();
}

std::optional<DecimalCents> DecimalCents::fromDecimal(const QString &text) {
    if (text.isEmpty()) return std::nullopt;

    qsizetype index = 0;
    bool negative = false;
    if (text.at(index) == QLatin1Char('-') || text.at(index) == QLatin1Char('+')) {
        negative = text.at(index) == QLatin1Char('-');
        if (++index == text.size()) return std::nullopt;
    }

    QString digits;
    digits.reserve(text.size());
    bool sawDigit = false;
    while (index < text.size() && isAsciiDigit(text.at(index))) {
        digits.append(text.at(index++));
        sawDigit = true;
    }

    qsizetype scale = 0;
    if (index < text.size() && text.at(index) == QLatin1Char('.')) {
        ++index;
        while (index < text.size() && isAsciiDigit(text.at(index))) {
            digits.append(text.at(index++));
            ++scale;
            sawDigit = true;
        }
    }
    if (!sawDigit) return std::nullopt;

    int exponent = 0;
    if (index < text.size() &&
        (text.at(index) == QLatin1Char('e') || text.at(index) == QLatin1Char('E'))) {
        ++index;
        bool exponentNegative = false;
        if (index < text.size() &&
            (text.at(index) == QLatin1Char('-') || text.at(index) == QLatin1Char('+'))) {
            exponentNegative = text.at(index++) == QLatin1Char('-');
        }
        const qsizetype exponentStart = index;
        while (index < text.size() && isAsciiDigit(text.at(index))) {
            const int digit = text.at(index++).unicode() - '0';
            if (exponent > (maximumExponentMagnitude - digit) / 10) return std::nullopt;
            exponent = exponent * 10 + digit;
        }
        if (index == exponentStart) return std::nullopt;
        if (exponentNegative) exponent = -exponent;
    }
    if (index != text.size()) return std::nullopt;

    if (exponent >= 0) {
        if (scale < exponent) {
            digits.append(QString(exponent - scale, QLatin1Char('0')));
            scale = 0;
        } else {
            scale -= exponent;
        }
    } else {
        if (scale > std::numeric_limits<qsizetype>::max() + exponent) return std::nullopt;
        scale -= exponent;
    }
    return DecimalCents(std::move(digits), scale, negative);
}

void DecimalCents::normalize() {
    // Remove insignificant zeros only; never round or discard fractional value.
    qsizetype firstNonzero = 0;
    while (firstNonzero < digits_.size() && digits_.at(firstNonzero) == QLatin1Char('0')) {
        ++firstNonzero;
    }
    if (firstNonzero == digits_.size()) {
        digits_ = QStringLiteral("0");
        scale_ = 0;
        negative_ = false;
        return;
    }
    if (firstNonzero > 0) digits_.remove(0, firstNonzero);
    while (scale_ > 0 && digits_.endsWith(QLatin1Char('0'))) {
        digits_.chop(1);
        --scale_;
    }
}

QString DecimalCents::toString() const {
    QString result;
    if (scale_ == 0) {
        result = digits_;
    } else if (scale_ >= digits_.size()) {
        result = QStringLiteral("0.") + QString(scale_ - digits_.size(), QLatin1Char('0')) + digits_;
    } else {
        result = digits_;
        result.insert(result.size() - scale_, QLatin1Char('.'));
    }
    if (negative_) result.prepend(QLatin1Char('-'));
    return result;
}

int DecimalCents::compareMagnitude(const DecimalCents &left, const DecimalCents &right) {
    const bool leftZero = left.digits_ == QStringLiteral("0");
    const bool rightZero = right.digits_ == QStringLiteral("0");
    if (leftZero || rightZero) return leftZero == rightZero ? 0 : (leftZero ? -1 : 1);

    const qsizetype leftOrder = left.digits_.size() - left.scale_;
    const qsizetype rightOrder = right.digits_.size() - right.scale_;
    if (leftOrder != rightOrder) return leftOrder < rightOrder ? -1 : 1;

    const qsizetype width = std::max(left.digits_.size(), right.digits_.size());
    for (qsizetype index = 0; index < width; ++index) {
        const QChar leftDigit = index < left.digits_.size() ? left.digits_.at(index) : QLatin1Char('0');
        const QChar rightDigit = index < right.digits_.size() ? right.digits_.at(index) : QLatin1Char('0');
        if (leftDigit != rightDigit) return leftDigit < rightDigit ? -1 : 1;
    }
    return 0;
}

DecimalCents DecimalCents::operator+(const DecimalCents &other) const {
    // Align decimal places, then add or subtract the integer digits exactly.
    const qsizetype scale = std::max(scale_, other.scale_);
    const QString leftDigits = digits_ + QString(scale - scale_, QLatin1Char('0'));
    const QString rightDigits = other.digits_ + QString(scale - other.scale_, QLatin1Char('0'));
    if (negative_ == other.negative_) {
        return DecimalCents(addDigits(leftDigits, rightDigits), scale, negative_);
    }
    const int comparison = compareMagnitude(*this, other);
    if (comparison == 0) return DecimalCents();
    if (comparison > 0) {
        return DecimalCents(subtractDigits(leftDigits, rightDigits), scale, negative_);
    }
    return DecimalCents(subtractDigits(rightDigits, leftDigits), scale, other.negative_);
}

DecimalCents DecimalCents::operator-(const DecimalCents &other) const {
    DecimalCents negated = other;
    if (negated.digits_ != QStringLiteral("0")) negated.negative_ = !negated.negative_;
    return *this + negated;
}

bool DecimalCents::operator<(const DecimalCents &other) const {
    if (negative_ != other.negative_) return negative_;
    const int comparison = compareMagnitude(*this, other);
    return negative_ ? comparison > 0 : comparison < 0;
}

bool DecimalCents::operator==(const DecimalCents &other) const {
    return negative_ == other.negative_ && scale_ == other.scale_ && digits_ == other.digits_;
}

std::ostream &operator<<(std::ostream &stream, const DecimalCents &value) {
    return stream << value.toString().toStdString();
}
