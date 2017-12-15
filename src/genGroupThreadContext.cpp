/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Multithreading context for the genetic algorithms
#include "genGroupThreadContext.hpp"
#include "strus/errorBufferInterface.hpp"
#include <algorithm>

using namespace strus;

GenGroupThreadContext::GenGroupThreadContext( GlobalCountAllocator* glbcnt_, GenGroupContext* groupctx_, const SimRelationReader* simrelreader_, const GenGroupParameter* parameter_, unsigned int nofThreads_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_)
	,m_glbcnt(glbcnt_),m_nofThreads(nofThreads_)
	,m_groupctx(groupctx_),m_simrelreader(simrelreader_)
	,m_parameter(parameter_)
{}

void GenGroupThreadContext::run( GenGroupProcedure proc, std::size_t startidx, std::size_t endidx)
{
	if (m_nofThreads && (endidx - startidx > STRUS_CACHELINE_SIZE))
	{
		boost::thread_group tgroup;
		std::size_t chunksize = (endidx - startidx + m_nofThreads - 1) / m_nofThreads;
		chunksize = ((chunksize + STRUS_CACHELINE_SIZE -1) / STRUS_CACHELINE_SIZE) * STRUS_CACHELINE_SIZE;

		std::size_t startchunkidx = startidx;
		unsigned int ti=0, te=m_nofThreads;
		for (ti=0; ti<te && startchunkidx < endidx; ++ti,startchunkidx += chunksize)
		{
			std::size_t endchunkidx = startchunkidx + chunksize;
			if (endchunkidx > endidx) endchunkidx = endidx;

			tgroup.create_thread( boost::bind( proc, m_parameter, m_glbcnt, m_groupctx, m_simrelreader, startchunkidx, endchunkidx, m_errorhnd));
		}
		tgroup.join_all();
	}
	else
	{
		proc( m_parameter, m_glbcnt, m_groupctx, m_simrelreader, startidx, endidx, m_errorhnd);
	}
	const char* err = m_groupctx->lastError();
	if (err)
	{
		std::string errstr = m_groupctx->fetchError();
		throw strus::runtime_error(_TXT("error in genmodel thread: %s"), errstr.c_str());
	}
}

void GenGroupThreadContext::runGroupAssignments()
{
	std::vector<SampleSimGroupAssignment> assignments = m_groupctx->fetchGroupAssignments();
	std::sort( assignments.begin(), assignments.end());
	std::vector<SampleSimGroupAssignment>::const_iterator ai = assignments.begin(), ae = assignments.end();
	if (m_nofThreads)
	{
		boost::thread_group tgroup;
		std::size_t endidx = m_glbcnt->nofGroupIdsAllocated()+1;
		std::size_t startidx = 1;
		std::size_t chunksize = (endidx - startidx + (m_nofThreads - 1)) / m_nofThreads;
		chunksize = ((chunksize + STRUS_CACHELINE_SIZE -1) / STRUS_CACHELINE_SIZE) * STRUS_CACHELINE_SIZE;

		std::size_t startchunkidx = startidx;
		unsigned int ti=0, te=m_nofThreads;
		for (ti=0; ti<te && startchunkidx < endidx; ++ti,startchunkidx += chunksize)
		{
			std::size_t endchunkidx = startchunkidx + chunksize;
			std::vector<SampleSimGroupAssignment>::const_iterator starti = ai;
			for (; ai < ae && (std::size_t)ai->conceptIndex < endchunkidx; ++ai){}

			tgroup.create_thread( boost::bind( &GenGroupContext::tryGroupAssignments, m_groupctx, starti, ai, *m_parameter));
		}
		tgroup.join_all();
	}
	else
	{
		m_groupctx->tryGroupAssignments( ai, ae, *m_parameter);
	}
	const char* err = m_groupctx->lastError();
	if (err)
	{
		std::string errstr = m_groupctx->fetchError();
		throw strus::runtime_error(_TXT("error in genmodel thread: %s"), errstr.c_str());
	}
}





