// Copyright(c) 2015-present, Gabi Melman & spdlog contributors.
// Distributed under the MIT License (http://opensource.org/licenses/MIT)

#pragma once

#ifndef SPDLOG_HEADER_ONLY
#include <spdlog/sinks/septa_file_sink.h>
#endif

#include <spdlog/common.h>

#include <spdlog/details/file_helper.h>
#include <spdlog/details/null_mutex.h>
#include <spdlog/details/os.h>
#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

namespace spdlog {
namespace sinks {

template <typename Mutex>
SPDLOG_INLINE septa_file_sink<Mutex>::septa_file_sink(filename_t base_filename,
                                                      std::size_t max_size,
                                                      std::size_t max_files,
                                                      bool rotate_on_open)
    : base_filename_(std::move(base_filename)), max_size_(max_size),
      max_files_(max_files) {
  file_helper_.open(calc_filename(base_filename_, 0));
  current_size_ = file_helper_.size();

  if (current_size_ > 0) {
    rotate_();
  }
}

template <typename Mutex>
SPDLOG_INLINE filename_t septa_file_sink<Mutex>::calc_filename(
    const filename_t &filename, std::size_t index) {
  // Get current time point
  auto now = std::chrono::system_clock::now();

  // Convert to time_t for calendar breakdown
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm local_tm = *std::localtime(&t);

  // Extract calendar components
  int year = local_tm.tm_year + 1900; // tm_year is years since 1900
  int month = local_tm.tm_mon + 1;    // tm_mon is months since January [0-11]
  int day = local_tm.tm_mday;
  int hour = local_tm.tm_hour;
  int minute = local_tm.tm_min;
  int second = local_tm.tm_sec;

  // Extract milliseconds
  int ms = std::chrono::duration_cast<std::chrono::milliseconds>(
               now.time_since_epoch())
               .count() %
           1000;

  std::ostringstream oss;
  oss << std::setw(2) << std::setfill('0') << year;

  std::string logFileName = oss.str();

  oss.str("");

  oss << std::setw(2) << std::setfill('0') << month;

  logFileName += oss.str();

  oss.str("");

  oss << std::setw(2) << std::setfill('0') << day;

  logFileName += oss.str();

  oss.str("");

  logFileName += "_";

  oss << std::setw(2) << std::setfill('0') << hour;

  logFileName += oss.str();

  oss.str("");

  oss << std::setw(2) << std::setfill('0') << minute;

  logFileName += oss.str();

  oss.str("");

  oss << std::setw(2) << std::setfill('0') << second;

  logFileName += oss.str();

  oss.str("");

  logFileName += ".";

  oss << std::setw(4) << std::setfill('0') << ms;

  logFileName += oss.str();

  filename_t basename, ext;
  std::tie(basename, ext) = details::file_helper::split_by_extension(filename);
  return fmt::format(SPDLOG_FILENAME_T("{}_{}.log"), basename, logFileName);
}

template <typename Mutex>
SPDLOG_INLINE filename_t septa_file_sink<Mutex>::filename() {
  std::lock_guard<Mutex> lock(base_sink<Mutex>::mutex_);
  return file_helper_.filename();
}

template <typename Mutex>
SPDLOG_INLINE void
septa_file_sink<Mutex>::sink_it_(const details::log_msg &msg) {
  std::lock_guard<Mutex> lock(septa_file_sink<Mutex>::rotateMutex_);

  memory_buf_t formatted;
  base_sink<Mutex>::formatter_->format(msg, formatted);

  current_size_ += formatted.size();

  if (current_size_ > max_size_) {
    rotate_();
    current_size_ = formatted.size();
  }

  file_helper_.write(formatted);
}

template <typename Mutex> SPDLOG_INLINE void septa_file_sink<Mutex>::flush_() {
  file_helper_.flush();
}

template <typename Mutex> SPDLOG_INLINE void septa_file_sink<Mutex>::rotate_() {
  file_helper_.close();

  file_helper_.open(calc_filename(base_filename_, 0));

  // Delete old log files beyond max_files_ limit
  try {
    namespace fs = std::filesystem;

    // Get the directory and base name
    fs::path base_path(base_filename_);
    fs::path dir_path = base_path.parent_path();
    std::string basename =
        base_path.stem().string(); // Get filename without extension

    if (!fs::exists(dir_path)) {
      return;
    }

    // Collect all matching log files
    std::vector<fs::path> log_files;
    for (const auto &entry : fs::directory_iterator(dir_path)) {
      if (entry.is_regular_file()) {
        std::string filename = entry.path().filename().string();
        // Check if file matches the pattern: basename_YYYYMMDD_HHMMSS.XXXX.log
        if (filename.find(basename) == 0 &&
            filename.length() > basename.length() &&
            filename.compare(filename.length() - 4, 4, ".log") == 0) {
          log_files.push_back(entry.path());
        }
      }
    }

    // Sort in descending order (most recent first, based on filename sorting)
    std::sort(log_files.rbegin(), log_files.rend());

    // Delete files beyond max_files_ limit
    if (log_files.size() > max_files_) {
      for (size_t i = max_files_; i < log_files.size(); ++i) {
        details::os::remove_if_exists(
            std::filesystem::absolute(log_files[i]).string());
      }
    }
  } catch (const std::exception &) {
    // Silently ignore errors during cleanup - don't let file deletion failures
    // affect logging
  }
}

// delete the target if exists, and rename the src file  to target
// return true on success, false otherwise.
template <typename Mutex>
SPDLOG_INLINE bool
septa_file_sink<Mutex>::rename_file_(const filename_t &src_filename,
                                     const filename_t &target_filename) {
  // try to delete the target file in case it already exists.
  // (void)details::os::remove(target_filename);
  //   return details::os::rename(src_filename, target_filename) == 0;
  return true;
}

} // namespace sinks
} // namespace spdlog
