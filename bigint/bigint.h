#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace Detail {

constexpr int shift = 48;

size_t getDigitCount(int64_t number) {
  size_t result = 0;

  do {
    ++result;
    number /= 10;
  } while (number != 0);

  return result;
}

char getChar(int digit) {
  return std::istream::traits_type::to_char_type(digit + shift);
}

int getInt(char digit) { return digit - shift; }

}  // namespace Detail

class BigInteger {
 public:
  BigInteger();
  BigInteger(int number);
  BigInteger(unsigned long number);
  explicit BigInteger(unsigned long long number);
  explicit BigInteger(const char* number, size_t size);

  std::string toString() const;

  BigInteger& operator+=(const BigInteger& number);
  BigInteger& operator-=(const BigInteger& number);
  BigInteger& operator*=(const BigInteger& number);
  BigInteger& operator/=(const BigInteger& number);
  BigInteger& operator%=(const BigInteger& number);
  BigInteger& operator++();
  BigInteger& operator--();
  BigInteger operator++(int);
  BigInteger operator--(int);
  BigInteger operator-() const;

  explicit operator bool() const;

  friend bool operator==(const BigInteger& lhs, const BigInteger& rhs);
  friend bool operator<(const BigInteger& lhs, const BigInteger& rhs);

 private:
  enum Sign : bool { NotNegative, Negative };

  Sign sign_;
  std::vector<int64_t> digits_;

  bool isZero() const;
  void insertDigit(size_t index, int64_t value);
  void removePrefixZeros();
  void changeSign();
  void updateSign();
  void buildFromString(const char* string, size_t size);

  static const size_t cDegree;
  static const int64_t cBase;

  static bool compareByModule(const BigInteger& lhs, const BigInteger& rhs);
  static Sign getSign(int64_t number);
};

const size_t BigInteger::cDegree = 9;
const int64_t BigInteger::cBase = std::pow(10, BigInteger::cDegree);

bool operator==(const BigInteger& lhs, const BigInteger& rhs) {
  return lhs.sign_ == rhs.sign_ && lhs.digits_.size() == rhs.digits_.size() &&
         std::memcmp(lhs.digits_.data(), rhs.digits_.data(),
                     lhs.digits_.size() * 8) == 0;
}

bool operator!=(const BigInteger& lhs, const BigInteger& rhs) {
  return !(lhs == rhs);
}

bool operator<(const BigInteger& lhs, const BigInteger& rhs) {
  if (lhs.sign_ != rhs.sign_) {
    return lhs.sign_ == BigInteger::Sign::Negative;
  }
  if (lhs.sign_ == BigInteger::Sign::Negative) {
    return BigInteger::compareByModule(rhs, lhs);
  }
  return BigInteger::compareByModule(lhs, rhs);
}

bool operator>(const BigInteger& lhs, const BigInteger& rhs) {
  return rhs < lhs;
}

bool operator<=(const BigInteger& lhs, const BigInteger& rhs) {
  return !(lhs > rhs);
}

bool operator>=(const BigInteger& lhs, const BigInteger& rhs) {
  return !(lhs < rhs);
}

BigInteger operator+(const BigInteger& lhs, const BigInteger& rhs) {
  BigInteger result(lhs);
  result += rhs;
  return result;
}

BigInteger operator-(const BigInteger& lhs, const BigInteger& rhs) {
  BigInteger result(lhs);
  result -= rhs;
  return result;
}

BigInteger operator*(const BigInteger& lhs, const BigInteger& rhs) {
  BigInteger result(lhs);
  result *= rhs;
  return result;
}

BigInteger operator/(const BigInteger& lhs, const BigInteger& rhs) {
  BigInteger result(lhs);
  result /= rhs;
  return result;
}

BigInteger operator%(const BigInteger& lhs, const BigInteger& rhs) {
  BigInteger result(lhs);
  result %= rhs;
  return result;
}

BigInteger operator""_bi(const char* number, size_t size) {
  BigInteger result(number, size);
  return result;
}

BigInteger operator""_bi(unsigned long long number) {
  BigInteger result(number);
  return result;
}

std::ostream& operator<<(std::ostream& out, const BigInteger& number) {
  return out << number.toString();
}

