// Copyright (C) 2025, ASC team.
// All rights reserved. See files LICENSE for details.

#include <asc/core/event.h>

#include <memory>
#include <utility>

namespace asc {
namespace detail {

class EventState {
 public:
  virtual ~EventState() = default;
  virtual bool IsReady() const noexcept = 0;
  virtual Status Wait() const = 0;
  virtual BackendKind GetBackend() const noexcept = 0;
};

class CompletedEventState final : public EventState {
 public:
  explicit CompletedEventState(Status status) : status_(std::move(status)) {}

  bool IsReady() const noexcept override { return true; }
  Status Wait() const override { return status_; }
  BackendKind GetBackend() const noexcept override {
    return BackendKind::kSerial;
  }

 private:
  Status status_;
};

}  // namespace detail

Event::Event()
    : state_(std::make_shared<detail::CompletedEventState>(Status::Ok())) {}

bool Event::IsReady() const noexcept {
  return state_ == nullptr || state_->IsReady();
}

Status Event::Wait() const {
  return state_ == nullptr ? Status::Ok() : state_->Wait();
}

BackendKind Event::GetBackend() const noexcept {
  return state_ == nullptr ? BackendKind::kSerial : state_->GetBackend();
}

}  // namespace asc
