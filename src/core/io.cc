#include "asc/core/io.h"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "asc/core/contracts.h"

namespace asc {

namespace internal_core_io {

std::FILE* ToFile(void* handle) noexcept {
  return static_cast<std::FILE*>(handle);
}

Result<std::FILE*> OpenNativeFile(const std::filesystem::path& path,
                                  bool write) {
  errno = 0;
#if defined(_WIN32)
  std::FILE* file = nullptr;
  const errno_t error = _wfopen_s(&file, path.c_str(), write ? L"wb" : L"rb");
  if (error != 0 || file == nullptr) {
    return Status(ErrorCode::kIo, "Unable to open local file", "system",
                  static_cast<std::int64_t>(error));
  }
#else
  std::FILE* file = std::fopen(path.c_str(), write ? "wb" : "rb");
  if (file == nullptr) {
    return Status(ErrorCode::kIo, "Unable to open local file", "system",
                  static_cast<std::int64_t>(errno));
  }
#endif
  return file;
}

Status FileError(std::string message) {
  return Status(ErrorCode::kIo, std::move(message), "system",
                static_cast<std::int64_t>(errno));
}

}  // namespace internal_core_io

ByteSource::~ByteSource() = default;
ByteSink::~ByteSink() = default;

Status ReadExact(ByteSource& source, std::span<std::byte> destination) {
  std::size_t completed = 0;
  while (completed < destination.size()) {
    auto count = source.ReadSome(destination.subspan(completed));
    if (!count.ok()) {
      return count.status();
    }
    if (*count == 0) {
      return Status(ErrorCode::kEndOfFile,
                    completed == 0 ? "EOF before exact read began"
                                   : "Short input before exact read completed");
    }
    if (*count > destination.size() - completed) {
      return Status(ErrorCode::kInternal,
                    "ByteSource returned more bytes than requested");
    }
    completed += *count;
  }
  return Status::Ok();
}

Status WriteAll(ByteSink& sink, std::span<const std::byte> source) {
  std::size_t completed = 0;
  while (completed < source.size()) {
    auto count = sink.WriteSome(source.subspan(completed));
    if (!count.ok()) {
      return count.status();
    }
    if (*count == 0) {
      return Status(ErrorCode::kIo,
                    "ByteSink made no progress during WriteAll");
    }
    if (*count > source.size() - completed) {
      return Status(ErrorCode::kInternal,
                    "ByteSink consumed more bytes than requested");
    }
    completed += *count;
  }
  return Status::Ok();
}

Result<File> File::OpenRead(const std::filesystem::path& path) {
  auto file = internal_core_io::OpenNativeFile(path, false);
  if (!file.ok()) {
    return file.status();
  }
  return File(*file, true, false);
}

Result<File> File::OpenWrite(const std::filesystem::path& path) {
  auto file = internal_core_io::OpenNativeFile(path, true);
  if (!file.ok()) {
    return file.status();
  }
  return File(*file, false, true);
}

File::File(void* handle, bool readable, bool writable) noexcept
    : handle_(handle), readable_(readable), writable_(writable) {}

File::File(File&& other) noexcept
    : handle_(std::exchange(other.handle_, nullptr)),
      readable_(std::exchange(other.readable_, false)),
      writable_(std::exchange(other.writable_, false)) {}

File& File::operator=(File&& other) noexcept {
  if (this != &other) {
    ASC_CHECK_MESSAGE(
        !is_open(),
        "Close an open File explicitly before move-assigning another File");
    handle_ = std::exchange(other.handle_, nullptr);
    readable_ = std::exchange(other.readable_, false);
    writable_ = std::exchange(other.writable_, false);
  }
  return *this;
}

File::~File() {
  if (is_open()) {
    static_cast<void>(std::fclose(internal_core_io::ToFile(handle_)));
  }
}

Result<std::size_t> File::ReadSome(std::span<std::byte> destination) {
  if (destination.empty()) {
    return std::size_t{0};
  }
  if (!is_open() || !readable_) {
    return Status(ErrorCode::kInvalidState, "File is not open for reading");
  }

  std::FILE* file = internal_core_io::ToFile(handle_);
  errno = 0;
  const std::size_t count =
      std::fread(destination.data(), 1, destination.size(), file);
  if (count == 0 && std::ferror(file) != 0) {
    return internal_core_io::FileError("Local-file read failed");
  }
  return count;
}

Result<std::size_t> File::WriteSome(std::span<const std::byte> source) {
  if (source.empty()) {
    return std::size_t{0};
  }
  if (!is_open() || !writable_) {
    return Status(ErrorCode::kInvalidState, "File is not open for writing");
  }

  errno = 0;
  const std::size_t count = std::fwrite(source.data(), 1, source.size(),
                                        internal_core_io::ToFile(handle_));
  if (count == 0) {
    return internal_core_io::FileError("Local-file write failed");
  }
  return count;
}

Status File::Flush() {
  if (!is_open() || !writable_) {
    return Status(ErrorCode::kInvalidState, "File is not open for writing");
  }
  errno = 0;
  if (std::fflush(internal_core_io::ToFile(handle_)) != 0) {
    return internal_core_io::FileError("Local-file flush failed");
  }
  return Status::Ok();
}

Status File::Close() noexcept {
  if (!is_open()) {
    return Status::Ok();
  }
  std::FILE* file = internal_core_io::ToFile(handle_);
  handle_ = nullptr;
  readable_ = false;
  writable_ = false;
  errno = 0;
  if (std::fclose(file) != 0) {
    return Status(ErrorCode::kIo, {}, {}, static_cast<std::int64_t>(errno));
  }
  return Status::Ok();
}

Result<std::string> ReadTextFile(const std::filesystem::path& path,
                                 std::size_t maximum_bytes) {
  auto file = File::OpenRead(path);
  if (!file.ok()) {
    return file.status();
  }

  std::string output;
  constexpr std::size_t kChunkSize = 4096;
  std::array<std::byte, kChunkSize> chunk{};
  while (true) {
    const std::size_t remaining = maximum_bytes - output.size();
    const std::size_t request =
        remaining < chunk.size() ? remaining + 1U : chunk.size();
    auto count = file->ReadSome(std::span<std::byte>(chunk).first(request));
    if (!count.ok()) {
      return count.status();
    }
    if (*count == 0) {
      break;
    }
    if (*count > remaining) {
      return Status(ErrorCode::kOverflow,
                    "Text file exceeds the configured size limit");
    }
    output.append(reinterpret_cast<const char*>(chunk.data()), *count);
  }
  const Status close_status = file->Close();
  if (!close_status.ok()) {
    return close_status;
  }
  return output;
}

Status WriteTextFile(const std::filesystem::path& path, std::string_view text) {
  auto file = File::OpenWrite(path);
  if (!file.ok()) {
    return file.status();
  }
  const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
  const Status write_status = WriteAll(*file, bytes);
  if (!write_status.ok()) {
    return write_status;
  }
  const Status flush_status = file->Flush();
  if (!flush_status.ok()) {
    return flush_status;
  }
  return file->Close();
}

}  // namespace asc
