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
#include "strus/base/string_format.hpp"
#include "strus/reference.hpp"
#include <memory>
#include <boost/thread.hpp>

using namespace strus;

static SimRelationMap getSimRelationMap_singlethread( 
		const std::vector<SimHash>& samplear, SampleIndex idx_begin, SampleIndex idx_end,
		unsigned int maxdist)
{
	SimRelationMap rt;
	for (SampleIndex sidx=idx_begin; sidx != idx_end; ++sidx)
	{
		const SimHash& srec = samplear[ sidx];

		std::vector<SimRelationMap::Element> row;

		std::vector<SimHash>::const_iterator pi = samplear.begin(), pe = samplear.end();
		for (SampleIndex pidx=0; pi != pe; ++pi,++pidx)
		{
			if (pidx != sidx && pi->near( srec, maxdist))
			{
				unsigned short dist = pi->dist( srec);
				row.push_back( SimRelationMap::Element( pidx, dist));
			}
		}
		rt.addRow( sidx, row);
	}
	return rt;
}

class RowBuilderGlobalContext
{
public:
	RowBuilderGlobalContext(
			const std::vector<SimHash>* samplear_, SampleIndex idx_begin, SampleIndex idx_end,
			unsigned int maxdist_)
		:m_sampleIndex(idx_begin),m_endSampleIndex(idx_end),m_samplear(samplear_)
		,m_maxdist(maxdist_)
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

	SampleIndex current() const
	{
		return m_sampleIndex.value();
	}

	std::vector<SimRelationMap::Element> getRow( SampleIndex sidx) const
	{
		const SimHash& srec = (*m_samplear)[ sidx];
		std::vector<SimRelationMap::Element> rt;

		std::vector<SimHash>::const_iterator pi = m_samplear->begin(), pe = m_samplear->end();
		for (SampleIndex pidx=0; pi != pe; ++pi,++pidx)
		{
			if (pidx != sidx && pi->near( srec, m_maxdist))
			{
				unsigned short dist = pi->dist( srec);
				rt.push_back( SimRelationMap::Element( pidx, dist));
			}
		}
		return rt;
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
	utils::Mutex m_mutex;
	std::string m_errormsg;
	SimRelationMap m_simrelmap;
};

class RowBuilder
{
public:
	RowBuilder( RowBuilderGlobalContext* ctx_, unsigned int threadid_)
		:m_ctx(ctx_),m_simrelmap(),m_threadid(threadid_){}
	~RowBuilder(){}

	void run()
	{
		try
		{
			SampleIndex sidx = 0;
			while (m_ctx->fetch( sidx))
			{
				m_simrelmap.addRow( sidx, m_ctx->getRow( sidx));
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
	RowBuilderGlobalContext* m_ctx;
	SimRelationMap m_simrelmap;
	unsigned int m_threadid;
};

SimRelationMap strus::getSimRelationMap(
		const std::vector<SimHash>& samplear, SampleIndex idx_begin, SampleIndex idx_end,
		unsigned int maxdist, unsigned int threads)
{
	if (idx_end < idx_begin)
	{
		throw strus::runtime_error(_TXT("illegal range of elements to calculate passed to '%s'"), "getSimRelationMap");
	}
	if (!threads)
	{
		return getSimRelationMap_singlethread( samplear, idx_begin, idx_end, maxdist);
	}
	else
	{
		RowBuilderGlobalContext threadGlobalContext( &samplear, idx_begin, idx_end, maxdist);

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

