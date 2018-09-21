/*
 * Copyright (c) 2014, Ford Motor Company
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following
 * disclaimer in the documentation and/or other materials provided with the
 * distribution.
 *
 * Neither the name of the Ford Motor Company nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef SRC_COMPONENTS_INCLUDE_UTILS_DATA_ACCESSOR_H_
#define SRC_COMPONENTS_INCLUDE_UTILS_DATA_ACCESSOR_H_

#include "utils/lock.h"

// This class is for thread-safe const access to data
template <class T>
class DataAccessor {
 public:
  DataAccessor(const T& data,
               const std::shared_ptr<sync_primitives::BaseLock>& lock)
      : data_(data), lock_(lock), counter_(new uint32_t(0)) {
    lock_->Acquire();
  }

  DataAccessor(const DataAccessor<T>& other)
      : data_(other.data_), lock_(other.lock_), counter_(other.counter_) {
    ++(*counter_);
  }

  ~DataAccessor() {
    if (0 == *counter_) {
      lock_->Release();
    } else {
      --(*counter_);
    }
  }
  const T& GetData() const {
    return data_;
  }
  T& GetMutableData() const {
    return const_cast<T&>(data_);
  }

 private:
  void* operator new(size_t size);
  const T& data_;
  // Require that the lock lives at least as long as the DataAccessor
  const std::shared_ptr<sync_primitives::BaseLock> lock_;
  std::shared_ptr<uint32_t> counter_;
};

#endif  // SRC_COMPONENTS_INCLUDE_UTILS_DATA_ACCESSOR_H_
