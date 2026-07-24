#include <asc/utilities.h>

int UtilitiesOdrB() {
  int value = 0;
  asc::OptionParser parser;
  parser.AddOption<asc::Variable<int>>("n", "number", "number", 0, &value);
  const char* argv[] = {"odr", "--number", "-8"};
  const asc::Status status = parser.TryParse(3, argv);

  const asc::Timer timer;
  return status.ok() && value == -8 && timer.GetMeasurementCount() == 0 ? 25
                                                                        : 0;
}
