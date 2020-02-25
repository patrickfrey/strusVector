/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of a sentence lexer based on a vector storage
/// \file sentenceLexerInstance.cpp
#include "sentenceLexerInstance.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/minimalCover.hpp"
#include "strus/base/string_format.hpp"
#include "strus/constants.hpp"
#include <limits>
#include <cstring>

using namespace strus;

#define MODULENAME "sentence lexer instance (vector storage)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"
#define SENTENCESIZE_AGAINST_COVERSIZE_WEIGHT 0.3

SentenceLexerInstance::SentenceLexerInstance( const VectorStorageClient* vstorage_, const DatabaseClientInterface* database_, const SentenceLexerConfig& config_, ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_vstorage(vstorage_),m_database(database_),m_config(config_)
	,m_typepriomap()
{
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);

	std::map<std::string,int>::const_iterator
		ti = m_config.typepriomap.begin(),
		te = m_config.typepriomap.end();
	for (; ti != te; ++ti)
	{
		strus::Index typeno = m_vstorage->getTypeNo( ti->first);
		if (typeno == 0) throw strus::runtime_error(_TXT("feature type '%s' undefined in vector storage"), ti->first.c_str());
		m_typepriomap[ typeno] = ti->second;
	}
}


SentenceLexerInstance::~SentenceLexerInstance()
{
	if (m_debugtrace) delete m_debugtrace;
}

typedef int GroupId;
typedef std::vector<GroupId> Group;

struct FeatNum
{
	strus::Index typeno;
	strus::Index featno;

	FeatNum( const strus::Index& typeno_, const strus::Index& featno_)
		:typeno(typeno_),featno(featno_){}
	FeatNum( const FeatNum& o)
		:typeno(o.typeno),featno(o.featno){}
	FeatNum()
		:typeno(0),featno(0){}

	bool operator < (const FeatNum& o) const
	{
		return featno == o.featno ? typeno < o.typeno : featno < o.featno;
	}
	bool valid() const
	{
		return typeno && featno;
	}
};

class SimGroupData
{
public:
	SimGroupData( const VectorStorageClient* vstorage_, double similarityDistance_)
		:m_vstorage(vstorage_),m_featnomap()
		,m_vectors(),m_groups(),m_featmap()
		,m_similarityDistance(similarityDistance_){}

	strus::Index getOrCreateFeatIndex( const std::string& value);
	std::string getOrCreateTypeString( strus::Index typeno);
	GroupId getOrCreateFeatGroup( const FeatNum& featnum);

	const std::vector<Group>& groups() const
	{
		return m_groups;
	}

private:
	const VectorStorageClient* m_vstorage;
	std::map<std::string,strus::Index> m_featnomap;
	std::vector<WordVector> m_vectors;
	std::vector<Group> m_groups;
	std::map<FeatNum,GroupId> m_featmap;
	double m_similarityDistance;
};

strus::Index SimGroupData::getOrCreateFeatIndex( const std::string& value)
{
	typedef std::map<std::string,strus::Index> Map;
	strus::Index featno = 0;
	Map::const_iterator mi = m_featnomap.find( value);
	if (mi == m_featnomap.end())
	{
		m_featnomap.insert( Map::value_type( value, featno=m_vstorage->getFeatNo( value)));
		return featno;
	}
	else
	{
		return mi->second;
	}
}

