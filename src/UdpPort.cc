/* Copyright 2017 Streampunk Media Ltd.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <nan.h>
#include "UdpPort.h"
#include "MyWorker.h"
#include "Memory.h"
#include "iNetworkDriver.h"
#include "NetworkFactory.h"

using namespace v8;

namespace streampunk {

class UdpPortProcessData : public iProcessData {
public:
  UdpPortProcessData(const std::string &errStr, const tBufVec &bufVec) 
    : mErrStr(errStr), mBufVec(bufVec) {}
  ~UdpPortProcessData() {}

  std::string errStr() const { return mErrStr; }
  tBufVec bufVec() const { return mBufVec; }

private:
  std::string mErrStr;
  tBufVec mBufVec;
};

class UdpPortBindProcessData : public iProcessData {
public:
  UdpPortBindProcessData(uint32_t port, const std::string &addrStr)
    : mPort(port), mAddrStr(addrStr) {}
  ~UdpPortBindProcessData() {}

  uint32_t mPort;
  std::string mAddrStr;
};

class UdpPortSendProcessData : public iProcessData {
public:
  UdpPortSendProcessData(const tUIntVec &sendVec, uint32_t port, const std::string &addrStr) 
    : mSendVec(sendVec), mPort(port), mAddrStr(addrStr) {}
  ~UdpPortSendProcessData() {}

  const tUIntVec mSendVec;
  const uint32_t mPort;
  const std::string mAddrStr;
};

class UdpPortCloseProcessData : public iProcessData {
public:
  UdpPortCloseProcessData() {}
  ~UdpPortCloseProcessData() {}
};

UdpPort::UdpPort(std::string ipType, bool reuseAddr, bool recvArray, uint32_t packetSize, 
                 uint32_t recvMinPackets, uint32_t sendMinPackets, 
                 Nan::Callback *portCallback, Nan::Callback *callback) 
  : mRecvArray(recvArray),
    mWorker(new MyWorker(callback, portCallback)),
    mNetwork(NetworkFactory::createNetwork(ipType, reuseAddr, packetSize, recvMinPackets, sendMinPackets)),
    mListenThread(std::thread(&UdpPort::listenLoop, this)) {
  AsyncQueueWorker(mWorker);
}
UdpPort::~UdpPort() {}

void UdpPort::listenLoop() {
  bool active = true;

  while (active) {
    std::string errStr;
    tBufVec bufVec;
    active = !mNetwork->processCompletions(errStr, bufVec);
    if (active) {
      if (!errStr.empty() || !bufVec.empty()) {
        mWorker->doProcess(std::make_shared<UdpPortProcessData>(errStr, bufVec), this, NULL);
      }
    }
    else
      mWorker->quit();
  }
}

// iProcess
void UdpPort::doProcess (std::shared_ptr<iProcessData> processData, std::string &errStr, 
                         tBufVec &bufVec, bool &recvArray, uint32_t &port, std::string &addrStr) {
  try {
    std::shared_ptr<UdpPortProcessData> upd = std::dynamic_pointer_cast<UdpPortProcessData>(processData);
    if (upd) {
      errStr = upd->errStr();
      bufVec = upd->bufVec();
      recvArray = mRecvArray;
    }

    std::shared_ptr<UdpPortBindProcessData> ubpd = std::dynamic_pointer_cast<UdpPortBindProcessData>(processData);
    if (ubpd) {
      mNetwork->Bind(ubpd->mPort, ubpd->mAddrStr);
      port = ubpd->mPort;
      addrStr = ubpd->mAddrStr;
    }

    std::shared_ptr<UdpPortSendProcessData> uspd = std::dynamic_pointer_cast<UdpPortSendProcessData>(processData);
    if (uspd) {
      mNetwork->Send(uspd->mSendVec, uspd->mPort, uspd->mAddrStr);
      mNetwork->CommitSend();
    }

    std::shared_ptr<UdpPortCloseProcessData> ucpd = std::dynamic_pointer_cast<UdpPortCloseProcessData>(processData);
    if (ucpd) {
      mNetwork->Close();
    }
  } catch (std::runtime_error& err) {
    errStr = err.what();
  }
}

NAN_METHOD(UdpPort::AddMembership) {
  if (info.Length() != 2)
    return Nan::ThrowError("UdpPort AddMembership expects 2 arguments");
  String::Utf8Value mAddrStr(v8::Isolate::GetCurrent(), Nan::To<String>(info[0]).ToLocalChecked());
  String::Utf8Value uAddrStr(v8::Isolate::GetCurrent(), Nan::To<String>(info[1]).ToLocalChecked());

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mNetwork->AddMembership(*mAddrStr, *uAddrStr);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::DropMembership) {
  if (info.Length() != 2)
    return Nan::ThrowError("UdpPort DropMembership expects 2 arguments");
  String::Utf8Value mAddrStr(v8::Isolate::GetCurrent(), Nan::To<String>(info[0]).ToLocalChecked());
  String::Utf8Value uAddrStr(v8::Isolate::GetCurrent(), Nan::To<String>(info[1]).ToLocalChecked());

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mNetwork->DropMembership(*mAddrStr, *uAddrStr);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::SetTTL) {
  if (info.Length() != 1)
    return Nan::ThrowError("UdpPort SetTTL expects 1 argument");
  uint32_t ttl = Nan::To<uint32_t>(info[0]).FromJust();

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mNetwork->SetTTL(ttl);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::SetMulticastTTL) {
  if (info.Length() != 1)
    return Nan::ThrowError("UdpPort SetMulticastTTL expects 1 argument");
  uint32_t ttl = Nan::To<uint32_t>(info[0]).FromJust();

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mNetwork->SetMulticastTTL(ttl);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::SetBroadcast) {
  if (info.Length() != 1)
    return Nan::ThrowError("UdpPort SetBroadcast expects 1 argument");
  bool flag = Nan::To<bool>(info[0]).FromJust();

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mNetwork->SetBroadcast(flag);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::SetMulticastLoopback) {
  if (info.Length() != 1)
    return Nan::ThrowError("UdpPort SetMulticastLoopback expects 1 argument");
  bool flag = Nan::To<bool>(info[0]).FromJust();

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mNetwork->SetMulticastLoopback(flag);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }
  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::Bind) {
  if (info.Length() != 3)
    return Nan::ThrowError("UdpPort Bind expects 3 arguments");
  if (!info[2]->IsFunction())
    return Nan::ThrowError("UdpPort Bind requires a valid callback as the third parameter");

  uint32_t port = Nan::To<uint32_t>(info[0]).FromJust();
  String::Utf8Value addrStr(v8::Isolate::GetCurrent(), Nan::To<String>(info[1]).ToLocalChecked());
  Local<Function> callback = Local<Function>::Cast(info[2]);

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mWorker->doProcess(std::make_shared<UdpPortBindProcessData>(port, *addrStr), obj, new Nan::Callback(callback));
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::Send) {
  if (info.Length() != 6)
    return Nan::ThrowError("UdpPort Send expects 6 arguments");
  if (!info[0]->IsArray())
    return Nan::ThrowError("UdpPort Send requires a valid buffer array as the first parameter");
  if (!info[5]->IsFunction())
    return Nan::ThrowError("UdpPort Send requires a valid callback as the sixth parameter");

  Local<Array> bufArray = Local<Array>::Cast(info[0]);
  uint32_t offset = Nan::To<uint32_t>(info[1]).FromJust();
  uint32_t length = Nan::To<uint32_t>(info[2]).FromJust();
  uint32_t port = Nan::To<uint32_t>(info[3]).FromJust();
  String::Utf8Value addrStr(v8::Isolate::GetCurrent(), Nan::To<String>(info[4]).ToLocalChecked());
  Nan::Callback *callback = new Nan::Callback(Local<Function>::Cast(info[5]));

  if (1 == bufArray->Length()) {
    uint32_t buffLen = (uint32_t)node::Buffer::Length(bufArray->Get(v8::Isolate::GetCurrent()->GetCurrentContext(), 0).ToLocalChecked());
    if (offset + length > buffLen)
      return Nan::ThrowError("UdpPort Send - out of range offset/length");
  }

  tBufVec bufVec;
  for (uint32_t i = 0; i < bufArray->Length(); ++i) {
    Local<Object> bufferObj = Local<Object>::Cast(bufArray->Get(v8::Isolate::GetCurrent()->GetCurrentContext(), i).ToLocalChecked());
    uint8_t *sendBuf = (uint8_t *)node::Buffer::Data(bufferObj);
    uint32_t bufLen = (uint32_t)node::Buffer::Length(bufferObj);
    if (1 == bufArray->Length()) {
      sendBuf += offset;
      bufLen = length;
    }
    bufVec.push_back(Memory::makeNew(sendBuf, bufLen));
  } 

  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    tUIntVec sendVec = obj->mNetwork->makeSendPackets(bufVec);
    obj->mWorker->doProcess(std::make_shared<UdpPortSendProcessData>(sendVec, port, *addrStr), obj, callback);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }

  info.GetReturnValue().SetUndefined();
}

NAN_METHOD(UdpPort::Close) {
  UdpPort *obj = Nan::ObjectWrap::Unwrap<UdpPort>(info.Holder());
  try {
    obj->mWorker->doProcess(std::make_shared<UdpPortCloseProcessData>(), obj, NULL);
  } catch (std::runtime_error& err) {
    return Nan::ThrowError(Nan::New(err.what()).ToLocalChecked());
  }
  info.GetReturnValue().SetUndefined();
}

NAN_MODULE_INIT(UdpPort::Init) {
  Local<FunctionTemplate> tpl = Nan::New<FunctionTemplate>(New);
  tpl->SetClassName(Nan::New("UdpPort").ToLocalChecked());
  tpl->InstanceTemplate()->SetInternalFieldCount(1);

  SetPrototypeMethod(tpl, "addMembership", AddMembership);
  SetPrototypeMethod(tpl, "dropMembership", DropMembership);
  SetPrototypeMethod(tpl, "setTTL", SetTTL);
  SetPrototypeMethod(tpl, "setMulticastTTL", SetMulticastTTL);
  SetPrototypeMethod(tpl, "setBroadcast", SetBroadcast);
  SetPrototypeMethod(tpl, "setMulticastLoopback", SetMulticastLoopback);
  SetPrototypeMethod(tpl, "bind", Bind);
  SetPrototypeMethod(tpl, "send", Send);
  SetPrototypeMethod(tpl, "close", Close);

  constructor().Reset(Nan::GetFunction(tpl).ToLocalChecked());
  Nan::Set(target, Nan::New("UdpPort").ToLocalChecked(),
    Nan::GetFunction(tpl).ToLocalChecked());
}

} // namespace streampunk
