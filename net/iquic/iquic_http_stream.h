// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "base/memory/weak_ptr.h"
#include "base/memory/scoped_refptr.h"

#include "net/http/http_stream.h"
#include <bnl/http3/client/connection.hpp>
#include <bnl/log/console.hpp>
#include <bnl/base/buffers.hpp>

namespace quic {
class QuicClock;
class QuicAlarmFactory;
}  // namespace quic

namespace net {
namespace iquic {

class IQuicSession;
class DatagramClientSocket;

// Temporary file-based implementation of a HttpStream as a proof of concept new
// network component for Chromium. Reads websites from a folder in the current
// directory with the same name.
class NET_EXPORT_PRIVATE IQuicHttpStream : public HttpStream {
 public:
  IQuicHttpStream(IQuicSession* session);
  ~IQuicHttpStream() override;

  int InitializeStream(const HttpRequestInfo* request_info,
                       bool can_send_early,
                       RequestPriority priority,
                       const NetLogWithSource& net_log,
                       CompletionOnceCallback callback) override;
  int SendRequest(const HttpRequestHeaders& request_headers,
                  HttpResponseInfo* response,
                  CompletionOnceCallback callback) override;
  int ReadResponseHeaders(CompletionOnceCallback callback) override;
  int ReadResponseBody(IOBuffer* buf,
                       int buf_len,
                       CompletionOnceCallback callback) override;
  void Close(bool not_reusable) override;
  bool IsResponseBodyComplete() const override;
  bool IsConnectionReused() const override;
  void SetConnectionReused() override;
  bool CanReuseConnection() const override;
  int64_t GetTotalReceivedBytes() const override;
  int64_t GetTotalSentBytes() const override;
  bool GetLoadTimingInfo(LoadTimingInfo* load_timing_info) const override;
  void GetSSLInfo(SSLInfo* ssl_info) override;
  bool GetAlternativeService(
      AlternativeService* alternative_service) const override;
  void GetSSLCertRequestInfo(SSLCertRequestInfo* cert_request_info) override;
  bool GetRemoteEndpoint(IPEndPoint* endpoint) override;
  void Drain(HttpNetworkSession* session) override;
  void PopulateNetErrorDetails(NetErrorDetails* details) override;
  void SetPriority(RequestPriority priority) override;
  HttpStream* RenewStreamForAuth() override;
  void SetRequestHeadersCallback(RequestHeadersCallback callback) override;

 private:
  void OnHttpEvent(bnl::http3::event event);

  void ProcessHeader(bnl::http3::header_view header, bool fin);

  bnl::log::console logger_;
  bnl::http3::request::handle request_;
  bnl::base::buffers body_;

  enum class State { headers, body, finished };
  State state_ = State::headers;

  IQuicSession* session_;
  const HttpRequestInfo* request_info_;
  HttpResponseInfo* response_info_;
  CompletionOnceCallback callback_;

  scoped_refptr<IOBuffer> buffer_;
  size_t buffer_size_;

  base::WeakPtrFactory<IQuicHttpStream> weak_factory_;
};

}  // namespace iquic
}  // namespace net