#include <asc/core/config.h>

#ifdef ASC_DEBUG
#undef ASC_DEBUG
#endif

#include <asc/core/contracts.h>

#include <string>

int main() {
  try {
    ASC_REQUIRE(false, "release-active marker");
  } catch (const asc::ContractException& exception) {
    const std::string message = exception.what();
    return message.find("release-active marker") == std::string::npos;
  }
  return 1;
}
