/*
 * Copyright (c) 2014-2015, Hewlett-Packard Development Company, LP.
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details. You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * HP designates this particular file as subject to the "Classpath" exception
 * as provided by HP in the LICENSE.txt file that accompanied this code.
 */
#ifndef FOEDUS_ASSORTED_BERNOULLI_RANDOM_HPP_
#define FOEDUS_ASSORTED_BERNOULLI_RANDOM_HPP_

#include <stdint.h>

#include <cmath>
#include <limits>

#include "uniform_random.hpp"

namespace foedus {
namespace assorted {
/**
 * @brief A simple Bernoulli generator.
 * @ingroup ASSORTED
 */

class BernoulliRandom {
 private:
  static constexpr uint32_t kMax = std::numeric_limits<uint32_t>::max();

 public:
  void init(double p, uint64_t urnd_seed) {
    p_ = p;
    urnd_.set_current_seed(urnd_seed);
  }

  BernoulliRandom(double p, uint64_t urnd_seed) {
    init(p, urnd_seed);
  }

  BernoulliRandom() {}

  bool next() {
    double u = urnd_.next_uint32() / static_cast<double>(kMax);
    if (u <= p_) {
      return true;
    }
    return false;
  }

  uint64_t  get_current_seed() const { return urnd_.get_current_seed(); }
  void      set_current_seed(uint64_t seed) { urnd_.set_current_seed(seed); }

 private:
  foedus::assorted::UniformRandom urnd_;
  double p_;
};

}  // namespace assorted
}  // namespace foedus

#endif  // FOEDUS_ASSORTED_BERNOULLI_RANDOM_HPP_

