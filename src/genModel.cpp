/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for breeding good representants of similarity classes with help of genetic algorithms
#include "genModel.hpp"
#include "genGroupContext.hpp"
#include "genGroupThreadContext.hpp"
#include "logger.hpp"
#include "sampleSimGroupMap.hpp"
#include "dependencyGraph.hpp"
#include "internationalization.hpp"
#include "strus/base/string_format.hpp"
#include <ctime>
#include <cmath>
#include <iostream>
#include <sstream>
#include <limits>
#include <algorithm>

using namespace strus;
#undef STRUS_LOWLEVEL_DEBUG

static void GenGroupProcedure_greedyChaseFreeFeatures( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	unsigned int mutationcnt = 0;
	SampleIndex si = startidx, se = endidx;
	for (; si != se; ++si)
	{
		genGroupContext->greedyChaseFreeFeatures( *groupIdAllocator, si, *simrelreader, *parameter, mutationcnt);
	}
	if (genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("free feature chase step triggered %u successful mutations"), mutationcnt);
}

static void GenGroupProcedure_greedyNeighbourGroupInterchange( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	unsigned int mutationcnt = 0;
	ConceptIndex gi = startidx, ge = endidx;
	for (; gi != ge; ++gi)
	{
		genGroupContext->greedyNeighbourGroupInterchange( *groupIdAllocator, gi, *parameter, mutationcnt);
	}
	if (genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("group interchange step triggered %u successful mutations"), mutationcnt);
}

static void GenGroupProcedure_improveGroups( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	unsigned int mutationcnt = 0;
	ConceptIndex gi = startidx, ge = endidx;
	for (; gi != ge; ++gi)
	{
		mutationcnt += genGroupContext->improveGroup( *groupIdAllocator, gi, *parameter) ? 1:0;
	}
	if (genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("group improve step triggered %u successful mutations"), mutationcnt);
}

static void GenGroupProcedure_similarNeighbourGroupElimination( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	unsigned int eliminationcnt = 0;
	ConceptIndex gi = startidx, ge = endidx;
	for (; gi != ge; ++gi)
	{
		eliminationcnt += genGroupContext->similarNeighbourGroupElimination( *groupIdAllocator, gi, *parameter) ? 1:0;
	}
	if (genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("neighbour elimination step triggered %u successful removals"), eliminationcnt);
}


std::vector<SimHash> GenModel::run(
		std::vector<SampleIndex>& singletons,
		SampleConceptIndexMap& sampleConceptIndexMap,
		ConceptSampleIndexMap& conceptSampleIndexMap,
		const std::string& clname,
		const std::vector<SimHash>& samplear,
		unsigned int maxconcepts_,
		const SimRelationReader& simrelreader,
		unsigned int nofThreads,
		const char* logfile) const
{
	unsigned int maxconcepts = maxconcepts_ ? maxconcepts_ : samplear.size() * FEATURE_MAXNOFCONCEPT_RELATION;
	GlobalCountAllocator glbcntalloc( maxconcepts);
	GenGroupParameter genParameter;
	GenGroupContext groupContext( &glbcntalloc, samplear, maxconcepts, m_assignments, logfile);
	GenGroupThreadContext threadContext( &glbcntalloc, &groupContext, &simrelreader, &genParameter, nofThreads);

	genParameter.simdist = m_simdist;
	genParameter.descendants = m_descendants;
	genParameter.mutations = m_mutations;
	genParameter.votes = m_votes;
	genParameter.maxage = m_maxage;
	genParameter.isaf = m_isaf;
	genParameter.eqdiff = m_eqdiff;
	genParameter.with_singletons = m_with_singletons;

	if (groupContext.logout()) groupContext.logout()
		<< string_format( _TXT("start learning concepts for class %s"), clname.c_str());

	// Do the iterations of creating new individuals
	unsigned int iteration=0;
	for (; iteration != m_iterations; ++iteration)
	{
		// Set 'parameter->simdist' as a value that starts with 'simdist' an moves towards 'raddist' with more iterations:
		unsigned int rad_iterations = (m_iterations - (m_iterations >> 2));
		if (iteration >= rad_iterations)
		{
			genParameter.simdist = m_raddist;
		}
		else
		{
			genParameter.simdist = m_raddist + ((m_simdist - m_raddist) * (rad_iterations - iteration - 1) / (rad_iterations - 1));
		}

		if (groupContext.logout()) groupContext.logout() << string_format( _TXT("starting iteration %u"), iteration);

#ifdef STRUS_LOWLEVEL_DEBUG
		checkSimGroupStructures( groupInstanceList, groupInstanceMap, sampleSimGroupMap, samplear.size());
#endif
		if (groupContext.logout()) groupContext.logout() << _TXT("chasing free elements");
		threadContext.run( &GenGroupProcedure_greedyChaseFreeFeatures, samplear.size());

		if (groupContext.logout()) groupContext.logout() << string_format( _TXT("interchanging elements out of %u"), glbcntalloc.nofGroupIdsAllocated());
		threadContext.run( &GenGroupProcedure_greedyNeighbourGroupInterchange, glbcntalloc.nofGroupIdsAllocated());

		if (groupContext.logout()) groupContext.logout() << _TXT("improving fitness of individuals");
		threadContext.run( &GenGroupProcedure_improveGroups, glbcntalloc.nofGroupIdsAllocated());

		if (groupContext.logout()) groupContext.logout() << _TXT("solving neighbours clashes");
		threadContext.run( &GenGroupProcedure_similarNeighbourGroupElimination, glbcntalloc.nofGroupIdsAllocated());
	}/*for (; iteration...*/

	if (groupContext.logout()) groupContext.logout() << _TXT("eliminating redundant (dependent) groups");
	groupContext.eliminateRedundantGroups( genParameter);

	// Build the result:
	if (groupContext.logout()) groupContext.logout() << _TXT("building the result");
	std::vector<SimHash> rt;
	groupContext.collectResults( rt, singletons, sampleConceptIndexMap, conceptSampleIndexMap, genParameter);

	if (groupContext.logout()) groupContext.logout() << string_format( _TXT("done, got %u categories"), rt.size());
	return rt;
}

std::string GenModel::tostring() const
{
	std::ostringstream rt;
	rt	<< "threads=" << m_threads << std::endl
		<< ", maxdist=" << m_maxdist << std::endl
		<< ", simdist=" << m_simdist << std::endl
		<< ", raddist=" << m_raddist << std::endl
		<< ", eqdist=" << m_eqdist << std::endl
		<< ", mutations=" << m_mutations << std::endl
		<< ", votes=" << m_votes << std::endl
		<< ", descendants=" << m_descendants << std::endl
		<< ", maxage=" << m_maxage << std::endl
		<< ", iterations=" << m_iterations << std::endl
		<< ", assignments=" << m_assignments << std::endl
		<< ", isaf=" << m_isaf << std::endl
		<< ", eqdiff=" << m_eqdiff << std::endl
		<< ", singletons=" << (m_with_singletons?"yes":"no") << std::endl;
	return rt.str();
}


