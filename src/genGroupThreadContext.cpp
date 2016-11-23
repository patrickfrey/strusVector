/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Multithreading context for the genetic algorithms
#include "genGroupThreadContext.hpp"
#include "cacheLineSize.hpp"
#include "utils.hpp"
#include <boost/thread.hpp>

using namespace strus;

GenGroupThreadContext::GenGroupThreadContext( GlobalCountAllocator* glbcnt_, GenGroupContext* groupctx_, const SimRelationReader* simrelreader_, const GenGroupParameter* parameter_, unsigned int nofThreads_)
	:m_glbcnt(glbcnt_),m_nofThreads(nofThreads_),m_groupctx(groupctx_),m_simrelreader(simrelreader_),m_allocators(),m_parameter(parameter_)
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
		while (chunksize % CacheLineSize != 0) ++chunksize;

		std::size_t startchunkidx = startidx;
		unsigned int ti=0, te=m_nofThreads;
		for (ti=0; ti<te && startchunkidx < endidx; ++ti,startchunkidx += chunksize)
		{
			std::size_t endchunkidx = startchunkidx + chunksize;
			if (endchunkidx > endidx) endchunkidx = endidx;
	
			tgroup.create_thread( boost::bind( proc, m_parameter, &m_allocators[ti], m_groupctx, m_simrelreader, startchunkidx, endchunkidx));
		}
		tgroup.join_all();
		const char* err = m_groupctx->lastError();
		if (err) throw strus::runtime_error(_TXT("error in genmodel thread: %s"), err);
	}
	else
	{
		proc( m_parameter, &m_allocators[0], m_groupctx, m_simrelreader, startidx, endidx);
	}
}




