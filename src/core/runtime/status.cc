// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <asc/core/status.h>

#include <asc/core/contracts.h>

namespace asc {

Status::Status(StatusCode code, std::string message)
    : Status(code, std::move(message), {}, 0) {}

Status::Status(StatusCode code, std::string message, std::string provider,
               std::int64_t provider_code)
    : code_(code),
      message_(std::move(message)),
      provider_(std::move(provider)),
      provider_code_(provider_code) {
  ASC_REQUIRE(code != StatusCode::kOk,
              "Use Status::Ok() to construct a successful status");
}

Status Status::FromProvider(StatusCode code, std::string message,
                            std::string provider,
                            std::int64_t provider_code) {
  return Status(code, std::move(message), std::move(provider), provider_code);
}

namespace detail {

void InvalidResultStatus() {
  ContractFailure(ContractKind::kPrecondition, "!status.ok()",
                  "A failed Result cannot be constructed from an OK Status");
}

void BadResultAccess(const Status& status) {
  ContractFailure(ContractKind::kPrecondition, "result.ok()",
                  std::string("Attempted to access a failed Result: ") +
                      std::string(status.message()));
}

}  // namespace detail
}  // namespace asc