std::istream& operator>>(std::istream& in, BigInteger& number) {
  std::string string;
  in >> string;

  BigInteger new_number(string.c_str(), string.size());
  std::swap(number, new_number);

  return in;
}

namespace Math {

BigInteger abs(const BigInteger& number) {
  BigInteger copy = number;

  if (copy < 0) {
    copy *= -1;
  }

  return copy;
}

BigInteger getGCD(const BigInteger& lhs, const BigInteger& rhs) {
  BigInteger first = abs(lhs);
  BigInteger second = abs(rhs);

  while (first != 0 && second != 0) {
    if (first > second) {
      first %= second;
    } else {
      second %= first;
    }
  }

  BigInteger result = first + second;

  return result;
}

}  // namespace Math

BigInteger::BigInteger() : BigInteger(0ULL) {}

BigInteger::BigInteger(int number)
    : BigInteger(static_cast<unsigned long long>(
          std::abs(static_cast<int64_t>(number)))) {
  sign_ = getSign(number);
}

BigInteger::BigInteger(unsigned long number)
    : BigInteger(static_cast<unsigned long long>(number)) {}

std::string BigInteger::toString() const {
  int64_t first = digits_.back();
  size_t length_of_first = Detail::getDigitCount(first);

  size_t length = (digits_.size() - 1) * cDegree + length_of_first;

  if (sign_ == Sign::Negative) {
    ++length;
  }

  std::string result(length, '0');

  size_t pointer = 0;

  if (sign_ == Sign::Negative) {
    result[pointer] = '-';
    ++pointer;
  }

  pointer += length_of_first;

  for (size_t i = 0; i < length_of_first; ++i) {
    result[pointer - i - 1] = Detail::getChar(first % 10);
    first /= 10;
  }

  for (size_t i = 1; i < digits_.size(); ++i) {
    pointer += cDegree;

    int64_t digit = digits_[digits_.size() - i - 1];
    size_t delta = 0;

    do {
      result[pointer - delta - 1] = Detail::getChar(digit % 10);
      digit /= 10;
      ++delta;
    } while (digit != 0);
  }

  return result;
}

BigInteger& BigInteger::operator+=(const BigInteger& number) {
  const BigInteger& minimum = std::min(*this, number, compareByModule);
  const BigInteger& maximum = std::max(*this, number, compareByModule);

  std::vector<int64_t> result(maximum.digits_.size() + 1);

  for (size_t i = 0; i < maximum.digits_.size(); ++i) {
    int64_t second = i < minimum.digits_.size() ? minimum.digits_[i] : 0;

    if (sign_ != number.sign_) {
      second *= -1;
    }

    result[i] += maximum.digits_[i] + second;

    if (result[i] >= cBase) {
      ++result[i + 1];
      result[i] -= cBase;
    } else if (result[i] < 0) {
      --result[i + 1];
      result[i] += cBase;
    }
  }

  sign_ = maximum.sign_;
  digits_.swap(result);
  removePrefixZeros();
  updateSign();
  return *this;
}

BigInteger& BigInteger::operator-=(const BigInteger& number) {
  return *this += -number;
}

BigInteger& BigInteger::operator*=(const BigInteger& number) {
  if (*this == 0) {
    return *this;
  }

  if (number == 0) {
    *this = 0;
    return *this;
  }

  if (number.sign_ == Sign::Negative) {
    changeSign();
  }

  if (number == 1 || number == -1) {
    return *this;
  }

  std::vector<int64_t> result(digits_.size() + number.digits_.size());

  for (size_t i = 0; i < number.digits_.size(); ++i) {
    for (size_t j = 0; j < digits_.size(); ++j) {
      result[i + j] += number.digits_[i] * digits_[j];

      if (result[i + j] >= cBase) {
        result[i + j + 1] += result[i + j] / cBase;
        result[i + j] %= cBase;
      }
    }
  }

  digits_.swap(result);

  removePrefixZeros();

  return *this;
}

