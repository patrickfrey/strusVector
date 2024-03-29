/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for retrieval of the most similar LSH values
#ifndef _STRUS_VECTOR_SIMHASH_MAP_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_MAP_HPP_INCLUDED
#include "strus/storage/index.hpp"
#include "strus/reference.hpp"
#include "simHashFilter.hpp"
#include "simHashReader.hpp"
#include "simHashQueryResult.hpp"
#include "simHashReader.hpp"
#include <utility>
#include <vector>

namespace strus {

/// \brief Structure for retrieval of the most similar LSH values
class SimHashMap
{
public:
	enum {MaxNofBenches=4};
	struct Stats
		:public SimHashFilter::Stats
	{
		int nofValues;
		int nofDatabaseReads;
		int probSum;
		int nofResults;
		int samplesMaxDist;

		Stats()
			:SimHashFilter::Stats(),nofValues(0),nofDatabaseReads(0),probSum(0),nofResults(0),samplesMaxDist(0) {}
		Stats( const Stats& o)
			:SimHashFilter::Stats(o),nofValues(o.nofValues),nofDatabaseReads(o.nofDatabaseReads),probSum(o.probSum),nofResults(o.nofResults),samplesMaxDist(o.samplesMaxDist) {}
	};

	SimHashMap( const strus::Reference<SimHashReaderInterface>& reader_, const strus::Index& typeno_)
		:m_filter(),m_idar(),m_reader(reader_),m_typeno(typeno_){}
	SimHashMap( const SimHashMap& o)
		:m_filter(o.m_filter),m_idar(o.m_idar),m_reader(o.m_reader),m_typeno(o.m_typeno){}
	~SimHashMap(){}
#if __cplusplus >= 201103L
	SimHashMap( SimHashMap&& o)
		:m_filter(std::move(o.m_filter)),m_idar(std::move(o.m_idar)),m_reader(std::move(o.m_reader)),m_typeno(o.m_typeno){}
	SimHashMap& operator =( SimHashMap&& o)
		{m_filter = std::move(o.m_filter); m_idar = std::move(o.m_idar); m_reader = std::move(o.m_reader); m_typeno = o.m_typeno; return *this;}
#endif
	SimHashMap& operator =( const SimHashMap& o)
		{m_filter = o.m_filter; m_idar = o.m_idar; m_reader = o.m_reader; m_typeno = o.m_typeno; return *this;}

	void load();

	std::vector<SimHashQueryResult> findSimilar( const SimHash& needle, int maxSimDist, int maxProbSimDist, int maxNofElements) const;
	std::vector<SimHashQueryResult> findSimilarWithStats( Stats& stats,const SimHash& needle, int maxSimDist, int maxProbSimDist, int maxNofElements) const;

	const strus::Index& typeno() const
	{
		return m_typeno;
	}

private:
	int getMaxSimDistFromBestFilterSamples( const std::vector<SimHashSelect>& candidates, const SimHash& needle, int maxNofElements, int nofSampleReads) const;

private:
	SimHashFilter m_filter;
	std::vector<Index> m_idar;
	strus::Reference<SimHashReaderInterface> m_reader;
	strus::Index m_typeno;
};

}//namespace
#endif


