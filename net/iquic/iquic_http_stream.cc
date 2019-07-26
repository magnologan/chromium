// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/iquic/iquic_http_stream.h"
#include "net/iquic/iquic_session.h"

#include "net/base/net_errors.h"

#include "base/callback_helpers.h"
#include "base/strings/string_util.h"
#include "net/base/io_buffer.h"
#include "net/base/url_util.h"
#include "net/http/http_request_info.h"
#include "net/http/http_response_headers.h"

#include <fmt/ostream.h>

#include <iostream>

namespace net {
namespace iquic {

IQuicHttpStream::IQuicHttpStream(IQuicSession* session)
    : session_(session), weak_factory_(this) {}

IQuicHttpStream::~IQuicHttpStream() {}

void IQuicHttpStream::OnHttpEvent(bnl::http3::event event) {
  switch (event) {
    case bnl::http3::event::type::settings:
      break;

    case bnl::http3::event::type::header: {
      ProcessHeader(event.header.header, event.header.fin);

      if (event.header.fin) {
        BNL_LOG_INFO(&logger_, "{}: Headers finished", event.header.id);
        state_ = State::body;
        if (callback_) {
          BNL_LOG_DEBUG(&logger_, "{}: Running HeadersFinished callback",
                        event.header.id);
          std::move(callback_).Run(OK);
        }
      }

      break;
    }

    case bnl::http3::event::type::body: {
      size_t min = std::min(event.body.buffer.size(), buffer_size_);
      if (callback_) {
        std::copy_n(event.body.buffer.data(), min, buffer_->data());
      } else {
        body_.push(std::move(event.body.buffer));
      }

      if (event.body.fin) {
        BNL_LOG_INFO(&logger_, "{}: Body finished", event.header.id);
        state_ = State::finished;
      }

      if (callback_) {
        std::move(callback_).Run(static_cast<int>(min));
      }

      break;
    }
  }
}

void IQuicHttpStream::ProcessHeader(bnl::http3::header_view header, bool fin) {
  std::string name(header.name().data(), header.name().size());
  std::string value(header.value().data(), header.value().size());

  if (name == ":status") {
    response_info_->headers->ReplaceStatusLine("HTTP/1.1 " + value);
  } else {
    response_info_->headers->AddHeader(name + ": " + value);
  }

  if (fin) {
    response_info_->remote_endpoint = session_->Peer();
    response_info_->connection_info =
        HttpResponseInfo::ConnectionInfo::CONNECTION_INFO_IQUIC_22;
    response_info_->was_alpn_negotiated = true;
    response_info_->alpn_negotiated_protocol =
        HttpResponseInfo::ConnectionInfoToString(
            response_info_->connection_info);
    response_info_->response_time = base::Time::Now();
    response_info_->was_fetched_via_spdy = true;
  }
}

int IQuicHttpStream::InitializeStream(const HttpRequestInfo* request_info,
                                      bool can_send_early,
                                      RequestPriority priority,
                                      const NetLogWithSource& net_log,
                                      CompletionOnceCallback callback) {
  request_info_ = request_info;

  auto r = session_->Request(base::BindRepeating(&IQuicHttpStream::OnHttpEvent,
                                                 weak_factory_.GetWeakPtr()));

  if (!r) {
    BNL_LOG_ERROR(&logger_, "error: {}", r.error().message());
    return ERR_FAILED;
  }

  request_ = std::move(r.value());

  BNL_LOG_TRACE(&logger_, "{}: InitializeStream", request_.id());

  request_.header({":method", request_info->method});
  request_.header({":scheme", request_info->url.scheme()});
  request_.header({":authority", GetHostAndOptionalPort(request_info->url)});
  request_.header({":path", request_info->url.PathForRequest()});

  return OK;
}

int IQuicHttpStream::SendRequest(const HttpRequestHeaders& request_headers,
                                 HttpResponseInfo* response,
                                 CompletionOnceCallback callback) {
  response_info_ = response;

  response_info_->request_time = base::Time::Now();
  response_info_->headers = new HttpResponseHeaders("");

  HttpRequestHeaders::Iterator it(request_headers);
  while (it.GetNext()) {
    std::string name = base::ToLowerASCII(it.name());
    if (name.empty() || name[0] == ':' || name == "connection" ||
        name == "proxy-connection" || name == "transfer-encoding" ||
        name == "host") {
      continue;
    }

    request_.header({name, it.value()});
  }

  request_.start();
  request_.fin();

  callback_ = std::move(callback);

  session_->Wake();

  return ERR_IO_PENDING;
}

int IQuicHttpStream::ReadResponseHeaders(CompletionOnceCallback callback) {
  BNL_LOG_TRACE(&logger_, "{}: ReadResponseHeaders", request_.id());

  if (state_ != State::headers) {
    return OK;
  }

  callback_ = std::move(callback);
  return ERR_IO_PENDING;
}

int IQuicHttpStream::ReadResponseBody(IOBuffer* buf,
                                      int buf_len,
                                      CompletionOnceCallback callback) {
  BNL_LOG_TRACE(&logger_, "{}: ReadResponseBody", request_.id());

  if (state_ == State::finished && body_.empty()) {
    return OK;
  }

  if (!body_.empty()) {
    size_t bytes_read = 0;
    size_t to_copy = 0;

    do {
      const bnl::base::buffer& first = body_.front();

      to_copy =
          std::min(first.size(), static_cast<size_t>(buf_len) - bytes_read);
      std::copy_n(first.data(), to_copy, buf->data() + bytes_read);

      body_.consume(to_copy);
      bytes_read += to_copy;
    } while (to_copy != 0 && !body_.empty());

    return static_cast<int>(bytes_read);
  }

  buffer_ = buf;
  buffer_size_ = static_cast<size_t>(buf_len);
  callback_ = std::move(callback);

  return ERR_IO_PENDING;
}

void IQuicHttpStream::Close(bool not_reusable) {
  BNL_LOG_TRACE(&logger_, "{}: Close", request_.id());
  session_->Remove(std::move(request_));
}

bool IQuicHttpStream::IsResponseBodyComplete() const {
  BNL_LOG_DEBUG(&logger_, "{}: IsResponseBodyComplete", request_.id());
  return state_ == State::finished;
}

// These methods did not require an implementation to get the file based
// implementation working.

bool IQuicHttpStream::IsConnectionReused() const {
  return false;
}

void IQuicHttpStream::SetConnectionReused() {}

bool IQuicHttpStream::CanReuseConnection() const {
  return false;
}

int64_t IQuicHttpStream::GetTotalReceivedBytes() const {
  return 0;
}

int64_t IQuicHttpStream::GetTotalSentBytes() const {
  return 0;
}

bool IQuicHttpStream::GetLoadTimingInfo(
    LoadTimingInfo* load_timing_info) const {
  return false;
}

void IQuicHttpStream::GetSSLInfo(SSLInfo* ssl_info) {}

bool IQuicHttpStream::GetAlternativeService(
    AlternativeService* alternative_service) const {
  return false;
}

void IQuicHttpStream::GetSSLCertRequestInfo(
    SSLCertRequestInfo* cert_request_info) {}

bool IQuicHttpStream::GetRemoteEndpoint(IPEndPoint* endpoint) {
  return false;
}

void IQuicHttpStream::Drain(HttpNetworkSession* session) {}

void IQuicHttpStream::PopulateNetErrorDetails(NetErrorDetails* details) {}

void IQuicHttpStream::SetPriority(RequestPriority priority) {}

HttpStream* IQuicHttpStream::RenewStreamForAuth() {
  return nullptr;
}

void IQuicHttpStream::SetRequestHeadersCallback(
    RequestHeadersCallback callback) {}

}  // namespace iquic
}  // namespace net