BigInteger& BigInteger::operator/=(const BigInteger& number) {
  if (*this == 0) {
    return *this;
  }

  if (number.sign_ == Sign::Negative) {
    changeSign();
  }

  if (number == 1 || number == -1) {
    return *this;
  }

  std::vector<int64_t> result;

  BigInteger remainder;
  BigInteger abs_number = Math::abs(number);

  for (int i = digits_.size() - 1; i >= 0; --i) {
    remainder.insertDigit(0, digits_[i]);

    int digit = 0;

    int left_border = 0;
    int right_border = cBase;

    while (left_border != right_border) {
      int middle = (left_border + right_border) / 2;

      if (remainder >= abs_number * middle) {
        digit = middle;
        left_border = middle + 1;
      } else {
        right_border = middle;
      }
    }

    remainder -= abs_number * digit;
    result.push_back(digit);
  }

  std::reverse(result.begin(), result.end());
  digits_.swap(result);
  removePrefixZeros();
  updateSign();

  return *this;
}

BigInteger& BigInteger::operator%=(const BigInteger& number) {
  *this -= *this / number * number;
  return *this;
}

BigInteger& BigInteger::operator++() { return *this += 1; }

BigInteger& BigInteger::operator--() { return *this += -1; }

BigInteger BigInteger::operator++(int) {
  BigInteger copy(*this);
  ++*this;
  return copy;
}

BigInteger BigInteger::operator--(int) {
  BigInteger copy(*this);
  --*this;
  return copy;
}

BigInteger BigInteger::operator-() const {
  BigInteger copy(*this);
  copy.changeSign();
  return copy;
}

BigInteger::operator bool() const { return *this != 0; }

BigInteger::BigInteger(unsigned long long number) : sign_(Sign::NotNegative) {
  do {
    digits_.push_back(number % cBase);
    number /= cBase;
  } while (number != 0);
}

BigInteger::BigInteger(const char* number, size_t size) {
  buildFromString(number, size);
}

bool BigInteger::isZero() const {
  return digits_.size() == 1 && digits_.front() == 0;
}

void BigInteger::insertDigit(size_t index, int64_t value) {
  digits_.insert(digits_.begin() + index, value);
  removePrefixZeros();
}

void BigInteger::removePrefixZeros() {
  while (digits_.size() > 1 && digits_.back() == 0) {
    digits_.pop_back();
  }
}

void BigInteger::changeSign() {
  if (!isZero()) {
    sign_ = Sign(!sign_);
  }
}

void BigInteger::updateSign() {
  if (isZero()) {
    sign_ = Sign::NotNegative;
  }
}

void BigInteger::buildFromString(const char* string, size_t length) {
  size_t begin = 0;
  size_t end = length - 1;

  if (string[begin] == '-') {
    sign_ = Sign::Negative;
    ++begin;
  } else {
    sign_ = Sign::NotNegative;
  }

  while (string[begin] == '0' && begin < end) {
    ++begin;
  }

  length = end - begin + 1;

  size_t size = length / cDegree;

  if (length % cDegree != 0) {
    ++size;
  }

  digits_.resize(size);

  end += cDegree;

  for (size_t i = 0; i < size; ++i) {
    end -= cDegree;

    digits_[i] = 0;

    size_t count_to_read = std::min(cDegree, end - begin + 1);
    int64_t digit = 1;

    for (size_t j = 0; j < count_to_read; ++j) {
      digits_[i] += Detail::getInt(string[end - j]) * digit;
      digit *= 10;
    }
  }

  updateSign();
}

bool BigInteger::compareByModule(const BigInteger& lhs, const BigInteger& rhs) {
  if (lhs.digits_.size() != rhs.digits_.size()) {
    return lhs.digits_.size() < rhs.digits_.size();
  }

  return std::lexicographical_compare(lhs.digits_.rbegin(), lhs.digits_.rend(),
                                      rhs.digits_.rbegin(), rhs.digits_.rend());
}

BigInteger::Sign BigInteger::getSign(int64_t number) {
  return Sign(std::signbit(number));
}

class Rational {
 public:
  Rational();
  Rational(int number);
  Rational(const BigInteger& number);

  std::string toString() const;
  std::string asDecimal(size_t precision) const;

  Rational& operator+=(const Rational& another);
  Rational& operator-=(const Rational& another);
  Rational& operator*=(const Rational& another);
  Rational& operator/=(const Rational& another);
  Rational operator-() const;

