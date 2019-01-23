/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for retrieval of the most similar LSH values
#include "simHashMap.hpp"
#include "simHashRankList.hpp"
#include <algorithm>

using namespace strus;

#undef STRUS_LOWLEVEL_DEBUG

void SimHashMap::load()
{
#ifdef STRUS_LOWLEVEL_DEBUG
	struct CheckRank
	{
		Index id;
		int simdist;

		CheckRank() :id(0),simdist(0){}
		CheckRank( Index id_, int simdist_) :id(id_),simdist(simdist_){}
		CheckRank( const CheckRank& o) :id(o.id),simdist(o.simdist){}

		bool operator<( const CheckRank& o) const
		{
			return simdist == o.simdist ? id < o.id : simdist < o.simdist;
		}
	};
	std::vector<SimHash> lshar;
#endif
	const SimHash* val = m_reader->loadFirst();
	for (; val; val=m_reader->loadNext())
	{
		m_filter.append( val, 1);
		m_idar.push_back( val->id());
#ifdef STRUS_LOWLEVEL_DEBUG
		lshar.push_back( *val);
#endif
	}
#ifdef STRUS_LOWLEVEL_DEBUG
	std::vector<SimHash>::const_iterator li = lshar.begin(), le = lshar.end();
	for (; li != le; ++li)
	{
		std::vector<CheckRank> chkar;
		std::vector<SimHash>::const_iterator oi = lshar.begin(), oe = lshar.end();
		for (; oi != oe; ++oi)
		{
			chkar.push_back( CheckRank( oi->id(), li->dist( *oi)));
		}
		std::sort( chkar.begin(), chkar.end());
		std::vector<CheckRank>::const_iterator ci = chkar.begin(), ce = chkar.end();
		std::size_t cidx = 0;
		for (++cidx; ci != ce && ci->simdist < 300; ++ci,++cidx){}
		
		chkar.resize( cidx);
		std::vector<SimHashQueryResult> cres = findSimilar( *li, 340/*maxSimDist*/, 640/*maxProbSimDist*/, 20);
		std::vector<SimHashQueryResult>::const_iterator ri = cres.begin(), re = cres.end();
		for (; ri != re; ++ri)
		{
			if (ri->weight() > 0.95)
			{
				ci = chkar.begin(), ce = chkar.end();
				for (; ci != ce && ci->id != ri->featno(); ++ci){}
				if (ci == ce)
				{
					std::cout << "FEATURE DOES NOT MATCH" << std::endl;
				}
			}
		}
		ci = chkar.begin(), ce = chkar.end();
		for (; ci != ce; ++ci)
		{
			if (ci->simdist < 200)
			{
				ri = cres.begin(), re = cres.end();
				for (; ri != re && ci->id != ri->featno(); ++ri){}
				if (ri == re)
				{
					std::cout << "FEATURE NOT FOUND" << std::endl;
				}
			}
		}
	}
#endif
}

int SimHashMap::getMaxSimDistFromBestFilterSamples( const std::vector<SimHashSelect>& candidates, const SimHash& needle, int maxNofElements) const
{
	RankList<SimHashSelect> selectRanklist( maxNofElements);
	std::vector<SimHashSelect>::const_iterator ci = candidates.begin(), ce = candidates.end();
	for (; ci != ce; ++ci)
	{
		selectRanklist.insert( *ci);
	}
	int lastdist = 0;
	RankList<SimHashSelect>::const_iterator si = selectRanklist.begin(), se = selectRanklist.end();
	for (; si != se; ++si)
	{
		Index elemid = m_idar[ si->idx];
		SimHash val = m_reader->load( elemid);
		if (val.defined())
		{
			int dist = val.dist( needle);
			if (dist > lastdist)
			{
				lastdist = dist;
			}
		}
	}
	return lastdist;
}

std::vector<SimHashQueryResult> SimHashMap::findSimilar( const SimHash& needle, int maxSimDist, int maxProbSimDist, int maxNofElements) const
{
	if (m_idar.empty()) return std::vector<SimHashQueryResult>();

	SimHashRankList ranklist( maxNofElements);

	std::vector<SimHashSelect> candidates;
	m_filter.search( candidates, needle, maxSimDist, maxProbSimDist);

	int lastdist = getMaxSimDistFromBestFilterSamples( candidates, needle, maxNofElements);
	int probSum = m_filter.maxProbSumDist( maxSimDist, lastdist * ((float)maxProbSimDist / (float)maxSimDist) + 1);

	std::vector<SimHashSelect>::const_iterator ci = candidates.begin(), ce = candidates.end();
	for (; ci != ce; ++ci)
	{
		Index elemid = m_idar[ ci->idx];
		if (ci->shdiff < probSum)
		{
			SimHash val = m_reader->load( elemid);
			if (val.defined())
			{
				if (val.near( needle, maxSimDist))
				{
					int dist = val.dist( needle);
					(void)ranklist.insert( SimHashRank( elemid, dist));
				}
			}
		}
	}
	return ranklist.result( needle.size());
}

std::vector<SimHashQueryResult> SimHashMap::findSimilarWithStats( Stats& stats, const SimHash& needle, int maxSimDist, int maxProbSimDist, int maxNofElements) const
{
	stats.nofValues = m_idar.size();
	if (m_idar.empty()) return std::vector<SimHashQueryResult>();

	SimHashRankList ranklist( maxNofElements);

	std::vector<SimHashSelect> candidates;
	m_filter.searchWithStats( stats, candidates, needle, maxSimDist, maxProbSimDist);

	int lastdist = getMaxSimDistFromBestFilterSamples( candidates, needle, maxNofElements);
	int probSum = m_filter.maxProbSumDist( maxSimDist, lastdist * ((float)maxProbSimDist / (float)maxSimDist) + 1);

	stats.nofDatabaseReads += (int)candidates.size() < maxNofElements ? (int)candidates.size() : (int)candidates.size();
	stats.probSum = probSum;

	std::vector<SimHashSelect>::const_iterator ci = candidates.begin(), ce = candidates.end();
	for (; ci != ce; ++ci)
	{
		Index elemid = m_idar[ ci->idx];
		if (ci->shdiff < probSum)
		{
			++stats.nofDatabaseReads;
			SimHash val = m_reader->load( elemid);
			if (val.defined())
			{
				if (val.near( needle, maxSimDist))
				{
					++stats.nofResults;
					int dist = val.dist( needle);
					(void) ranklist.insert( SimHashRank( elemid, dist));
				}
			}
		}
	}
	return ranklist.result( needle.size());
}


