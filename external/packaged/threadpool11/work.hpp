/**
 * threadpool11
 * Copyright (C) 2013, 2014, 2015, 2016, 2017, 2018, 2019  Tolga HOSGOR
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include <functional>

namespace threadpool11 {

class work {
public:
  using callable_t = std::function<void()>;

  enum class type_t {
    STANDARD,
    TERMINAL,
  };

public:
  work(type_t type, callable_t callable)
    : type_{std::move(type)}
    , callable_{std::move(callable)} {
  }

  work(const work&) = delete;
  work(work&&) = default;

  work& operator=(const work&) = delete;
  work& operator=(work&&) = default;

  type_t type() const { return type_; }

  void operator()() const { callable_(); }

private:
  type_t type_;
  callable_t callable_;
};

}

