#include "m7_provider_odr.h"

int main() {
  return M7ProviderOdrA() == kM7ProviderTypeSize &&
                 M7ProviderOdrB() == kM7ProviderTypeSize
             ? 0
             : 1;
}