  explicit operator double() const;

  friend bool operator==(const Rational& lhs, const Rational& rhs);
  friend bool operator<(const Rational& lhs, const Rational& rhs);

 private:
  BigInteger numerator_;
  BigInteger denominator_;

  bool isInteger() const;
  void shrink();
};

Rational operator+(const Rational& lhs, const Rational& rhs) {
  Rational result(lhs);
  result += rhs;
  return result;
}

Rational operator-(const Rational& lhs, const Rational& rhs) {
  Rational result(lhs);
  result -= rhs;
  return result;
}

Rational operator*(const Rational& lhs, const Rational& rhs) {
  Rational result(lhs);
  result *= rhs;
  return result;
}

Rational operator/(const Rational& lhs, const Rational& rhs) {
  Rational result(lhs);
  result /= rhs;
  return result;
}

bool operator==(const Rational& lhs, const Rational& rhs) {
  return lhs.numerator_ == rhs.numerator_ &&
         lhs.denominator_ == rhs.denominator_;
}

bool operator!=(const Rational& lhs, const Rational& rhs) {
  return !(lhs == rhs);
}

bool operator<(const Rational& lhs, const Rational& rhs) {
  if (lhs.denominator_ == rhs.denominator_) {
    return lhs.numerator_ < rhs.numerator_;
  }
  return lhs.numerator_ * rhs.denominator_ < rhs.numerator_ * lhs.denominator_;
}

bool operator>(const Rational& lhs, const Rational& rhs) { return rhs < lhs; }

bool operator<=(const Rational& lhs, const Rational& rhs) {
  return !(lhs > rhs);
}

bool operator>=(const Rational& lhs, const Rational& rhs) {
  return !(lhs < rhs);
}

Rational::Rational() : Rational(0) {}

Rational::Rational(int number) : Rational(BigInteger(number)) {}

Rational::Rational(const BigInteger& number)
    : numerator_(number), denominator_(1) {}

std::string Rational::toString() const {
  std::string result(numerator_.toString());

  if (!isInteger()) {
    (result += '/') += denominator_.toString();
  }

  return result;
}

std::string Rational::asDecimal(size_t precision = 0) const {
  std::string result((numerator_ / denominator_).toString());

  bool is_zero = result == "0";

  BigInteger abs_numerator = Math::abs(numerator_);
  BigInteger remaider = abs_numerator % denominator_;

  if (precision != 0) {
    result += '.';
  }

  for (size_t counter = 0; counter < precision; ++counter) {
    remaider *= 10;

    int digit = 0;

    while (remaider >= denominator_) {
      remaider -= denominator_;
      ++digit;
    }

    result += Detail::getChar(digit);
    is_zero = is_zero && digit == 0;
  }

  if (!is_zero && numerator_ < 0 && result.front() != '-') {
    result = '-' + result;
  }

  return result;
}

Rational& Rational::operator+=(const Rational& another) {
  if (denominator_ == another.denominator_) {
    numerator_ += another.numerator_;
  } else {
    numerator_ *= another.denominator_;
    numerator_ += another.numerator_ * denominator_;
    denominator_ *= another.denominator_;
  }

  shrink();

  return *this;
}

Rational& Rational::operator-=(const Rational& another) {
  return *this += -another;
}

Rational& Rational::operator*=(const Rational& another) {
  numerator_ *= another.numerator_;
  denominator_ *= another.denominator_;

  shrink();

  return *this;
}

Rational& Rational::operator/=(const Rational& another) {
  numerator_ *= another.denominator_;
  denominator_ *= another.numerator_;

  if (denominator_ < 0) {
    numerator_ *= -1;
    denominator_ *= -1;
  }

  shrink();

  return *this;
}

Rational Rational::operator-() const {
  Rational copy(*this);
  copy.numerator_ *= -1;
  return copy;
}

Rational::operator double() const {
  const size_t cPrecision = 15;
  return std::atof(asDecimal(cPrecision).c_str());
}

bool Rational::isInteger() const { return denominator_ == 1; }

void Rational::shrink() {
  BigInteger gcd = Math::getGCD(numerator_, denominator_);
  numerator_ /= gcd;
  denominator_ /= gcd;
}