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
#include "simRelationReader.hpp"
#include "indexListMap.hpp"
#include <vector>
#include <list>
#include <set>
#include <map>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

/// \brief Factor used to calculate the maximum number of concepts from the number of samples if maxnofconcepts is not specified
#define FEATURE_MAXNOFCONCEPT_RELATION 16

/// \brief Structure for implementing unsupervised learning of SimHash group representants with help of genetic algorithms
class GenModel
{
public:
	/// \brief Default constructor
	GenModel()
		:m_errorhnd(0)
		,m_threads(0)
		,m_maxdist(0),m_simdist(0),m_raddist(0),m_eqdist(0)
		,m_mutations(0),m_votes(0)
		,m_descendants(0),m_maxage(0),m_iterations(0)
		,m_assignments(0),m_greediness(0),m_isaf(0.0),m_eqdiff(0.0)
		,m_with_singletons(0)
		{}
	/// \brief Constructor
	GenModel( unsigned int threads_, unsigned int maxdist_, unsigned int simdist_, unsigned int raddist_, unsigned int eqdist_, unsigned int mutations_, unsigned int votes_, unsigned int descendants_, unsigned int maxage_, unsigned int iterations_, unsigned int assignments_, unsigned int greediness_, float isaf_, float eqdiff_, bool with_singletons_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_)
		,m_threads(threads_)
		,m_maxdist(maxdist_),m_simdist(simdist_),m_raddist(raddist_),m_eqdist(eqdist_)
		,m_mutations(mutations_),m_votes(votes_)
		,m_descendants(descendants_),m_maxage(maxage_),m_iterations(iterations_)
		,m_assignments(assignments_),m_greediness(greediness_)
		,m_isaf(isaf_),m_eqdiff(eqdiff_)
		,m_with_singletons(with_singletons_)
		{}
	/// \brief Copy constructor
	GenModel( const GenModel& o)
		:m_errorhnd(o.m_errorhnd)
		,m_threads(o.m_threads)
		,m_maxdist(o.m_maxdist),m_simdist(o.m_simdist),m_raddist(o.m_raddist),m_eqdist(o.m_eqdist)
		,m_mutations(o.m_mutations),m_votes(o.m_votes)
		,m_descendants(o.m_descendants),m_maxage(o.m_maxage),m_iterations(o.m_iterations)
		,m_assignments(o.m_assignments),m_greediness(o.m_greediness)
		,m_isaf(o.m_isaf),m_eqdiff(o.m_eqdiff)
		,m_with_singletons(o.m_with_singletons)
		{}

	/// \brief map contents to string in readable form
	std::string tostring() const;

	/// \brief Unsupervised learning of a good group representantion of the sample set passed as argument
	std::vector<SimHash> run(
			std::vector<SampleIndex>& singletons,
			SampleConceptIndexMap& sampleConceptIndexMap,
			ConceptSampleIndexMap& conceptSampleIndexMap,
			const std::string& clname,
			const std::vector<SimHash>& samples,
			unsigned int maxconcepts_,
			const SimRelationReader& simrelreader,
			unsigned int nofThreads,
			const char* logfile) const;

private:
	ErrorBufferInterface* m_errorhnd;	///< interface for error messages
	unsigned int m_threads;			///< maximum number of threads to use (0 = no threading)
	unsigned int m_maxdist;			///< maximum distance
	unsigned int m_simdist;			///< maximum distance to be considered similar
	unsigned int m_raddist;			///< maximum radius distance of group elements to centroid
	unsigned int m_eqdist;			///< maximum distance to be considered equal
	unsigned int m_mutations;		///< maximum number of random selected mutation candidates of the non kernel elements of a group
	unsigned int m_votes;			///< maximum number of votes to use to determine a mutation direction
	unsigned int m_descendants;		///< number of descendants of which the fittest is selected
	unsigned int m_maxage;			///< upper bound value used for calculate number of mutations (an older individuum mutates less)
	unsigned int m_iterations;		///< number of iterations
	unsigned int m_assignments;		///< maximum number of assignments on a sample to a group
	unsigned int m_greediness;		///< non-negative integer specifying the greediness of collecting near elements into a newly created group (ascending)
	float m_isaf;				///< fraction of samples of a superset that has to be in a subset for declaring the subset as dependent (is a) of the superset
	float m_eqdiff;				///< fraction of maximum elements that can to differ for sets to considered to be equal (unfittest competitor elimination)
	bool m_with_singletons;			///< true, if singletons should also get into the result
};

}//namespace
#endif


