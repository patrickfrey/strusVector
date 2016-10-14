/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for breeding good representants of similarity classes with help of genetic algorithms
#ifndef _STRUS_VECTOR_SPACE_MODEL_GENMODEL_HPP_INCLUDED
#define _STRUS_VECTOR_SPACE_MODEL_GENMODEL_HPP_INCLUDED
#include "simHash.hpp"
#include "simGroup.hpp"
#include "simRelationMap.hpp"
#include "indexListMap.hpp"
#include <vector>
#include <list>
#include <set>
#include <map>

namespace strus {

/// \brief Some constants that are not configurable yet
#define STRUS_VECTOR_MAXAGE_MATURE_PERCENTAGE 30   // define what precentage of age compared with maxage is considered mature
#define STRUS_VECTOR_BAD_FITNESS_FRACTION     0.6  // factor of fitness considered bad compared with the best fitness of the group, used to decide wheter a 'mature' group can be left when maximum capacity of releations is reached

typedef IndexListMap<SampleIndex,FeatureIndex> SampleFeatureIndexMap;
typedef IndexListMap<FeatureIndex,SampleIndex> FeatureSampleIndexMap;

/// \brief Structure for implementing unsupervised learning of SimHash group representants with help of genetic algorithms
class GenModel
{
public:
	/// \brief Default constructor
	GenModel()
		:m_threads(0)
		,m_simdist(0),m_raddist(0),m_eqdist(0)
		,m_mutations(0),m_votes(0)
		,m_descendants(0),m_maxage(0),m_iterations(0)
		,m_assignments(0),m_isaf(0.0)
		,m_with_singletons(0)
		{}
	/// \brief Constructor
	GenModel( unsigned int threads_, unsigned int simdist_, unsigned int raddist_, unsigned int eqdist_, unsigned int mutations_, unsigned int votes_, unsigned int descendants_, unsigned int maxage_, unsigned int iterations_, unsigned int assignments_, float isaf_, bool with_singletons_)
		:m_threads(threads_)
		,m_simdist(simdist_),m_raddist(raddist_),m_eqdist(eqdist_)
		,m_mutations(mutations_),m_votes(votes_)
		,m_descendants(descendants_),m_maxage(maxage_),m_iterations(iterations_)
		,m_assignments(assignments_),m_isaf(isaf_)
		,m_with_singletons(with_singletons_)
		{}
	/// \brief Copy constructor
	GenModel( const GenModel& o)
		:m_threads(o.m_threads)
		,m_simdist(o.m_simdist),m_raddist(o.m_raddist),m_eqdist(o.m_eqdist)
		,m_mutations(o.m_mutations),m_votes(o.m_votes)
		,m_descendants(o.m_descendants),m_maxage(o.m_maxage),m_iterations(o.m_iterations)
		,m_assignments(o.m_assignments),m_isaf(o.m_isaf)
		,m_with_singletons(o.m_with_singletons)
		{}

	/// \brief map contents to string in readable form
	std::string tostring() const;

	/// \brief Collecting all similar sample relations in a sparse matrix
	SimRelationMap getSimRelationMap(
			const std::vector<SimHash>& samples,
			const char* logfile) const;

	/// \brief Unsupervised learning of a good group representantion of the sample set passed as argument
	std::vector<SimHash> run(
			SampleFeatureIndexMap& sampleFeatureIndexMap,
			FeatureSampleIndexMap& featureSampleIndexMap,
			const std::vector<SimHash>& samples,
			const SimRelationMap& simrelmap,
			const char* logfile) const;

private:
	unsigned int m_threads;			///< maximum number of threads to use (0 = no threading)
	unsigned int m_simdist;			///< maximal distance to be considered similar
	unsigned int m_raddist;			///< maximal radius distance of group elements to centroid
	unsigned int m_eqdist;			///< maximum distance to be considered equal
	unsigned int m_mutations;		///< maximum number of random selected mutation candidates of the non kernel elements of a group
	unsigned int m_votes;			///< maximum number of votes to use to determine a mutation direction
	unsigned int m_descendants;		///< number of descendants of which the fittest is selected
	unsigned int m_maxage;			///< upper bound value used for calculate number of mutations (an older individuum mutates less)
	unsigned int m_iterations;		///< number of iterations
	unsigned int m_assignments;		///< maximum number of assignments on a sample to a group
	float m_isaf;				///< fraction of samples of a superset that has to be in a subset for declaring the subset as dependent (is a) of the superset
	bool m_with_singletons;			///< true, if singletons should also get into the result
};

}//namespace
#endif


