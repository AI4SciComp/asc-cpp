#ifndef ASC_TESTS_LOCALE_TEST_SUPPORT_H_
#define ASC_TESTS_LOCALE_TEST_SUPPORT_H_

#include <locale>
#include <sstream>
#include <string>

namespace asc_locale_test {

// An unnamed custom C++ locale avoids dependence on optional OS locale packs.
// The positive ostream control proves the active decimal/grouping behavior;
// this does not claim a named C setlocale locale was installed.
class CommaPunctuation final : public std::numpunct<char> {
 protected:
  [[nodiscard]] char do_decimal_point() const override { return ','; }
  [[nodiscard]] char do_thousands_sep() const override { return '.'; }
  [[nodiscard]] std::string do_grouping() const override { return "\3"; }
};
class LocaleGuard {
 public:
  LocaleGuard()
      : original_(std::locale::global(
            std::locale(std::locale::classic(), new CommaPunctuation))) {}
  ~LocaleGuard() { std::locale::global(original_); }
  LocaleGuard(const LocaleGuard&) = delete;
  LocaleGuard& operator=(const LocaleGuard&) = delete;
  LocaleGuard(LocaleGuard&&) = delete;
  LocaleGuard& operator=(LocaleGuard&&) = delete;

 private:
  std::locale original_;
};
inline bool LocaleControl() {
  std::ostringstream stream;
  stream << 1234.5;
  return stream.str() == "1.234,5";
}

}  // namespace asc_locale_test

#endif  // ASC_TESTS_LOCALE_TEST_SUPPORT_H_
