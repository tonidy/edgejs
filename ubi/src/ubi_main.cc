#include <iostream>
#include <string>

#include "ubi_cli.h"

int main(int argc, char** argv) {
  UbiInitializeCliProcess();
  std::string error;
  const int exit_code = UbiRunCli(argc, argv, &error);
  const bool is_process_exit_marker =
      error.rfind("process.exit(", 0) == 0 && !error.empty() && error.back() == ')';
  if (!error.empty() && !is_process_exit_marker) {
    std::cerr << error << "\n";
  }
  return exit_code;
}
