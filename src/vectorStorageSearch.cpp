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
#include "armautils.hpp"

#define MODULENAME   "vector storage"
#undef STRUS_LOWLEVEL_DEBUG

using namespace strus;

VectorStorageSearch::VectorStorageSearch( const Reference<DatabaseAdapter>& database_, const LshModel& model_, const std::string& type, int indexPart, int nofParts, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_)
	,m_lshmodel(model_)
	,m_simhashar()
	,m_typeno(database_->readTypeno( type))
	,m_database(database_)
{
	if (m_typeno)
	{
		if (indexPart == 0 && nofParts == 0)
		{
			m_simhashar = SimHashMap( database_->readSimHashVector( m_typeno), m_lshmodel.vecdim());
		}
		else if (indexPart >= nofParts || nofParts <= 0)
		{
			throw std::runtime_error(_TXT("logic error: incorrect arguments for index part in vector storage search constructor"));
		}
		else
		{
			int nofVectors = database_->readNofVectors( m_typeno);
			int maxPartSize = (nofVectors + nofParts - 1) / nofParts;
			Index featnoStart = database_->readFeatnoStart( m_typeno, indexPart * maxPartSize);
			m_simhashar = SimHashMap( database_->readSimHashVector( m_typeno, featnoStart, maxPartSize), m_lshmodel.vecdim());
		}
	}
}

VectorStorageSearch::~VectorStorageSearch()
{}

std::vector<VectorQueryResult> VectorStorageSearch::findSimilar( const WordVector& vec, int maxNofResults, double minSimilarity, bool realVecWeights) const
{
	try
	{
		std::vector<VectorQueryResult> rt;
		rt.reserve( maxNofResults);
		std::vector<SimHashMap::QueryResult> res;

		if (realVecWeights)
		{
			res.reserve( maxNofResults * 2 + 10);

			arma::fvec vv = arma::fvec( vec);
			SimHash hash( m_lshmodel.simHash( strus::normalizeVector( vec), 0));
			if (m_lshmodel.probsimdist())
			{
				res = m_simhashar.findSimilar( hash, m_lshmodel.simdist(), m_lshmodel.probsimdist(), maxNofResults * 2 + 10);
			}
			else
			{
				res = m_simhashar.findSimilar( hash, m_lshmodel.simdist(), maxNofResults * 2 + 10);
			}
			std::vector<SimHashMap::QueryResult>::iterator ri = res.begin(), re = res.end();
			for (; ri != re; ++ri)
			{
				arma::fvec resvv( m_database->readVector( m_typeno, ri->featno()));
				ri->setWeight( arma::norm_dot( vv, resvv));
			}
			std::sort( res.begin(), res.end(), std::greater<SimHashMap::QueryResult>());
		}
		else
		{
			SimHash hash( m_lshmodel.simHash( strus::normalizeVector( vec), 0));
			if (m_lshmodel.probsimdist())
			{
				res = m_simhashar.findSimilar( hash, m_lshmodel.simdist(), m_lshmodel.probsimdist(), maxNofResults);
			}
			else
			{
				res = m_simhashar.findSimilar( hash, m_lshmodel.simdist(), maxNofResults);
			}
		}
		std::vector<SimHashMap::QueryResult>::const_iterator ri = res.begin(), re = res.end();
		for (int ridx=0; ri != re && ridx < maxNofResults; ++ri,++ridx)
		{
			rt.push_back( VectorQueryResult( m_database->readFeatName( ri->featno()), ri->weight()));
		}
		if (m_errorhnd->hasError())
		{
			throw strus::runtime_error(_TXT("vector search failed: %s"), m_errorhnd->fetchError());
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in similar vector search of '%s': %s"), MODULENAME, *m_errorhnd, std::vector<VectorQueryResult>());
}

void VectorStorageSearch::close()
{
	m_database->close();
}


