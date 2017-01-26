/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage search interface
#include "vectorStorageSearch.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "errorUtils.hpp"
#include "internationalization.hpp"
#include <boost/make_shared.hpp>

#define MODULENAME   "standard vector storage"
#undef STRUS_LOWLEVEL_DEBUG

using namespace strus;

VectorStorageSearch::VectorStorageSearch( const Reference<DatabaseAdapter>& database, const VectorStorageConfig& config_, const Index& range_from_, const Index& range_to_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_)
	,m_config(config_)
	,m_lshmodel(database->readLshModel())
	,m_samplear(database->readSampleSimhashVector(range_from_,range_to_),1/*prob select random seed*/)
	,m_database(m_config.with_realvecweights?database:Reference<DatabaseAdapter>())
	,m_range_from(range_from_)
	,m_range_to(range_to_)
{}

typedef VectorStorageSearchInterface::Result Result;

std::vector<Result> VectorStorageSearch::findSimilar( const std::vector<double>& vec, unsigned int maxNofResults) const
{
	try
	{
		if (m_config.with_realvecweights && m_database.get())
		{
			std::vector<Result> rt;
			rt.reserve( maxNofResults * 2 + 10);

			arma::vec vv = arma::vec( vec);
			SimHash hash( m_lshmodel.simHash( arma::normalise( vv)));
			if (m_config.gencfg.probdist)
			{
				rt = m_samplear.findSimilar( hash, m_config.maxdist, m_config.maxdist * m_config.gencfg.probdist / m_config.gencfg.simdist, maxNofResults * 2 + 10, m_range_from);
			}
			else
			{
				rt = m_samplear.findSimilar( hash, m_config.maxdist, maxNofResults * 2 + 10, m_range_from);
			}
			std::vector<Result>::iterator ri = rt.begin(), re = rt.end();
			for (; ri != re; ++ri)
			{
				arma::vec resvv( m_database->readSampleVector( ri->featidx()));
				ri->setWeight( arma::norm_dot( vv, resvv));
			}
			std::sort( rt.begin(), rt.end(), std::greater<strus::VectorStorageSearchInterface::Result>());
			rt.resize( maxNofResults);
			return rt;
		}
		else
		{
			SimHash hash( m_lshmodel.simHash( arma::normalise( arma::vec( vec))));
			if (m_config.gencfg.probdist)
			{
				return m_samplear.findSimilar( hash, m_config.maxdist, m_config.maxdist * m_config.gencfg.probdist / m_config.gencfg.simdist, maxNofResults, m_range_from);
			}
			else
			{
				return m_samplear.findSimilar( hash, m_config.maxdist, maxNofResults, m_range_from);
			}
		}
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in similar vector search of '%s': %s"), MODULENAME, *m_errorhnd, std::vector<Result>());
}

std::vector<Result> VectorStorageSearch::findSimilarFromSelection( const std::vector<Index>& candidates, const std::vector<double>& vec, unsigned int maxNofResults) const
{
	try
	{
		std::vector<Result> rt;
		arma::vec vv = arma::vec( vec);
		SimHash hash( m_lshmodel.simHash( arma::normalise( vv)));

		if (m_config.with_realvecweights && m_database.get())
		{
			rt.reserve( maxNofResults * 2 + 10);
			rt = m_samplear.findSimilarFromSelection( candidates, hash, m_config.maxdist, maxNofResults * 2 + 10, m_range_from);
		}
		else
		{
			rt.reserve( maxNofResults);
			rt = m_samplear.findSimilarFromSelection( candidates, hash, m_config.maxdist, maxNofResults, m_range_from);
		}
		if (m_config.with_realvecweights && m_database.get())
		{
			std::vector<Result>::iterator ri = rt.begin(), re = rt.end();
			for (; ri != re; ++ri)
			{
				arma::vec resvv( m_database->readSampleVector( ri->featidx()));
				ri->setWeight( arma::norm_dot( vv, resvv));
			}
			std::sort( rt.begin(), rt.end(), std::greater<strus::VectorStorageSearchInterface::Result>());
			rt.resize( maxNofResults);
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in similar vector search from selection of '%s': %s"), MODULENAME, *m_errorhnd, std::vector<Result>());
}

void VectorStorageSearch::close()
{
	m_database.reset();
}