GroupId SimGroupData::getOrCreateFeatGroup( const FeatNum& featnum)
{
	typedef std::map<FeatNum,GroupId> Map;
	std::pair<Map::iterator,bool> ins = m_featmap.insert( Map::value_type( featnum, m_groups.size()));
	if (ins.second /*insert took place*/)
	{
		GroupId created_gidx = m_groups.size();
		m_groups.push_back( Group());
		m_groups.back().push_back( m_groups.size());

		if (featnum.typeno == 0)
		{
			// ... feature not in vector storage, create single element group without similarity to others
			m_vectors.push_back( WordVector());
		}
		else
		{
			m_vectors.push_back( m_vstorage->getVector( featnum.typeno, featnum.featno));

			std::vector<Group>::iterator gi = m_groups.begin(), ge = m_groups.end();
			GroupId gidx = 0;
	
			for (--ge/*without new group added*/; gi != ge; ++gi,++gidx)
			{
				if (!m_vectors.back().empty() && !m_vectors[ gidx].empty())
				{
					double sim = m_vstorage->vectorSimilarity( m_vectors.back(), m_vectors[ gidx]);
					if (sim > m_similarityDistance)
					{
						m_groups.back().push_back( gidx);
						gi->push_back( created_gidx);
					}
				}
			}
		}
		return created_gidx;
	}
	else
	{
		return ins.first->second;
	}
}

struct Rank
{
	int idx;
	double weight;

	Rank()
		:idx(0),weight(0.0){}
	Rank( int idx_, double weight_)
		:idx(idx_),weight(weight_){}
	Rank( const Rank& o)
		:idx(o.idx),weight(o.weight){}

	bool operator > (const Rank& o) const
	{
		return (std::abs( weight - o.weight) <= std::numeric_limits<float>::epsilon())
			? idx < o.idx
			: weight > o.weight;
	}
};

struct Local
{
	static double getWeight( int coverSize, int sentenceSize)
	{
		return 1.0/(double)std::sqrt(coverSize) + 1.0/(double)sentenceSize;
	}
};

std::vector<strus::Index> SentenceLexerInstance::getSelectedTypes( strus::Index featno) const
{
	std::vector<strus::Index> rt;
	std::vector<strus::Index> types = m_vstorage->getRelatedTypes( featno);
	std::vector<strus::Index>::const_iterator ti = types.begin(), te = types.end();
	int priority = -1;
	for (; ti != te; ++ti)
	{
		std::map<strus::Index,int>::const_iterator pi = m_typepriomap.find( *ti);
		if (pi != m_typepriomap.end())
		{
			if (priority < 0 || pi->second < priority)
			{
				
				rt.clear();
				rt.push_back( *ti);
			}
			else if (pi->second == priority)
			{
				rt.push_back( *ti);
			}
		}
	}
	return rt;
}

typedef std::vector<FeatNum> FeatNumList;

struct FeatNumVariantList
{
	std::vector<FeatNumList> ar;

	FeatNumVariantList() :ar() {ar.push_back(FeatNumList());}

	void add( const FeatNum& fn)
	{
		std::vector<FeatNumList>::iterator ai = ar.begin(), ae = ar.end();
		for (; ai != ae; ++ai)
		{
			ai->push_back( fn);
		}
	}

	void add( const FeatNumList& list)
	{
		if (list.empty()) return;
		if (list.size() == 1) add( list[0]);
		std::vector<FeatNumList> new_ar;
		FeatNumList::const_iterator li = list.begin(), le = list.end();
		for (; li != le; ++li)
		{
			std::vector<FeatNumList> follow = ar;
			std::vector<FeatNumList>::iterator fi = follow.begin(), fe = follow.end();
			for (; fi != fe; ++fi)
			{
				fi->push_back( *li);
			}
			new_ar.insert( new_ar.end(), follow.begin(), follow.end());
		}
		ar.swap( new_ar);
	}

	void crossJoin( const std::vector<FeatNumList>& list)
	{
		if (list.empty()) return;
		if (list.size() == 1)
		{
			std::vector<FeatNumList>::iterator ai = ar.begin(), ae = ar.end();
			for (; ai != ae; ++ai)
			{
				ai->insert( ai->end(), list[0].begin(), list[0].end());
			}
		}
		else
		{
			std::vector<FeatNumList> new_ar;
			std::vector<FeatNumList>::const_iterator li = list.begin(), le = list.end();
			for (; li != le; ++li)
			{
				std::vector<FeatNumList> follow = ar;
				std::vector<FeatNumList>::iterator fi = follow.begin(), fe = follow.end();
				for (; fi != fe; ++fi)
				{
					fi->insert( fi->end(), li->begin(), li->end());
				}
				new_ar.insert( new_ar.end(), follow.begin(), follow.end());
			}
			ar.swap( new_ar);
		}
	}
};

