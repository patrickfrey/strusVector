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
#define STRUS_LOWLEVEL_DEBUG

static void GenGroupProcedure_greedyChaseFreeFeatures( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	try
	{
		unsigned int newgroupcnt = 0;
		SampleIndex si = startidx, se = endidx;
		for (; si != se; ++si)
		{
			newgroupcnt += genGroupContext->greedyChaseFreeFeatures( *groupIdAllocator, si, *simrelreader, *parameter) ? 1:0;
		}
		if (newgroupcnt && genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("free feature chase step triggered creation of %u new groups"), newgroupcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
}

static void GenGroupProcedure_greedyNeighbourGroupInterchange( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	try
	{
		unsigned int interchangecnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			genGroupContext->greedyNeighbourGroupInterchange( *groupIdAllocator, gi, *parameter, interchangecnt);
		}
		if (interchangecnt && genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("neighbour groups interchanged %u features"), interchangecnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
}

static void GenGroupProcedure_improveGroups( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	try
	{
		unsigned int mutationcnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			mutationcnt += genGroupContext->improveGroup( *groupIdAllocator, gi, *parameter) ? 1:0;
		}
		if (mutationcnt && genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("group improve step triggered %u successful mutations"), mutationcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
}

static void GenGroupProcedure_similarNeighbourGroupElimination( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	try
	{
		unsigned int eliminationcnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			eliminationcnt += genGroupContext->similarNeighbourGroupElimination( *groupIdAllocator, gi, *parameter) ? 1:0;
		}
		if (eliminationcnt && genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("neighbour elimination step triggered %u successful removals"), eliminationcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
}

static void GenGroupProcedure_unfittestGroupElimination( const GenGroupParameter* parameter, SimGroupIdAllocator* groupIdAllocator, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx)
{
	try
	{
		unsigned int eliminationcnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			eliminationcnt += genGroupContext->unfittestGroupElimination( *groupIdAllocator, gi, *parameter) ? 1:0;
		}
		if (eliminationcnt && genGroupContext->logout()) genGroupContext->logout() << string_format( _TXT("unfittest group elimination step triggered %u successful removals"), eliminationcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
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
	GenGroupContext groupContext( &glbcntalloc, samplear, maxconcepts, m_assignments, nofThreads, logfile);
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
		groupContext.checkSimGroupStructures();
#endif
		if (groupContext.logout()) groupContext.logout() << _TXT("chasing free elements");
		threadContext.run( &GenGroupProcedure_greedyChaseFreeFeatures, 0, samplear.size());

		groupContext.logout() << _TXT("run group interchange assignments");
		threadContext.runGroupAssignments();

		if (groupContext.logout()) groupContext.logout() << _TXT("improving fitness of individuals");
		threadContext.run( &GenGroupProcedure_improveGroups, 1, glbcntalloc.nofGroupIdsAllocated()+1);

		if (groupContext.logout()) groupContext.logout() << string_format( _TXT("interchanging elements out of %u"), glbcntalloc.nofGroupIdsAllocated());
		threadContext.run( &GenGroupProcedure_greedyNeighbourGroupInterchange, 1, glbcntalloc.nofGroupIdsAllocated()+1);

		if (groupContext.logout()) groupContext.logout() << _TXT("improving fitness of individuals");
		threadContext.run( &GenGroupProcedure_improveGroups, 1, glbcntalloc.nofGroupIdsAllocated()+1);

		if (groupContext.logout()) groupContext.logout() << _TXT("solving neighbour clashes");
		threadContext.run( &GenGroupProcedure_similarNeighbourGroupElimination, 1, glbcntalloc.nofGroupIdsAllocated()+1);

		if (groupContext.logout()) groupContext.logout() << _TXT("improving capacity");
		threadContext.run( &GenGroupProcedure_unfittestGroupElimination, 1, glbcntalloc.nofGroupIdsAllocated()+1);
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


