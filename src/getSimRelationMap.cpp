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
#include "logger.hpp"
#include "thread.hpp"
#include "strus/base/string_format.hpp"
#include <memory>

using namespace strus;

static SimRelationMap getSimRelationMap_singlethread( const std::vector<SimHash>& samplear, unsigned int simdist, const char* logfile)
{
	Logger logout( logfile);
	SimRelationMap rt;
	if (logout) logout << _TXT("calculate similarity relation map");
	std::vector<SimHash>::const_iterator si = samplear.begin(), se = samplear.end();
	for (SampleIndex sidx=0; si != se; ++si,++sidx)
	{
		if (logout && sidx % 10000 == 0) logout << string_format( _TXT("processed %u lines with %u similarities"), sidx, rt.nofRelationsDetected());
		std::vector<SimRelationMap::Element> row;

		std::vector<SimHash>::const_iterator pi = samplear.begin();
		for (SampleIndex pidx=0; pi != si; ++pi,++pidx)
		{
			if (pidx != sidx && si->near( *pi, simdist))
			{
				unsigned short dist = si->dist( *pi);
				row.push_back( SimRelationMap::Element( pidx, dist));
			}
		}
		rt.addRow( sidx, row);
	}
	if (logout) logout << string_format( _TXT("got %u with %u similarities"), samplear.size(), rt.nofRelationsDetected());
	return rt.mirror();
}

class RowBuilderGlobalContext
{
public:
	explicit RowBuilderGlobalContext( const std::vector<SimHash>* samplear_, unsigned int simdist_, const char* logfile)
		:m_sampleIndex(0),m_samplear(samplear_),m_simdist(simdist_),m_logout(logfile)
	{
		if (m_logout) m_logout << _TXT("build similarity relation map (multithreaded)");
	}

	bool fetch( SampleIndex& index)
	{
		index = m_sampleIndex.allocIncrement();
		if (index >= (SampleIndex)m_samplear->size())
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

		std::vector<SimHash>::const_iterator pi = m_samplear->begin();
		for (SampleIndex pidx=0; pidx != sidx; ++pi,++pidx)
		{
			if (pi->near( srec, m_simdist))
			{
				unsigned short dist = pi->dist( srec);
				rt.push_back( SimRelationMap::Element( pidx, dist));
			}
		}
		return rt;
	}

	void logmsg( const std::string& msg)
	{
		utils::ScopedLock lock( m_mutex);
		m_logout << msg;
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
	const std::vector<SimHash>* m_samplear;
	unsigned int m_simdist;
	Logger m_logout;
	utils::Mutex m_mutex;
	std::string m_errormsg;
	SimRelationMap m_simrelmap;
};

class RowBuilder
{
public:
	RowBuilder( RowBuilderGlobalContext* ctx_, unsigned int threadid_)
		:m_ctx(ctx_),m_simrelmap(),m_terminated(false),m_threadid(threadid_){}
	~RowBuilder(){}

	void sigStop()
	{
		m_terminated = true;
	}

	void run()
	{
		try
		{
			SampleIndex sidx;
			SampleIndex cnt = 0;
			while (!m_terminated && m_ctx->fetch(sidx))
			{
				++cnt;
				m_simrelmap.addRow( sidx, m_ctx->getRow( sidx));
				if (cnt % 10000 == 0) m_ctx->logmsg( string_format( _TXT("processed %u lines"), m_ctx->current()));
			}
			m_ctx->logmsg( string_format( _TXT("processed %u lines"), m_ctx->current()));
			m_ctx->pushResult( m_simrelmap);
		}
		catch (const std::runtime_error& err)
		{
			m_ctx->logmsg( string_format( _TXT("error in thread %u: %s"), m_threadid, err.what()));
			m_ctx->reportError( string_format( _TXT("error in thread %u: %s"), m_threadid, err.what()));
		}
		catch (const std::bad_alloc&)
		{
			m_ctx->logmsg( string_format( _TXT("out of memory in thread %u"), m_threadid));
			m_ctx->reportError( string_format( _TXT("out of memory in thread %u"), m_threadid));
		}
	}

private:
	RowBuilderGlobalContext* m_ctx;
	SimRelationMap m_simrelmap;
	bool m_terminated;
	unsigned int m_threadid;
};

SimRelationMap strus::getSimRelationMap( const std::vector<SimHash>& samplear, unsigned int simdist, const char* logfile, unsigned int threads)
{
	if (!threads)
	{
		return getSimRelationMap_singlethread( samplear, simdist, logfile);
	}
	else
	{
		RowBuilderGlobalContext threadGlobalContext( &samplear, simdist, logfile);
		std::auto_ptr< ThreadGroup< RowBuilder > >
			rowBuilderThreads(
				new ThreadGroup<RowBuilder>( "simrelbuilder"));

		for (unsigned int ti = 0; ti<threads; ++ti)
		{
			rowBuilderThreads->start( new RowBuilder( &threadGlobalContext, ti+1));
		}
		rowBuilderThreads->wait_termination();
		if (threadGlobalContext.hasError())
		{
			throw strus::runtime_error("failed to build similarity relation map: %s", threadGlobalContext.error().c_str());
		}
		return threadGlobalContext.result().mirror();
	}
}

