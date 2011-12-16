// Copyright 2011 Google Inc. All Rights Reserved.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: nadavs@google.com <Nadav Samet>

namespace zrpc {

SimpleRpcChannel::SimpleRpcChannel(Connection* connection)
    : connection_(connection) {
}

SimpleRpcChannel::~SimpleRpcChannel() {
}

struct RpcResponseContext {
  RPC* rpc;
  ::google::protobuf::Message* response_msg;
  std::string* response_str;
  Closure* user_closure;
  ClientRequest client_request;
};

void SimpleRpcChannel::CallMethodFull(
    const std::string& service_name,
    const std::string& method_name,
    RPC* rpc,
    const std::string& request,
    std::string* response_str,
    ::google::protobuf::Message* response_msg,
    Closure* done) {
  CHECK(rpc->GetStatus() == GenericRPCResponse::INACTIVE);
  GenericRPCRequest generic_request;
  generic_request.set_service(service_name);
  generic_request.set_method(method_name);

  std::string msg_request = generic_request.SerializeAsString();
  zmq::message_t* msg_out = new zmq::message_t(msg_request.size());
  memcpy(msg_out->data(), msg_request.c_str(), msg_request.size());

  zmq::message_t* payload_out = new zmq::message_t(request.size());
  memcpy(payload_out->data(), request.c_str(), request.size());

  MessageVector vout;
  vout.push_back(msg_out);
  vout.push_back(payload_out);

  RpcResponseContext *response_context = new RpcResponseContext;
  response_context->client_request.closure = NewCallback(
      this, &SimpleRpcChannel::HandleClientResponse,
      response_context);
  response_context->rpc = rpc;
  response_context->client_request.deadline_ms = rpc->GetDeadlineMs();
  response_context->client_request.start_time = zclock_time();
  response_context->user_closure = done;
  response_context->response_str = response_str;
  response_context->response_msg = response_msg;
  rpc->SetStatus(GenericRPCResponse::INFLIGHT);
  rpc->connection_ = connection_;
  rpc->rpc_response_context_ = response_context;

  connection_->SendClientRequest(&response_context->client_request,
                                 vout);
}

void SimpleRpcChannel::CallMethod0(const std::string& service_name,
                                const std::string& method_name,
                                RPC* rpc,
                                const std::string& request,
                                std::string* response,
                                Closure* done) {
  CallMethodFull(service_name,
                 method_name,
                 rpc,
                 request,
                 response,
                 NULL,
                 done);
}

void SimpleRpcChannel::CallMethod(
    const google::protobuf::MethodDescriptor* method,
    RPC* rpc,
    const google::protobuf::Message* request,
    google::protobuf::Message* response,
    Closure* done) {
  CallMethodFull(method->service()->name(),
                 method->name(),
                 rpc,
                 request->SerializeAsString(),
                 NULL,
                 response,
                 done);
}

void SimpleRpcChannel::HandleClientResponse(
    RpcResponseContext* response_context) {
  ClientRequest& client_request = response_context->client_request;

  switch (client_request.status) {
    case ClientRequest::DEADLINE_EXCEEDED:
      response_context->rpc->SetStatus(
          GenericRPCResponse::DEADLINE_EXCEEDED);
      break;
    case ClientRequest::DONE: {
        GenericRPCResponse generic_response;
        zmq::message_t& msg_in = *response_context->client_request.result[0];
        CHECK(generic_response.ParseFromArray(msg_in.data(), msg_in.size()));
        if (generic_response.status() != GenericRPCResponse::OK) {
          response_context->rpc->SetFailed(generic_response.application_error(),
                                           generic_response.error());
        } else {
          response_context->rpc->SetStatus(GenericRPCResponse::OK);
          if (response_context->response_msg) {
            CHECK(response_context->response_msg->ParseFromArray(
                    response_context->client_request.result[1]->data(),
                    response_context->client_request.result[1]->size()));
          } else if (response_context->response_str) {
            response_context->response_str->assign(
                static_cast<char*>(
                    response_context->client_request.result[1]->data()),
                response_context->client_request.result[1]->size());
          }
        }
      }
      break;
    case ClientRequest::ACTIVE:
    case ClientRequest::INACTIVE:
    default:
      CHECK(false) << "Unexpected ClientRequest state: "
          << client_request.status;
  }
  if (response_context->user_closure) {
    response_context->user_closure->Run();
  }
  delete response_context;
}
}  // namespace zrpc