#include <asc/cpp.h>

#include <iostream>

int main() {
  asc::DMatrix<double> matrix(2, 2);
  matrix << 3.0, 1.0, 1.0, 2.0;

  asc::DVector<double> vector(2);
  vector << 1.0, 2.0;
  asc::DVector<double> result(2);
  asc::Gemv(matrix, vector, result);
  std::cout << result[0] << ' ' << result[1] << '\n';
  return 0;
}
