int UtilitiesOdrA();
int UtilitiesOdrB();

int main() {
  return UtilitiesOdrA() + UtilitiesOdrB() == 42 ? 0 : 1;
}
