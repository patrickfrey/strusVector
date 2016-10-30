/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function for evaluating similarity relations (incl. multithreaded)
#ifndef _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_GET_SIMILARITY_RELATION_MAP_HPP_INCLUDED
#include "simRelationMap.hpp"
#include "simHash.hpp"
#include <vector>

namespace strus {

/// \brief Get a part of the sparse matrix of similarity relations defined by edit distance of the LSH values of an array of samples
/// \param[in] samplear array of all samples to process
/// \param[in] idx_begin index of first element to process by this call
/// \param[in] idx_end index of end element to process by this call
/// \param[in] maxdist maximum edit distance to put into the result matrix
/// \param[in] maxsimsam maximum number of nearest neighbours put input similarity relation map
/// \param[in] rndsimsam number of additional random samples put into similarity relation map, if 'maxsimsam' is specified
/// \param[in] threads number of threads to use, 0 for no threading at all
/// \return similarity relation map
SimRelationMap getSimRelationMap(
		const std::vector<SimHash>& samplear, SampleIndex idx_begin, SampleIndex idx_end,
		unsigned int maxdist, unsigned int maxsimsam, unsigned int rndsimsam, unsigned int threads);

} //namespace
#endif


