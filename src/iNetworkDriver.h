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

#ifndef NETWORKDRIVER_H
#define NETWORKDRIVER_H

#include <memory>

namespace streampunk {

class Memory;
typedef std::vector<std::shared_ptr<Memory> > tBufVec;
typedef std::vector<uint32_t> tUIntVec;

class iNetworkDriver {
public:
  virtual ~iNetworkDriver() {}

  virtual void AddMembership(std::string mAddrStr, std::string uAddrStr) = 0;
  virtual void DropMembership(std::string mAddrStr, std::string uAddrStr) = 0;
  virtual void SetTTL(uint32_t ttl) = 0;
  virtual void SetMulticastTTL(uint32_t ttl) = 0;
  virtual void SetBroadcast(bool flag) = 0;
  virtual void SetMulticastLoopback(bool flag) = 0;
  virtual void Bind(uint32_t &port, std::string &addrStr) = 0;
  virtual tUIntVec makeSendPackets(tBufVec bufVec) = 0;
  virtual void Send(const tUIntVec& bufVec, uint32_t port, std::string addrStr) = 0;
  virtual void CommitSend() = 0;
  virtual void Close() = 0;

  virtual bool processCompletions(std::string &errStr, tBufVec &bufVec) = 0;
};

} // namespace streampunk

#endif