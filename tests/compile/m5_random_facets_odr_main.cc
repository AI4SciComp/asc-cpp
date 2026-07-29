bool CheckM5OdrDense();
bool CheckM5OdrSparse();

int main() { return CheckM5OdrDense() && CheckM5OdrSparse() ? 0 : 1; }
