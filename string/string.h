#include <algorithm>
#include <cstring>
#include <iostream>

namespace Detail {

bool isSpace(char symbol) {
  return std::isspace(static_cast<unsigned char>(symbol)) != 0;
}

char getChar(std::istream& input) {
  return std::istream::traits_type::to_char_type(input.get());
}

void ignoreSpaces(std::istream& input) {
  while (input.good()) {
    char symbol = getChar(input);

    if (!isSpace(symbol)) {
      input.unget();
      break;
    }
  }
}

}  // namespace Detail

class String {
 public:
  String();
  String(const char* string);
  String(size_t count, char symbol);
  String(const String& string);
  ~String();

  String substr(size_t start, size_t count) const;
  size_t length() const;
  size_t size() const;
  size_t capacity() const;
  size_t find(const String& string) const;
  size_t rfind(const String& string) const;
  char* data();
  const char* data() const;
  char& front();
  const char& front() const;
  char& back();
  const char& back() const;
  bool empty() const;
  void push_back(char symbol);
  void pop_back();
  void clear();
  void shrink_to_fit();

  String& operator=(const String& string);
  String& operator+=(char symbol);
  String& operator+=(const String& string);
  char& operator[](size_t index);
  const char& operator[](size_t index) const;

 private:
  char* data_;
  size_t size_;
  size_t capacity_;

  explicit String(size_t size);

  size_t find(const String& string, size_t start) const;
  char* begin();
  const char* begin() const;
  char* end();
  const char* end() const;
  void swap(String& string);
  void reserve(size_t new_capacity);
};

bool operator==(const String& lhs, const String& rhs) {
  return lhs.size() == rhs.size() &&
         std::memcmp(lhs.data(), rhs.data(), lhs.size()) == 0;
}

bool operator!=(const String& lhs, const String& rhs) { return !(lhs == rhs); }

bool operator<(const String& lhs, const String& rhs) {
  size_t min_size = std::min(lhs.size(), rhs.size());
  int compare = std::memcmp(lhs.data(), rhs.data(), min_size);
  return compare < 0 || (compare == 0 && lhs.size() < rhs.size());
}

bool operator>(const String& lhs, const String& rhs) { return rhs < lhs; }

bool operator<=(const String& lhs, const String& rhs) { return !(lhs > rhs); }

bool operator>=(const String& lhs, const String& rhs) { return !(lhs < rhs); }

String operator+(char lhs, const String& rhs) {
  String result(rhs.size() + 1, '\0');
  result.clear();
  result.push_back(lhs);
  result += rhs;
  return result;
}

String operator+(const String& lhs, char rhs) {
  String result(lhs.size() + 1, '\0');
  result.clear();
  result += lhs;
  result.push_back(rhs);
  return result;
}

String operator+(const String& lhs, const String& rhs) {
  String result(lhs.size() + rhs.size(), '\0');
  result.clear();
  (result += lhs) += rhs;
  return result;
}

std::ostream& operator<<(std::ostream& output, const String& string) {
  for (size_t index = 0; index < string.size(); ++index) {
    output << string[index];
  }
  return output;
}

std::istream& operator>>(std::istream& input, String& string) {
  Detail::ignoreSpaces(input);

  string.clear();

  while (input.good()) {
    char symbol = Detail::getChar(input);

    if (Detail::isSpace(symbol)) {
      input.unget();
      break;
    }
    if (symbol == EOF || symbol == '\0') {
      break;
    }

    string.push_back(symbol);
  }

  return input;
}

String::String() : String(static_cast<size_t>(0)) {}

String::String(const char* string) : String(strlen(string)) {
  std::copy(string, string + size_, begin());
}

String::String(size_t count, char symbol) : String(count) {
  std::fill(begin(), end(), symbol);
}

String::String(const String& string) : String(string.size_) {
  std::copy(string.begin(), string.end(), begin());
}

String::~String() { delete[] data_; }

String String::substr(size_t start, size_t count) const {
  String substring(count);

  const char* start_pointer = begin() + start;
  const char* end_pointer = start_pointer + count;
  std::copy(start_pointer, end_pointer, substring.begin());

  return substring;
}

size_t String::length() const { return size_; }

size_t String::size() const { return size_; }

size_t String::capacity() const { return capacity_; }

size_t String::find(const String& string) const { return find(string, 0); }

size_t String::rfind(const String& string) const {
  size_t result = size_;

  for (size_t i = find(string, 0); i < size_; i = find(string, i + 1)) {
    result = i;
  }

  return result;
}

char* String::data() { return data_; }

const char* String::data() const { return data_; }

char& String::front() { return *begin(); }

const char& String::front() const { return *begin(); };

char& String::back() { return *(end() - 1); }

const char& String::back() const { return *(end() - 1); }

bool String::empty() const { return size_ == 0; }

void String::push_back(char symbol) {
  if (size_ == capacity_) {
    reserve(capacity_ == 0 ? 1 : capacity_ * 2);
  }

  ++size_;
  back() = symbol;
  *end() = '\0';
}

void String::pop_back() {
  --size_;
  *end() = '\0';
}

void String::clear() {
  size_ = 0;
  *end() = '\0';
}

void String::shrink_to_fit() { reserve(size_); }

String& String::operator=(const String& string) {
  if (this == &string) {
    return *this;
  }

  clear();
  *this += string;
  return *this;
}

String& String::operator+=(char symbol) {
  push_back(symbol);
  return *this;
}

String& String::operator+=(const String& string) {
  size_t new_size = size_ + string.size_;

  if (capacity_ < new_size) {
    reserve(new_size);
  }

  std::copy(string.begin(), string.end(), end());
  size_ = new_size;
  *end() = '\0';

  return *this;
}

char& String::operator[](size_t index) { return data_[index]; }

const char& String::operator[](size_t index) const { return data_[index]; }

String::String(size_t size)
    : data_(new char[size + 1]{}), size_(size), capacity_(size) {}

size_t String::find(const String& string, size_t start) const {
  if (string.size_ == 0 || string.size_ > size_) {
    return size_;
  }

  for (size_t i = start; i < size_ - string.size_ + 1; ++i) {
    if (std::memcmp(begin() + i, string.begin(), string.size_) == 0) {
      return i;
    }
  }

  return size_;
}

char* String::begin() { return data_; }

const char* String::begin() const { return data_; }

char* String::end() { return data_ + size_; }

const char* String::end() const { return data_ + size_; }

void String::swap(String& string) {
  std::swap(data_, string.data_);
  std::swap(size_, string.size_);
  std::swap(capacity_, string.capacity_);
}

void String::reserve(size_t new_capacity) {
  if (new_capacity == capacity_) {
    return;
  }

  String copy(new_capacity);
  copy.size_ = std::min(size_, new_capacity);
  std::copy(begin(), begin() + copy.size_, copy.begin());
  swap(copy);
}