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
#include "strus/errorBufferInterface.hpp"
#include <ctime>
#include <cmath>
#include <iostream>
#include <sstream>
#include <limits>
#include <algorithm>

using namespace strus;
#define STRUS_LOWLEVEL_DEBUG

static void GenGroupProcedure_greedyChaseFreeFeatures( const GenGroupParameter* parameter, GlobalCountAllocator* glbcnt, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx, ErrorBufferInterface* errorhnd)
{
	try
	{
		SimGroupIdAllocator groupIdAllocator( glbcnt);
		unsigned int newgroupcnt = 0;
		SampleIndex si = startidx, se = endidx;
		for (; si != se; ++si)
		{
			newgroupcnt += genGroupContext->greedyChaseFreeFeatures( groupIdAllocator, si, *simrelreader, *parameter) ? 1:0;
		}
		genGroupContext->logout().countItems( newgroupcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	catch (const std::logic_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	errorhnd->releaseContext();
}

static void GenGroupProcedure_greedyNeighbourGroupInterchange( const GenGroupParameter* parameter, GlobalCountAllocator* glbcnt, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx, ErrorBufferInterface* errorhnd)
{
	try
	{
		SimGroupIdAllocator groupIdAllocator( glbcnt);
		unsigned int interchangecnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			genGroupContext->greedyNeighbourGroupInterchange( groupIdAllocator, gi, *parameter, interchangecnt);
		}
		genGroupContext->logout().countItems( interchangecnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	catch (const std::logic_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	errorhnd->releaseContext();
}

static void GenGroupProcedure_improveGroups( const GenGroupParameter* parameter, GlobalCountAllocator* glbcnt, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx, ErrorBufferInterface* errorhnd)
{
	try
	{
		SimGroupIdAllocator groupIdAllocator( glbcnt);
		unsigned int mutationcnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			mutationcnt += genGroupContext->improveGroup( groupIdAllocator, gi, *parameter) ? 1:0;
		}
		genGroupContext->logout().countItems( mutationcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	catch (const std::logic_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	errorhnd->releaseContext();
}

static void GenGroupProcedure_similarNeighbourGroupElimination( const GenGroupParameter* parameter, GlobalCountAllocator* glbcnt, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx, ErrorBufferInterface* errorhnd)
{
	try
	{
		SimGroupIdAllocator groupIdAllocator( glbcnt);
		unsigned int eliminationcnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			eliminationcnt += genGroupContext->similarNeighbourGroupElimination( groupIdAllocator, gi, *parameter) ? 1:0;
		}
		genGroupContext->logout().countItems( eliminationcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	catch (const std::logic_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	errorhnd->releaseContext();
}

static void GenGroupProcedure_unfittestGroupElimination( const GenGroupParameter* parameter, GlobalCountAllocator* glbcnt, GenGroupContext* genGroupContext, const SimRelationReader* simrelreader, std::size_t startidx, std::size_t endidx, ErrorBufferInterface* errorhnd)
{
	try
	{
		SimGroupIdAllocator groupIdAllocator( glbcnt);
		unsigned int eliminationcnt = 0;
		ConceptIndex gi = startidx, ge = endidx;
		for (; gi != ge; ++gi)
		{
			eliminationcnt += genGroupContext->unfittestGroupElimination( groupIdAllocator, gi, *parameter) ? 1:0;
		}
		genGroupContext->logout().countItems( eliminationcnt);
	}
	catch (const std::bad_alloc& err)
	{
		genGroupContext->reportError( _TXT("memory allocation error"));
	}
	catch (const std::runtime_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	catch (const std::logic_error& err)
	{
		genGroupContext->reportError( err.what());
	}
	errorhnd->releaseContext();
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
	GenGroupContext groupContext( &glbcntalloc, samplear, maxconcepts, m_assignments, nofThreads, logfile, m_errorhnd);
	GenGroupThreadContext threadContext( &glbcntalloc, &groupContext, &simrelreader, &genParameter, nofThreads, m_errorhnd);

	genParameter.simdist = m_simdist;
	genParameter.eqdist = m_eqdist;
	genParameter.descendants = m_descendants;
	genParameter.mutations = m_mutations;
	genParameter.votes = m_votes;
	genParameter.maxage = m_maxage;
	genParameter.greediness = m_greediness;
	genParameter.isaf = m_isaf;
	genParameter.eqdiff = m_eqdiff;
	genParameter.with_singletons = m_with_singletons;

	if (groupContext.logout()) groupContext.logout()
		<< string_format( _TXT("start learning concepts for class %s"), clname.c_str());

#ifdef STRUS_LOWLEVEL_DEBUG
#define STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG \
		if (groupContext.logout()) groupContext.logout() << _TXT("checking consistency of structures"); \
		{ConceptIndex nofGroups; groupContext.garbageCollectSimGroupIds( nofGroups);} \
		groupContext.checkSimGroupStructures();
#else
#define STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG
#endif

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

		if (groupContext.logout()) groupContext.logout() << _TXT("greedy chase free features");
		threadContext.run( &GenGroupProcedure_greedyChaseFreeFeatures, 0, samplear.size());
		if (groupContext.logout()) groupContext.logout().printAccuLine( _TXT("free feature chase triggered creation of %u new groups"));

		STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG

		groupContext.logout() << _TXT("run cross group assignments from gready chase feature step");
		threadContext.runGroupAssignments();
		if (groupContext.logout()) groupContext.logout().printAccuLine( _TXT("assigned %u features to groups"));

		STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG

		if (groupContext.logout()) groupContext.logout() << _TXT("improving fitness of individuals");
		threadContext.run( &GenGroupProcedure_improveGroups, 1, glbcntalloc.nofGroupIdsAllocated()+1);
		if (groupContext.logout()) groupContext.logout().printAccuLine( _TXT("group improve triggered %u successful mutations"));

		STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG

		if (groupContext.logout()) groupContext.logout() << string_format( _TXT("interchanging elements of %u individuals"), glbcntalloc.nofGroupIdsAllocated());
		threadContext.run( &GenGroupProcedure_greedyNeighbourGroupInterchange, 1, glbcntalloc.nofGroupIdsAllocated()+1);
		if (groupContext.logout()) groupContext.logout().printAccuLine( _TXT("neighbour groups interchanged %u features"));

		STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG

		if (groupContext.logout()) groupContext.logout() << _TXT("improving fitness of individuals");
		threadContext.run( &GenGroupProcedure_improveGroups, 1, glbcntalloc.nofGroupIdsAllocated()+1);
		if (groupContext.logout()) groupContext.logout().printAccuLine( _TXT("group improve triggered %u successful mutations"));

		STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG

		if (groupContext.logout()) groupContext.logout() << _TXT("solving neighbour clashes");
		threadContext.run( &GenGroupProcedure_similarNeighbourGroupElimination, 1, glbcntalloc.nofGroupIdsAllocated()+1);
		if (groupContext.logout()) groupContext.logout().printAccuLine( _TXT("neighbour elimination triggered %u successful removals"));

		STRUS_CHECK_CONSISTENCY_IF_LOWLEVEL_DEBUG

		if (groupContext.logout()) groupContext.logout() << _TXT("improving capacity of group elements");
		threadContext.run( &GenGroupProcedure_unfittestGroupElimination, 1, glbcntalloc.nofGroupIdsAllocated()+1);
		if (groupContext.logout()) groupContext.logout().printAccuLine( _TXT("successful withdrawal of %u group elements from their unfittest group"));

		if (groupContext.logout()) groupContext.logout() << _TXT("garbage collection of groupids leaked");
		ConceptIndex nofGroups;
		groupContext.garbageCollectSimGroupIds( nofGroups);
		if (groupContext.logout()) groupContext.logout() << _TXT("got %u active groups");

#ifdef STRUS_LOWLEVEL_DEBUG
		if (groupContext.logout()) groupContext.logout() << _TXT("checking consistency of structures");
		groupContext.checkSimGroupStructures();
#endif
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
		<< ", greediness=" << m_greediness << std::endl
		<< ", iterations=" << m_iterations << std::endl
		<< ", assignments=" << m_assignments << std::endl
		<< ", isaf=" << m_isaf << std::endl
		<< ", eqdiff=" << m_eqdiff << std::endl
		<< ", singletons=" << (m_with_singletons?"yes":"no") << std::endl;
	return rt.str();
}


