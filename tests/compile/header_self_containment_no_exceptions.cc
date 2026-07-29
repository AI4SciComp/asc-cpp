#ifndef ASC_TEST_HEADER
#error "ASC_TEST_HEADER must name one public header."
#endif

#include ASC_TEST_HEADER

#if defined(__cpp_exceptions) || defined(__EXCEPTIONS) || defined(_CPPUNWIND)
#error "This public-header compile contract must disable C++ exceptions."
#endif

int main() { return 0; }
