/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Some local helper functions in the context of the armadillo library
#ifndef _STRUS_VECTOR_ARMADILLO_UTILS_HPP_INCLUDED
#define _STRUS_VECTOR_ARMADILLO_UTILS_HPP_INCLUDED
#include "armadillo"

namespace strus {

arma::fvec normalizeVector( const arma::fvec& vec);

}
#endif

