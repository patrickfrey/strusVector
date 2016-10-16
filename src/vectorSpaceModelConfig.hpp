/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Configuration structure for the strus standard vector space model
#ifndef _STRUS_VECTOR_SPACE_MODEL_CONFIG_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_CONFIG_HPP_INCLUDED
#include <string>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

struct VectorSpaceModelConfig
{
	enum Defaults {
		DefaultThreads = 0,
		DefaultDim = 300,
		DefaultBits = 64,
		DefaultVariations = 32,
		DefaultSimDist = 340,	//< 340 out of 2K (32*64) ~ cosine dist 0.9
		DefaultRadDist = 320,	//< 340 out of 2K (32*64) ~ cosine dist 0.9
		DefaultEqDist = 60,
		DefaultMutations = 50,
		DefaultMutationVotes = 13,
		DefaultDescendants = 10,
		DefaultMaxAge = 20,
		DefaultIterations = 20,
		DefaultAssignments = 7,
		DefaultIsaf = 60,
		DefaultWithSingletons = 0
	};
	VectorSpaceModelConfig( const VectorSpaceModelConfig& o);
	VectorSpaceModelConfig();
	VectorSpaceModelConfig( const std::string& config, ErrorBufferInterface* errorhnd);

	std::string tostring() const;

	std::string path;		///< path of model
	std::string prepath;		///< for builder: path with preprocessed model (similarity relation map and input LSH values are precalculated)
	std::string logfile;		///< file where to log some status data
	unsigned int threads;		///< maximum number of threads to use (0 = no threading)
	unsigned int dim;		///< input vector dimension
	unsigned int bits;		///< number of bits to calculate for an LSH per variation
	unsigned int variations;	///< number of variations
	unsigned int simdist;		///< maximum LSH edit distance considered as similarity
	unsigned int raddist;		///< centroid radius distance (smaller than simdist)
	unsigned int eqdist;		///< maximum LSH edit distance considered as equal
	unsigned int mutations;		///< number of mutations when a genetic code is changed
	unsigned int votes;		///< number of votes to decide in which direction a mutation should go
	unsigned int descendants;	///< number of descendants to evaluate the fitest of when trying to change an individual
	unsigned int maxage;		///< a factor used to slow down mutation rate
	unsigned int iterations;	///< number of iterations in the loop
	unsigned int assignments;	///< maximum number of group assignments for each input vector
	float isaf;			///< fraction of elements of a superset that has to be in a subset for declaring the subset as dependent (is a) of the superset
	bool with_singletons;		///< true, if singleton vectors thould also get into the result
};

}//namespace
#endif


