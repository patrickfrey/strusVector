/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Functions for probabilistic evaluating similarity relations (incl. multithreaded)
#include "simRelationMapBuilder.hpp"
#include "simHash.hpp"
#include "lshBench.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/atomic.hpp"
#include "strus/base/thread.hpp"
#include "strus/reference.hpp"
#include <vector>
#include <algorithm>
#include <limits>

using namespace strus;

#define WEIGHTFACTOR(dd) (dd + dd / 3)


SimRelationMapBuilder::SimRelationMapBuilder( const std::vector<SimHash>& samplear, unsigned int maxdist_, unsigned int maxsimsam_, unsigned int rndsimsam_, unsigned int threads_, bool probabilistic_, Logger& logout, const SimRelationReader* simmapreader_)
	:m_base(samplear.data()),m_probabilistic(probabilistic_),m_maxdist(maxdist_),m_maxsimsam(maxsimsam_),m_rndsimsam(rndsimsam_),m_threads(threads_),m_index(0),m_benchar(),m_rnd(),m_simmapreader(simmapreader_)
{
	m_selectseed = m_rnd.get(0,std::numeric_limits<unsigned int>::max());
	std::size_t ofs = 0;
	unsigned int oi = 0;
	while (ofs < samplear.size())
	{
		m_benchar.push_back( LshBench( m_base, samplear.size(), m_selectseed, WEIGHTFACTOR(m_maxdist)));
		ofs += m_benchar.back().init( ofs);
		++oi;
	}
	if (!m_benchar.empty())
	{
		if (logout) logout << strus::string_format( _TXT("got %u samples in %u benches with maxdiff %u/64"), (unsigned int)ofs, oi, m_benchar.back().maxDiff());
	}
}

SimRelationMap SimRelationMapBuilder::getSimRelationMap( strus::Index idx) const
{
	SimRelationMap rt;
	if (m_probabilistic)
	{
		typedef std::map<strus::Index,std::vector<SimRelationMap::Element> > RowMap;
		RowMap rowmap;
		std::vector<LshBench::Candidate> res;
		res.reserve( LshBench::ReserveMemSize);
		unsigned int ridx = 0;
		std::size_t bi = 0, be = m_benchar.size();
		for (; bi != be; ++bi)
		{
			m_benchar[ idx].findSimCandidates( res, m_benchar[ bi]);
			std::vector<LshBench::Candidate>::const_iterator ri = res.begin(), re = res.end();
			for (; ri != re; ++ri)
			{
				if (m_base[ ri->row].near( m_base[ ri->col], m_maxdist))
				{
					unsigned short simdist = m_base[ ri->row].dist( m_base[ ri->col]);
					rowmap[ ri->row].push_back( SimRelationMap::Element( ri->col, simdist));
					++ridx;
				}
			}
			res.resize(0);
		}
		RowMap::iterator mi = rowmap.begin(), me = rowmap.end();
		for (; mi != me; ++mi)
		{
			std::vector<SimRelationMap::Element>& elems = mi->second;
			if (m_simmapreader)
			{
				std::vector<SimRelationMap::Element> prvelems = m_simmapreader->readSimRelations( mi->first);
				std::vector<SimRelationMap::Element> mrgelems = SimRelationMap::mergeElementLists( elems, prvelems);
				if ((m_maxsimsam != 0 || m_rndsimsam != 0) && elems.size() > m_maxsimsam + m_rndsimsam)
				{
					std::vector<SimRelationMap::Element>
						selelems = SimRelationMap::selectElementSubset( mrgelems, m_maxsimsam, m_rndsimsam, m_selectseed);
					rt.addRow( mi->first, selelems);
				}
				else
				{
					rt.addRow( mi->first, mrgelems);
				}
			}
			else if ((m_maxsimsam != 0 || m_rndsimsam != 0) && elems.size() > m_maxsimsam + m_rndsimsam)
			{
				std::vector<SimRelationMap::Element> selelems = SimRelationMap::selectElementSubset( elems, m_maxsimsam, m_rndsimsam, m_selectseed);
				rt.addRow( mi->first, selelems);
			}
			else
			{
				rt.addRow( mi->first, elems);
			}
		}
	}
	else
	{
		strus::Index si = m_benchar[ idx].startIndex(), se = m_benchar[ idx].endIndex();
		for (; si < se; ++si)
		{
			std::vector<SimRelationMap::Element> elems;
			strus::Index oi = 0, oe = m_benchar.back().endIndex();
			for (; oi < oe; ++oi)
			{
				if (si != oi && m_base[ si].near( m_base[oi], m_maxdist))
				{
					unsigned short simdist = m_base[si].dist( m_base[oi]);
					elems.push_back( SimRelationMap::Element( oi, simdist));
				}
			}
			rt.addRow( si, elems);
		}
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
		strus::scoped_lock lock( m_mutex);
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

	void pushResult( const SimRelationMap& result_)
	{
		strus::scoped_lock lock( m_mutex);
		m_simrelmap.join( result_);
	}

	const SimRelationMap& result()
	{
		return m_simrelmap;
	}

private:
	strus::AtomicCounter<SampleIndex> m_sampleIndex;
	std::size_t m_endSampleIndex;
	strus::mutex m_mutex;
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
		catch (...)
		{
			m_ctx->reportError( string_format( _TXT("failed to complete calculation: uncaught exception in thread %u"), m_threadid));
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
		unsigned int next_index = m_index + m_threads;
		if (next_index > m_benchar.size()) next_index = m_benchar.size();
		ThreadGlobalContext threadGlobalContext( m_index, next_index);

		std::vector<strus::Reference<ThreadLocalContext> > processorList;
		unsigned int ti = 0, te = next_index - m_index;
		processorList.reserve( te);
		for (; ti<te; ++ti)
		{
			processorList.push_back( new ThreadLocalContext( &threadGlobalContext, this, ti+1));
		}
		{
			std::vector<strus::Reference<strus::thread> > threadGroup;
			for (ti=0; ti<te; ++ti)
			{
				ThreadLocalContext* tc = processorList[ ti].get();
				strus::Reference<strus::thread> th( new strus::thread( &ThreadLocalContext::run, tc));
				threadGroup.push_back( th);
			}
			std::vector<strus::Reference<strus::thread> >::iterator gi = threadGroup.begin(), ge = threadGroup.end();
			for (; gi != ge; ++gi) (*gi)->join();
		}
		if (threadGlobalContext.hasError())
		{
			throw strus::runtime_error("failed to build similarity relation map: %s", threadGlobalContext.error().c_str());
		}
		res = threadGlobalContext.result();
		m_index = next_index;
		return true;
	}
}




