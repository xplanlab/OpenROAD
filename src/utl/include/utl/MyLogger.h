//
// Created by zarkin404 on 2023/2/1.
//

#pragma once

#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/stdout_color_sinks.h"

namespace utl {

template <typename... Args>
void myLogger(const std::string& message, const Args&... args)
{
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("my.log");

  spdlog::logger logger("MY_LOGGER", {console_sink, file_sink});
  logger.set_level(spdlog::level::info);
  logger.set_pattern("[%n - %T.%e] %v");
  logger.info(message, args...);
}

}  // namespace utl