static std::string termListString( const SentenceTermList& terms, const char* sep)
{
	std::string rt;
	SentenceTermList::const_iterator ti = terms.begin(), te = terms.end();
	for (; ti != te; ++ti)
	{
		if (!rt.empty()) rt.append( sep);
		rt.append( strus::string_format( "%s '%s'", ti->type().c_str(), ti->value().c_str()));
	}
	return rt;
}

std::vector<SentenceGuess> SentenceLexerInstance::call( const std::string& source, int maxNofResults) const
{
	try
	{
		SentenceLexerKeySearch keySearch( m_vstorage, m_database, m_errorhnd, m_config.spaceSubst, m_config.linkSubst);
		std::vector<std::string> undefinedFeatureList;
	
		// Map features to sets of integers with the property that A * B != {}, if A ~ B
		// Each set gets an integer assigned
		// Assign group of such integers to sentences
		// The minimal cover of a group is used to calculate the weight of the candidate
		SimGroupData simGroupData( m_vstorage, m_config.similarityDistance);
		FeatNumVariantList sentences;
		std::vector<std::string> fields = m_config.normalizeSource( source);

		{
		std::vector<std::string>::const_iterator fi = fields.begin(), fe = fields.end();
		for (; fi != fe; ++fi)
		{
			std::vector<FeatNumList> fieldSentenceList;
			std::vector<SentenceLexerKeySearch::ItemList> items = keySearch.scanField( *fi);
			std::vector<SentenceLexerKeySearch::ItemList>::const_iterator
				ii = items.begin(), ie = items.end();
			for (; ii != ie; ++ii)
			{
				FeatNumVariantList variants;
				SentenceLexerKeySearch::ItemList::const_iterator
					ti = ii->begin(), te = ii->end();
				for (; ti != te; ++ti)
				{
					if (ti->resolved)
					{
						std::string featstr( fi->c_str() + ti->startpos, ti->endpos - ti->startpos);
						strus::Index featno = simGroupData.getOrCreateFeatIndex( featstr);
						if (!featno) throw std::runtime_error(_TXT("inconsistency in vector storage: resolved value not found"));
						std::vector<strus::Index> selectedTypes = getSelectedTypes( featno);
						if (selectedTypes.empty())
						{
							undefinedFeatureList.push_back( featstr);
							variants.add( FeatNum( 0, undefinedFeatureList.size()));
						}
						else if (selectedTypes.size() ==  1)
						{
							variants.add( FeatNum( selectedTypes[0], featno));
						}
						else
						{
							std::vector<FeatNum> fn;
							std::vector<strus::Index>::const_iterator
								si = selectedTypes.begin(), se = selectedTypes.end();
							for (; si != se; ++si)
							{
								fn.push_back( FeatNum( *si, featno));
							}
							variants.add( fn);
						}
					}
					else
					{
						std::string featstr( fi->c_str() + ti->startpos, ti->endpos - ti->startpos);
						undefinedFeatureList.push_back( featstr);
						variants.add( FeatNum( 0, undefinedFeatureList.size()));
					}
				}
				fieldSentenceList.insert( fieldSentenceList.end(), variants.ar.begin(), variants.ar.end());
			}
			sentences.crossJoin( fieldSentenceList);
		}}

		std::vector<std::vector<GroupId> > sentence_groups;
		sentence_groups.reserve( sentences.ar.size());

		std::vector<FeatNumList>::const_iterator si = sentences.ar.begin(), se = sentences.ar.end();
		for (; si != se; ++si)
		{
			sentence_groups.push_back( std::vector<GroupId>());
			FeatNumList::const_iterator ti = si->begin(), te = si->end();
			for (; ti != te; ++ti)
			{
				GroupId gid = simGroupData.getOrCreateFeatGroup( *ti);
				sentence_groups.back().push_back( gid);
			}
		}

		// Calculate minimal cover approximations and value boundaries for mapping pairs of (minimal cover size, nof terms) to an integer:
		MinimalCoverData minimalCoverData( simGroupData.groups(), m_errorhnd);
		std::vector<Rank> ranks;
		ranks.reserve( sentences.ar.size());

		std::vector<std::vector<GroupId> >::const_iterator gi = sentence_groups.begin(), ge = sentence_groups.end();
		for (int gidx=0; gi != ge; ++gi,++gidx)
		{
			int minimalCoverSize = minimalCoverData.minimalCoverApproximation( *gi).size();
			Rank rank( gidx, minimalCoverSize + sentences.ar[ gidx].size() * SENTENCESIZE_AGAINST_COVERSIZE_WEIGHT);
			ranks.push_back( rank);
		}

		// Select the best N (weight) of the ranks and return them
		if (maxNofResults < 0 || maxNofResults > (int)ranks.size())
		{
			maxNofResults = ranks.size();
			std::sort( ranks.begin(), ranks.end(), std::greater<Rank>());
		}
		else
		{
			std::nth_element( ranks.begin(), ranks.begin() + maxNofResults, ranks.end(), std::greater<Rank>());
			std::sort( ranks.begin(), ranks.begin() + maxNofResults, std::greater<Rank>());
			ranks.resize( maxNofResults);
		}

		// Normalize rank weights:
		double maxWeight = ranks.empty() ? 1.0 : ranks[ 0].weight;
		std::vector<Rank>::iterator ri = ranks.begin(), re = ranks.end();
		for (; ri != re; ++ri)
		{
			ri->weight /= maxWeight;
		}

		std::vector<SentenceGuess> rt;
		rt.reserve( maxNofResults);
		std::vector<std::string> typestrmap;

		// Build result returned:
		ri = ranks.begin();
		re = ranks.end();
		for (; ri != re; ++ri)
		{
			SentenceTermList termlist;
			const FeatNumList& feats = sentences.ar[ ri->idx];
			FeatNumList::const_iterator fi = feats.begin(), fe = feats.end();
			for (; fi != fe; ++fi)
			{
				if (fi->typeno == 0)
				{
					termlist.push_back( SentenceTerm( "", undefinedFeatureList[ fi->featno-1]));
				}
				else if (fi->typeno >= (int)typestrmap.size())
				{
					typestrmap.resize( fi->typeno+1);
					typestrmap[ fi->typeno] = m_vstorage->getTypeNameFromIndex( fi->typeno);
					termlist.push_back(
						SentenceTerm(
							typestrmap[ fi->typeno], 
							m_vstorage->getFeatNameFromIndex( fi->featno)));
				}
				else if (typestrmap[ fi->typeno].empty())
				{
					typestrmap[ fi->typeno] = m_vstorage->getTypeNameFromIndex( fi->typeno);
					termlist.push_back(
						SentenceTerm(
							typestrmap[ fi->typeno], 
							m_vstorage->getFeatNameFromIndex( fi->featno)));
				}
			}
			rt.push_back( SentenceGuess( termlist, ri->weight));
			if (m_debugtrace)
			{
				m_debugtrace->open( "ranklist");
				std::string sentstr = termListString( rt.back().terms(), ", ");
				m_debugtrace->event( "sentence", "weight %.3f content %s", ri->weight, sentstr.c_str());
				m_debugtrace->close();
			}
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' getting ranked list of sentence guesses: %s"), MODULENAME, *m_errorhnd, std::vector<SentenceGuess>());
}





