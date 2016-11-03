/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function for evaluating similarity relations (incl. multithreaded)
#include "getSimRelationMap.hpp"
#include "simGroup.hpp"
#include "utils.hpp"
#include "random.hpp"
#include "lshBench.hpp"
#include "strus/base/string_format.hpp"
#include "strus/reference.hpp"
#include <memory>
#include <limits>
#include <boost/thread.hpp>

using namespace strus;

static std::vector<SimRelationMap::Element> getRow(
		const std::vector<SimHash>& samplear, SampleIndex sidx,
		unsigned int maxdist, unsigned int maxsimsam, unsigned int rndsimsam, unsigned int rndseed)
{
	const SimHash& srec = samplear[ sidx];
	std::vector<SimRelationMap::Element> rt;

	std::vector<SimHash>::const_iterator pi = samplear.begin(), pe = samplear.end();
	for (SampleIndex pidx=0; pi != pe; ++pi,++pidx)
	{
		if (pidx != sidx && pi->near( srec, maxdist))
		{
			unsigned short dist = pi->dist( srec);
			rt.push_back( SimRelationMap::Element( pidx, dist));
		}
	}
	if ((maxsimsam != 0 || rndsimsam != 0) && rt.size() > maxsimsam + rndsimsam)
	{
		return SimRelationMap::selectElementSubset( rt, maxsimsam, rndsimsam, rndseed);
	}
	else
	{
		return rt;
	}
}

static SimRelationMap getSimRelationMap_singlethread( 
		const std::vector<SimHash>& samplear, SampleIndex idx_begin, SampleIndex idx_end,
		unsigned int maxdist, unsigned int maxsimsam, unsigned int rndsimsam)
{
	SimRelationMap rt;
	Random rnd;
	for (SampleIndex sidx=idx_begin; sidx != idx_end; ++sidx)
	{
		unsigned int rndseed = rnd.get( 0, std::numeric_limits<int>::max());
		rt.addRow( sidx, ::getRow( samplear, sidx, maxdist, maxsimsam, rndsimsam, rndseed));
	}
	return rt;
}

class RowBuilderGlobalContext
{
public:
	RowBuilderGlobalContext(
			const std::vector<SimHash>* samplear_, SampleIndex idx_begin, SampleIndex idx_end,
			unsigned int maxdist_, unsigned int maxsimsam_, unsigned int rndsimsam_)
		:m_sampleIndex(idx_begin),m_endSampleIndex(idx_end),m_samplear(samplear_)
		,m_maxdist(maxdist_),m_maxsimsam(maxsimsam_),m_rndsimsam(rndsimsam_)
	{
		Random rnd;
		m_selectseed = rnd.get( 0, std::numeric_limits<int>::max());
	}

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

	SampleIndex current() const
	{
		return m_sampleIndex.value();
	}

	std::vector<SimRelationMap::Element> getRow( SampleIndex sidx) const
	{
		return ::getRow( *m_samplear, sidx, m_maxdist, m_maxsimsam, m_rndsimsam, m_selectseed);
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
	const std::vector<SimHash>* m_samplear;
	unsigned int m_maxdist;
	unsigned int m_maxsimsam;
	unsigned int m_rndsimsam;
	utils::Mutex m_mutex;
	std::string m_errormsg;
	SimRelationMap m_simrelmap;
	unsigned int m_selectseed;
};

class RowBuilder
{
public:
	RowBuilder( RowBuilderGlobalContext* ctx_, unsigned int threadid_)
		:m_ctx(ctx_),m_threadid(threadid_){}
	~RowBuilder(){}

	void run()
	{
		try
		{
			SimRelationMap simrelmap;
			SampleIndex sidx = 0;
			while (m_ctx->fetch( sidx))
			{
				simrelmap.addRow( sidx, m_ctx->getRow( sidx));
			}
			m_ctx->pushResult( simrelmap);
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
	RowBuilderGlobalContext* m_ctx;
	Random m_rnd;
	unsigned int m_threadid;
};


SimRelationMap strus::getSimRelationMap(
		const std::vector<SimHash>& samplear, SampleIndex idx_begin, SampleIndex idx_end,
		unsigned int maxdist, unsigned int maxsimsam, unsigned int rndsimsam, unsigned int threads)
{
	if (idx_end < idx_begin)
	{
		throw strus::runtime_error(_TXT("illegal range of elements to calculate passed to '%s'"), "getSimRelationMap");
	}
	if (!threads)
	{
		return getSimRelationMap_singlethread( samplear, idx_begin, idx_end, maxdist, maxsimsam, rndsimsam);
	}
	else
	{
		RowBuilderGlobalContext threadGlobalContext( &samplear, idx_begin, idx_end, maxdist, maxsimsam, rndsimsam);

		std::vector<strus::Reference<RowBuilder> > processorList;
		processorList.reserve( threads);
		for (unsigned int ti = 0; ti<threads; ++ti)
		{
			processorList.push_back( new RowBuilder( &threadGlobalContext, ti+1));
		}
		{
			boost::thread_group tgroup;
			for (unsigned int ti=0; ti<threads; ++ti)
			{
				tgroup.create_thread( boost::bind( &RowBuilder::run, processorList[ti].get()));
			}
			tgroup.join_all();
		}
		if (threadGlobalContext.hasError())
		{
			throw strus::runtime_error("failed to build similarity relation map: %s", threadGlobalContext.error().c_str());
		}
		return threadGlobalContext.result();
	}
}

