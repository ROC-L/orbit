// Copyright (c) 2022 The Orbit Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MIZAR_DATA_BASELINE_AND_COMPARISON_HELPER_H_
#define MIZAR_DATA_BASELINE_AND_COMPARISON_HELPER_H_

#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <stdint.h>

#include <string>

#include "MizarBase/AbsoluteAddress.h"
#include "MizarBase/SampledFunctionId.h"
#include "MizarData/MizarDataProvider.h"

namespace orbit_mizar_data {

struct AddressToIdAndIdToName {
  absl::flat_hash_map<orbit_mizar_base::AbsoluteAddress, orbit_mizar_base::SFID>
      baseline_address_to_sfid;
  absl::flat_hash_map<orbit_mizar_base::AbsoluteAddress, orbit_mizar_base::SFID>
      comparison_address_to_sfid;
  absl::flat_hash_map<orbit_mizar_base::SFID, std::string> sfid_to_name;
};

[[nodiscard]] AddressToIdAndIdToName AssignSampledFunctionIds(
    const absl::flat_hash_map<orbit_mizar_base::AbsoluteAddress, FunctionSymbol>&
        baseline_address_to_symbol,
    const absl::flat_hash_map<orbit_mizar_base::AbsoluteAddress, FunctionSymbol>&
        comparison_address_to_symbol);

}  // namespace orbit_mizar_data

#endif  // MIZAR_DATA_BASELINE_AND_COMPARISON_HELPER_H_
