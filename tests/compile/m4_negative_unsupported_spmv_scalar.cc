#include "asc/core/execution.h"
#include "asc/sparse/blas.h"
#include "m4_multi_tu.h"

void AttemptIntegerSpmv(const asc::ExecutionContext& context,
                        asc::CsrView<const int> matrix,
                        const M4CompileVector<const int>& input,
                        M4CompileVector<int>& output) {
  (void)asc::Spmv(context, 1, matrix, input, 0, output);
}

int main() { return 0; }
