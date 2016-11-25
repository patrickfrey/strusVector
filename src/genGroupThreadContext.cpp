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
#include "cacheLineSize.hpp"
#include "utils.hpp"
#include <boost/thread.hpp>

using namespace strus;

GenGroupThreadContext::GenGroupThreadContext( GlobalCountAllocator* glbcnt_, GenGroupContext* groupctx_, const SimRelationReader* simrelreader_, const GenGroupParameter* parameter_, unsigned int nofThreads_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_)
	,m_glbcnt(glbcnt_),m_nofThreads(nofThreads_)
	,m_groupctx(groupctx_),m_simrelreader(simrelreader_)
	,m_allocators(),m_parameter(parameter_)
{
	unsigned int ti=0, te=m_nofThreads?m_nofThreads:1;
	for (; ti != te; ++ti)
	{
		m_allocators.push_back( SimGroupIdAllocator( m_glbcnt));
	}
}

void GenGroupThreadContext::run( GenGroupProcedure proc, std::size_t startidx, std::size_t endidx)
{
	if (m_nofThreads && (endidx - startidx > CacheLineSize))
	{
		boost::thread_group tgroup;
		std::size_t chunksize = (endidx - startidx + m_nofThreads - 1) / m_nofThreads;
		chunksize = ((chunksize + CacheLineSize -1) / CacheLineSize) * CacheLineSize;
		std::size_t group_chunksize = (m_glbcnt->nofGroupIdsAllocated() + m_nofThreads - 1) / m_nofThreads;
		group_chunksize = ((group_chunksize + CacheLineSize -1) / CacheLineSize) * CacheLineSize;
		m_groupAssignQueue.setChunkSize( group_chunksize);

		std::size_t startchunkidx = startidx;
		unsigned int ti=0, te=m_nofThreads;
		for (ti=0; ti<te && startchunkidx < endidx; ++ti,startchunkidx += chunksize)
		{
			std::size_t endchunkidx = startchunkidx + chunksize;
			if (endchunkidx > endidx) endchunkidx = endidx;

			tgroup.create_thread( boost::bind( proc, m_parameter, &m_allocators[ti], m_groupctx, m_simrelreader, startchunkidx, endchunkidx, m_errorhnd));
		}
		tgroup.join_all();
	}
	else
	{
		proc( m_parameter, &m_allocators[0], m_groupctx, m_simrelreader, startidx, endidx, m_errorhnd);
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
	if (m_nofThreads)
	{
		boost::thread_group tgroup;
		unsigned int ti=0, te=m_nofThreads;
		for (ti=0; ti<te; ++ti)
		{
			tgroup.create_thread( boost::bind( &GenGroupContext::tryGroupAssignments, m_groupctx, ti, *m_parameter));
		}
		tgroup.join_all();
	}
	else
	{
		m_groupctx->tryGroupAssignments( 0, *m_parameter);
	}
	m_groupAssignQueue.clear();
	const char* err = m_groupctx->lastError();
	if (err)
	{
		std::string errstr = m_groupctx->fetchError();
		throw strus::runtime_error(_TXT("error in genmodel thread: %s"), errstr.c_str());
	}
}





