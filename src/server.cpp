/***************************************************************************\
    server.cpp - iqdb server (database maintenance and queries)

    Copyright (C) 2008 piespy@gmail.com

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
\**************************************************************************/

#include <chrono>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>

#include <iqdb/debug.h>
#include <iqdb/imgdb.h>
#include <iqdb/imglib.h>
#include <iqdb/haar_signature.h>
#include <iqdb/metrics.h>
#include <iqdb/types.h>

#include <httplib.h>
#include <nlohmann/json.hpp>

using nlohmann::json;
using httplib::Server;
using iqdb::IQDB;

namespace iqdb {

static Server server;

static void signal_handler(int signal, siginfo_t* info, void* ucontext) {
  INFO("Received signal {} ({})", signal, strsignal(signal));

  if (signal == SIGSEGV) {
    INFO("Address: {}", info->si_addr);
    exit(1);
  }

  if (server.is_running()) {
    server.stop();
  }
}

void install_signal_handlers() {
  struct sigaction action = {};
  sigfillset(&action.sa_mask);
  action.sa_flags = SA_RESTART | SA_SIGINFO;

  action.sa_sigaction = signal_handler;

  sigaction(SIGINT, &action, NULL);
  sigaction(SIGTERM, &action, NULL);
  sigaction(SIGSEGV, &action, NULL);
}

bool check_is_valid(nlohmann::json json) {
  if(json.is_array() && json.size() == NUM_PIXELS_SQUARED) {
    return std::all_of(json.begin(), json.end(), [](const nlohmann::json& el){ return el.is_number_integer(); });
  }
  return false;
}

bool channel_param_valid(nlohmann::json json) {
  if(json.is_object() && json.contains("r") && json.contains("g") && json.contains("b")) {
    return check_is_valid(json["r"]) && check_is_valid(json["g"]) && check_is_valid(json["b"]);
  }
  return false;
}

void validate_json_is_valid(nlohmann::json json) {
  if(!json.is_object() || !json.contains("channels") || !channel_param_valid(json["channels"])) {
    throw param_error("`POST /images` must be { 'channels': { 'r': [], 'g': [], 'b': [] }} 128^2 entries each");
  }
}

void http_server(const std::string host, const int port, const std::string database_filename) {
  INFO("Starting server...");

  std::shared_mutex mutex_;
  auto memory_db = std::make_unique<IQDB>(database_filename);

  install_signal_handlers();

  server.Post("/images/(\\d+)", [&](const auto &request, auto &response) {
    std::unique_lock lock(mutex_);

    const postId post_id = std::stoi(request.matches[1]);
    const auto json = json::parse(request.body);
    validate_json_is_valid(json);
    const auto channels = json["channels"];
    const auto signature = HaarSignature::from_channels(channels["r"], channels["g"], channels["b"]);
    memory_db->addImage(post_id, signature);

    nlohmann::json data = {
      { "post_id", post_id },
      { "hash", signature.to_string() },
    };

    response.set_content(data.dump(4), "application/json");
  });

  server.Delete("/images/(\\d+)", [&](const auto &request, auto &response) {
    std::unique_lock lock(mutex_);

    const postId post_id = std::stoi(request.matches[1]);
    memory_db->removeImage(post_id);

    json data = {
      { "post_id", post_id },
    };

    response.set_content(data.dump(4), "application/json");
  });

  server.Get("/images/(\\d+)", [&](const auto &request, auto &response) {
    std::unique_lock lock(mutex_);

    const postId post_id = std::stoi(request.matches[1]);
    auto image = memory_db->getImage(post_id);

    json data;
    if (image == std::nullopt) {
      data = {
        { "message", "Not found" }
      };
      response.status = 404;
    } else {
      data = {
        { "post_id", post_id },
        { "hash", image->haar().to_string() }
      };
    }
    response.set_content(data.dump(4), "application/json");
  });

  server.Post("/query", [&](const auto &request, auto &response) {
    std::shared_lock lock(mutex_);

    const auto json = json::parse(request.body);
    int limit = 10;
    if (json.contains("limit") && json["limit"].is_number_integer()) {
      limit = json["limit"];
    }

    sim_vector matches;
    if (json.contains("hash")) {
      HaarSignature haar = HaarSignature::from_hash(json["hash"]);
      matches = memory_db->queryFromSignature(haar, limit);
    } else if (json.contains("channels")) {
      validate_json_is_valid(json);
      const auto channels = json["channels"];
      matches = memory_db->queryFromChannels(channels["r"], channels["g"], channels["b"], limit);
    } else {
      throw param_error("POST /query requires either `hash` or `channels` param");
    }

    nlohmann::json data = json::array();
    for (const auto &match : matches) {
      auto image = memory_db->getImage(match.id);
      auto haar = image->haar();

      data += {
        { "post_id", match.id },
        { "score", match.score },
        { "hash", haar.to_string() },
      };
    }

    response.set_content(data.dump(4), "application/json");
  });

  server.Get("/status", [&](const auto &request, auto &response) {
    std::shared_lock lock(mutex_);

    const size_t count = memory_db->getImgCount();
    json data = {{"images", count}};

    response.set_content(data.dump(4), "application/json");
  });

  server.Get("/metrics", [&](const auto &request, auto &response) {
    std::shared_lock lock(mutex_);

    auto& registry = metrics::Registry::instance();
    registry.images_total.set(static_cast<double>(memory_db->getImgCount()));

    response.set_content(registry.render(), "text/plain; version=0.0.4");
  });

  server.set_logger([](const auto &req, const auto &res) {
    INFO("{} \"{} {} {}\" {} {}", req.remote_addr, req.method, req.path, req.version, res.status, res.body.size());

    const std::string route = req.matched_route.empty() ? req.path : req.matched_route;
    const std::string status = std::to_string(res.status);
    const double elapsed_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - req.start_time_).count();

    auto& registry = metrics::Registry::instance();
    registry.http_requests_total.increment({req.method, route, status});
    registry.http_request_duration_seconds.observe({req.method, route}, elapsed_seconds);
  });

  server.set_exception_handler([](const auto& req, auto& res, std::exception_ptr ep) {
    json data;
    try {
      std::rethrow_exception(ep);
    } catch (std::exception &e) {
      const auto message = e.what();

      data = {
        { "message", message }
      };

      ERROR("Exception: {}", message);

    } catch (...) {
      data = {
        { "message", "Unknown exception" }
      };
      ERROR("Exception: {}", "Unknown exception");
    }

    res.set_content(data.dump(4), "application/json");
    res.status = 500;
  });

  INFO("Listening on {}:{}.", host, port);
  server.listen(host.c_str(), port);
  INFO("Stopping server...");
}

void help() {
  printf(
    "Usage: iqdb COMMAND [ARGS...]\n"
    "  iqdb http [host] [port] [dbfile]  Run HTTP server on given host/port.\n"
    "  iqdb help                         Show this help.\n"
  );

  exit(0);
}

}
