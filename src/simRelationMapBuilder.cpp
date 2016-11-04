/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Functions for probabilistic evaluating similarity relations (incl. multithreaded)
#include "simRelationMapBuilder.hpp"
#include "databaseAdapter.hpp"
#include "simHash.hpp"
#include "lshBench.hpp"
#include "utils.hpp"
#include "strus/base/string_format.hpp"
#include "strus/reference.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include <vector>
#include <algorithm>
#include <limits>
#include <boost/thread.hpp>

using namespace strus;

#define WEIGHTFACTOR(dd) (dd + dd / 2)


SimRelationMapBuilder::SimRelationMapBuilder( const DatabaseInterface* database_, const std::string& databaseConfig_, const std::vector<SimHash>& samplear, unsigned int maxdist_, unsigned int maxsimsam_, unsigned int rndsimsam_, unsigned int threads_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_database(database_),m_databaseConfig(databaseConfig_),m_base(samplear.data()),m_maxdist(maxdist_),m_maxsimsam(maxsimsam_),m_rndsimsam(rndsimsam_),m_threads(threads_),m_index(0),m_benchar(),m_rnd()
{
	m_selectseed = m_rnd.get(0,std::numeric_limits<unsigned int>::max());
	std::size_t ofs = 0;
	while (ofs < samplear.size())
	{
		m_benchar.push_back( LshBench( m_base, samplear.size(), m_selectseed, WEIGHTFACTOR(m_maxdist)));
		ofs += m_benchar.back().init( ofs);
	}
}

SimRelationMap SimRelationMapBuilder::getSimRelationMap( strus::Index idx) const
{
	std::vector<LshBench::Candidate> res;
	std::size_t bi = 0, be = m_benchar.size();
	for (; bi != be; ++bi)
	{
		m_benchar[ idx].findSimCandidates( res, m_benchar[ bi]);
	}
	if (m_database)
	{
		// If database specified, new sim relation map element candidates are merged with previously defined elements
		DatabaseAdapter db( m_database, m_databaseConfig, m_errorhnd);
		strus::Index si = m_benchar[ idx].startIndex(), se = m_benchar[ idx].endIndex();
		for (; si != se; ++si)
		{
			std::vector<SimRelationMap::Element> elems = db.readSimRelations( si);
			std::vector<SimRelationMap::Element>::const_iterator ei = elems.begin(), ee = elems.end();
			for (; ei != ee; ++ei)
			{
				res.push_back( LshBench::Candidate( si, ei->index));
			}
		}
	}
	std::vector<LshBench::Candidate>::const_iterator ri = res.begin(), re = res.end();
	typedef std::map<strus::Index,std::vector<SimRelationMap::Element> > RowMap;
	RowMap rowmap;
	for (; ri != re; ++ri)
	{
		if (m_base[ ri->row].near( m_base[ri->col], m_maxdist))
		{
			unsigned short simdist = m_base[ri->row].dist( m_base[ri->col]);
			rowmap[ ri->row].push_back( SimRelationMap::Element( ri->row, simdist));
		}
	}
	SimRelationMap rt;
	RowMap::iterator mi = rowmap.begin(), me = rowmap.end();
	for (; mi != me; ++mi)
	{
		std::vector<SimRelationMap::Element>& elems = mi->second;
		if ((m_maxsimsam != 0 || m_rndsimsam != 0) && elems.size() > m_maxsimsam + m_rndsimsam)
		{
			elems = SimRelationMap::selectElementSubset( elems, m_maxsimsam, m_rndsimsam, m_selectseed);
		}
		rt.addRow( mi->first, elems);
	}
	return rt;
}


class ThreadGlobalContext
{
public:
	ThreadGlobalContext( SampleIndex idx_begin, SampleIndex idx_end)
		:m_sampleIndex(idx_begin),m_endSampleIndex(idx_end),m_errormsg(),m_simrelmap()
	{}

	bool fetch( SampleIndex& index)
	{
		index = m_sampleIndex.allocIncrement();
		if (index >= (SampleIndex)m_endSampleIndex)
		{
			m_sampleIndex.decrement();
			return false;
		}
		return true;
	}

	void reportError( const std::string& msg)
	{
		utils::ScopedLock lock( m_mutex);
		m_errormsg.append( msg);
		m_errormsg.push_back( '\n');
	}

	bool hasError() const
	{
		return !m_errormsg.empty();
	}

	const std::string& error() const
	{
		return m_errormsg;
	}

	void pushResult( const SimRelationMap& result)
	{
		utils::ScopedLock lock( m_mutex);
		m_simrelmap.join( result);
	}

	const SimRelationMap& result()
	{
		return m_simrelmap;
	}

private:
	utils::AtomicCounter<SampleIndex> m_sampleIndex;
	std::size_t m_endSampleIndex;
	utils::Mutex m_mutex;
	std::string m_errormsg;
	SimRelationMap m_simrelmap;
};


class ThreadLocalContext
{
public:
	ThreadLocalContext( ThreadGlobalContext* ctx_, const SimRelationMapBuilder* builder_, unsigned int threadid_)
		:m_ctx(ctx_),m_builder(builder_),m_threadid(threadid_),m_simrelmap(){}
	~ThreadLocalContext(){}

	void run()
	{
		try
		{
			SampleIndex sidx = 0;
			while (m_ctx->fetch( sidx))
			{
				m_simrelmap.join( m_builder->getSimRelationMap( sidx));
			}
			m_ctx->pushResult( m_simrelmap);
			m_simrelmap.clear();
		}
		catch (const std::runtime_error& err)
		{
			m_ctx->reportError( string_format( _TXT("error in thread %u: %s"), m_threadid, err.what()));
		}
		catch (const std::bad_alloc&)
		{
			m_ctx->reportError( string_format( _TXT("out of memory in thread %u"), m_threadid));
		}
		catch (const boost::thread_interrupted&)
		{
			m_ctx->reportError( string_format( _TXT("failed to complete calculation: thread %u interrupted"), m_threadid));
		}
	}

private:
	ThreadGlobalContext* m_ctx;
	const SimRelationMapBuilder* m_builder;
	unsigned int m_threadid;
	SimRelationMap m_simrelmap;
};


bool SimRelationMapBuilder::getNextSimRelationMap( SimRelationMap& res)
{
	res.clear();
	if (m_index >= m_benchar.size())
	{
		return false;
	}
	if (!m_threads)
	{
		res = getSimRelationMap( m_index++);
		return true;
	}
	else
	{
		unsigned int nt = m_index + m_threads > m_benchar.size() ? (m_benchar.size() - m_index) : m_threads;
		ThreadGlobalContext threadGlobalContext( m_index, nt);

		std::vector<strus::Reference<ThreadLocalContext> > processorList;
		processorList.reserve( nt);
		for (unsigned int ti = 0; ti<nt; ++ti)
		{
			processorList.push_back( new ThreadLocalContext( &threadGlobalContext, this, ti+1));
		}
		{
			boost::thread_group tgroup;
			for (unsigned int ti=0; ti<nt; ++ti)
			{
				tgroup.create_thread( boost::bind( &ThreadLocalContext::run, processorList[ti].get()));
			}
			tgroup.join_all();
		}
		if (threadGlobalContext.hasError())
		{
			throw strus::runtime_error("failed to build similarity relation map: %s", threadGlobalContext.error().c_str());
		}
		res = threadGlobalContext.result();
		return true;
	}
}




