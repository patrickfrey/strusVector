/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function for evaluating similarity relations (incl. multithreaded)
#ifndef _STRUS_VECTOR_SPACE_MODEL_GET_SIMHASH_VALUES_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_GET_SIMHASH_VALUES_HPP_INCLUDED
#include "simHash.hpp"
#include <vector>

namespace strus {

/// \brief Forward declaration
class LshModel;

/// \brief Calculation of similarity LSH values of an array with possibly multiple threads
/// \param[in] lshmodel defines the mapping of vectors to LSH values
/// \param[in] vectors array of vectors to process
/// \param[in] threads number of threads to use, 0 for no threading at all
/// \return similarity LSH values
std::vector<SimHash> getSimhashValues(
		const LshModel& lshmodel,
		const std::vector<std::vector<double> >& vecar,
		unsigned int threads);

} //namespace
#endif


