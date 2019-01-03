/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Function for evaluating similarity relations (incl. multithreaded)
#include "getSimhashValues.hpp"
#include "lshModel.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/thread.hpp"
#include "strus/base/atomic.hpp"
#include "strus/reference.hpp"
#include "strus/errorBufferInterface.hpp"
#include <memory>
#include <iostream>
#include "armadillo"

#undef STRUS_LOWLEVEL_DEBUG

using namespace strus;

static std::vector<SimHash> getSimhashValues_singlethread( const LshModel& lshmodel, const std::vector<VectorDef>& vecar)
{
	std::vector<SimHash> rt;
	rt.reserve( vecar.size());
	std::vector<VectorDef>::const_iterator si = vecar.begin(), se = vecar.end();
	for (; si != se; ++si)
	{
		rt.push_back( lshmodel.simHash( arma::normalise( arma::fvec(si->vec())), si->id()));
	}
	return rt;
}


class SimhashBuilderGlobalContext
{
public:
	SimhashBuilderGlobalContext( SimHash* resar_, const VectorDef* vecar_, std::size_t arsize_, std::size_t chunksize_)
		:m_chunkIndex(0),m_resar(resar_),m_vecar(vecar_),m_arsize(arsize_),m_chunksize(chunksize_)
	{}

	bool fetch( SimHash*& chunk_resar, const VectorDef*& chunk_vecar, std::size_t& chunk_arsize)
	{
		unsigned int chunk_index = m_chunkIndex.allocIncrement();
		std::size_t chunk_ofs = chunk_index * m_chunksize;
		if (chunk_ofs >= m_arsize)
		{
			m_chunkIndex.decrement();
			return false;
		}
		chunk_vecar = m_vecar + chunk_ofs;
		chunk_resar = m_resar + chunk_ofs;
		chunk_arsize = m_arsize - chunk_ofs;
		if (chunk_arsize > m_chunksize)
		{
			chunk_arsize = m_chunksize;
		}
#ifdef STRUS_LOWLEVEL_DEBUG
		std::cerr << strus::string_format( "fetch %u %u %u", chunk_index, chunk_ofs, chunk_arsize) << std::endl;
#endif
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

private:
	strus::AtomicCounter<unsigned int> m_chunkIndex;
	SimHash* m_resar;
	const VectorDef* m_vecar;
	std::size_t m_arsize;
	std::size_t m_chunksize;
	strus::mutex m_mutex;
	std::string m_errormsg;
};

class SimhashBuilder
{
public:
	SimhashBuilder( SimhashBuilderGlobalContext* ctx_, const LshModel* lshModel_, unsigned int threadid_, ErrorBufferInterface* errorhnd_)
		:m_errorhnd(errorhnd_)
		,m_ctx(ctx_),m_lshModel(lshModel_)
		,m_terminated(false),m_threadid(threadid_){}
	~SimhashBuilder(){}

	void sigStop()
	{
		m_terminated = true;
	}

	void run()
	{
		try
		{
			SimHash* chunk_resar = 0;
			const VectorDef* chunk_vecar = 0;
			std::size_t chunk_arsize = 0;

			while (!m_terminated && m_ctx->fetch( chunk_resar, chunk_vecar, chunk_arsize))
			{
				std::size_t ai = 0, ae = chunk_arsize;
				for (;ai != ae; ++ai)
				{
					chunk_resar[ ai] = m_lshModel->simHash( arma::normalise( arma::fvec( chunk_vecar[ ai].vec())), chunk_vecar[ ai].id());
				}
			}
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
		m_errorhnd->releaseContext();
	}

private:
	ErrorBufferInterface* m_errorhnd;
	SimhashBuilderGlobalContext* m_ctx;
	const LshModel* m_lshModel;
	bool m_terminated;
	unsigned int m_threadid;
};


std::vector<SimHash> strus::getSimhashValues(
		const LshModel& lshmodel,
		const std::vector<VectorDef>& vecar,
		unsigned int threads,
		ErrorBufferInterface* errorhnd)
{
	if (!threads)
	{
		return getSimhashValues_singlethread( lshmodel, vecar);
	}
	else
	{
		std::vector<SimHash> rt;
		rt.reserve( vecar.size());
		std::size_t si = 0, se = vecar.size();
		for (; si != se; ++si) rt.push_back( SimHash());

		unsigned int chunksize = 16;
		while (chunksize * threads * 5 < se) chunksize *= 2;
		SimhashBuilderGlobalContext context( rt.data(), vecar.data(), se, chunksize);

		std::vector<strus::Reference<SimhashBuilder> > processorList;
		processorList.reserve( threads);
		for (unsigned int ti = 0; ti<threads; ++ti)
		{
			processorList.push_back(
				new SimhashBuilder( &context, &lshmodel, ti+1, errorhnd));
		}
		{
			std::vector<strus::Reference<strus::thread> > threadGroup;
			for (unsigned int ti=0; ti<threads; ++ti)
			{
				SimhashBuilder* ctx = processorList[ti].get();
				strus::Reference<strus::thread> th( new strus::thread( &SimhashBuilder::run, ctx));
				threadGroup.push_back( th);
			}
			std::vector<strus::Reference<strus::thread> >::iterator gi = threadGroup.begin(), ge = threadGroup.end();
			for (; gi != ge; ++gi) (*gi)->join();
		}
		if (context.hasError())
		{
			throw strus::runtime_error("failed to build similarity hash values of vectors: %s", context.error().c_str());
		}
		return rt;
	}
}

