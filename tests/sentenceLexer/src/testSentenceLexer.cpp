/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Test program for feature vector database based sentence lexer
#include "strus/lib/vector_std.hpp"
#include "strus/lib/database_leveldb.hpp"
#include "strus/lib/error.hpp"
#include "strus/lib/filelocator.hpp"
#include "strus/vectorStorageInterface.hpp"
#include "strus/vectorStorageClientInterface.hpp"
#include "strus/vectorStorageTransactionInterface.hpp"
#include "strus/databaseInterface.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/fileLocatorInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "strus/sentenceLexerContextInterface.hpp"
#include "strus/sentenceLexerInstanceInterface.hpp"
#include "strus/base/configParser.hpp"
#include "strus/base/stdint.h"
#include "strus/base/fileio.hpp"
#include "strus/base/local_ptr.hpp"
#include "strus/base/string_format.hpp"
#include "strus/base/numstring.hpp"
#include "strus/base/pseudoRandom.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/string_format.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <cstring>
#include <limits>
#include <cstdio>
#include <algorithm>

#define STRUS_PSEUDO_RANDOM_SEED -763074118
#define STRUS_DBGTRACE_COMPONENT_NAME "test"
#define VEC_EPSILON 1e-5
static bool g_verbose = false;
static strus::PseudoRandom g_random( STRUS_PSEUDO_RANDOM_SEED);
static strus::ErrorBufferInterface* g_errorhnd = 0;
static strus::DebugTraceInterface* g_dbgtrace = 0;
static strus::FileLocatorInterface* g_fileLocator = 0;

static char g_delimiterSubst = '-';
static char g_spaceSubst = '_';
static std::vector<int> g_delimiters = {0x2019,'`','\'','?','!','/',':','.',',','-',0x2014,')','(','[',']','{','}','<','>'};
static std::vector<int> g_spaces = {32,'\t',0xA0,0x2008,0x200B};
static std::vector<int> g_alphabet = {'a','b','c','d','e','f','0','1','2','3','4','5','6','7','8','9',0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0x391,0x392,0x393,0x394,0x395,0x396,0x9A0,0x9A1,0x9A2,0x9A3,0x9A4,0x9A5,0x10B0,0x10B1,0x10B2,0x10B3,0x10B4,0x10B5,0x35B0,0x35B1,0x35B2,0x35B3,0x35B4,0x35B5};
strus::Index g_nofTypes = 2;
enum  {MaxNofTypes = 1000};
strus::Index g_nofTerms = 1000;
strus::Index g_nofFeatures = 10000;
int g_maxTermLength = 30;
int g_maxFeatureTerms = 5;
int g_maxNofCollisions = 1000;
int g_maxNofPositionVisits = 100; //... same value as 'lexprun=?' in DEFAULT_CONFIG

#define DEFAULT_CONFIG "path=vstorage;lexprun=100"

struct FeatureTerm
{
	std::string type;
	std::string value;

	FeatureTerm( const std::string& type_, const std::string& value_)
		:type(type_),value(value_){}
	FeatureTerm( const FeatureTerm& o)
		:type(o.type),value(o.value){}
};

static std::string typeString( int tidx)
{
	char buf[ 32];
	if (g_nofTypes >= 100)
	{
		std::snprintf( buf, sizeof(buf), "T%03d", tidx);
	}
	else if (g_nofTypes >= 10)
	{
		std::snprintf( buf, sizeof(buf), "T%02d", tidx);
	}
	else
	{
		std::snprintf( buf, sizeof(buf), "T%d", tidx);
	}
	return std::string(buf);
}

static std::string randomTerm()
{
	std::string rt;
	std::size_t length = 1+g_random.get( 0, 1+g_random.get( 0, 1+g_random.get( 0, 1+g_maxTermLength)));
	if (length == 0) length = 1;
	std::size_t li = 0, le = length;
	for (; li != le; ++li)
	{
		char chrbuf[ 32];
		int32_t chr = g_alphabet[ g_random.get( 0, 1+g_random.get( 0, g_alphabet.size()))];
		std::size_t chrlen = strus::utf8encode( chrbuf, chr);
		rt.append( chrbuf, chrlen);
	}
	return rt;
}

static char randomSeparator( bool withEmpty, bool withSpace)
{
	char rndsep[4] = {'\0',g_delimiterSubst,g_spaceSubst,' '};
	int start = withEmpty ? 0 : 1;
	int end = withSpace ? 4 : 3;
	return rndsep[ g_random.get( start, end)];
}

static std::string randomDelimiterString( const std::vector<int>& delimiters)
{
	std::string rt;
	do
	{
		int chr = delimiters[ g_random.get( 0, 1+g_random.get( 0, delimiters.size()))];
		char chrbuf[32];
		std::size_t chrlen = strus::utf8encode( chrbuf, chr);
		rt.append( chrbuf, chrlen);
	}
	while (g_random.get( 0, 10) == 1);
	return rt;
}

static std::string randomSeparatorMasking( char separator)
{
	std::string rt;
	if (!separator) separator = ' ';
	if (g_random.get( 0,3) > 1)
	{
		rt.push_back( separator);
	}
	else if (separator == ' ')
	{
		rt.append( randomDelimiterString( g_spaces));
	}
	else if (separator == g_delimiterSubst)
	{
		rt.append( randomDelimiterString( g_delimiters));
	}
	else if (separator == g_spaceSubst)
	{
		rt.push_back( separator);
	}
	return rt;
}

struct TermDef
{
	int termidx;
	char separator;
	bool split;

	TermDef( bool withEmpty, bool withSpace)
		:split(true)
	{
		termidx = g_random.get( 0, 1+g_random.get( 0, 1+g_random.get( 0, g_nofTerms)));
		separator = randomSeparator( withEmpty, withSpace);
	}

	TermDef( int termidx_, char separator_)
		:termidx(termidx_),separator(separator_),split(true){}
	TermDef( const TermDef& o)
		:termidx(o.termidx),separator(o.separator),split(o.split){}

	bool operator < (const TermDef& o) const
	{
		return termidx == o.termidx ? (separator == o.separator ? split < o.split : separator < o.separator) : termidx < o.termidx;
	}
	bool operator == (const TermDef& o) const
	{
		return termidx == o.termidx && separator == o.separator && split == o.split;
	}
	int compare( const TermDef& o) const
	{
		return termidx == o.termidx ? (separator == o.separator ? (int)split - (int)o.split : separator - o.separator) : termidx - o.termidx;
	}
};

struct TermSequenceDef
{
	TermSequenceDef( const std::vector<TermDef>& terms_, char separatorAtStart_)
		:terms(terms_),separatorAtStart(separatorAtStart_){}
	TermSequenceDef( const TermSequenceDef& o)
		:terms(o.terms),separatorAtStart(o.separatorAtStart){}
	TermSequenceDef( bool withUnknownTerms, bool withSpace)
	{
		int fi = 0, fe = 1+g_random.get( 0, 1+g_random.get( 0, g_maxFeatureTerms));
		for (; fi != fe; ++fi)
		{
			terms.push_back( TermDef( false/*with empty*/, withSpace));
			if (withUnknownTerms && g_random.get( 0, 20) == 1)
			{
				terms.back().termidx = -terms.back().termidx;
			}
		}
		separatorAtStart = g_random.get( 1, 50 + g_nofFeatures / 20) == 1 ? randomSeparator( true/*with empty*/, false/*with space*/) : '\0';
		if (g_random.get( 1, 20 + g_nofFeatures / 20) > 1)
		{
			//... mask out separator and end
			terms.back().separator = '\0';
		}
	}
	TermSequenceDef& operator = (const TermSequenceDef& o)
	{
		terms = o.terms;
		separatorAtStart = o.separatorAtStart;
		return *this;
	}

	void modify( bool withSpace)
	{
		int rndidx;
		int newtermidx;
		do
		{
			switch (g_random.get( 0, 4))
			{
				case 0:
					separatorAtStart = randomSeparator( true/*with empty*/, false/*with space*/);
					break;
				case 1:
					rndidx = g_random.get( 0, terms.size());
					terms[ rndidx].separator = randomSeparator( false/*with empty*/, withSpace);
					break;
				case 2:
					rndidx = g_random.get( 0, terms.size());
					newtermidx = (g_random.get( 0, 3) == 1)
						? g_random.get( 0, 1+g_random.get( 0, 1+g_random.get( 0, g_nofTerms)))
						: ((terms[ rndidx].termidx + 1) % g_nofTerms);
					terms[ rndidx].termidx = newtermidx;
					break;
				default:
				{
					TermDef newterm( false/*withEmpty*/, withSpace);
					terms.back().separator = newterm.separator;
					newterm.separator = '\0';
					terms.push_back( newterm);
					break;
				}
			}
		}
		while (g_random.get( 0, 4) == 1);
	}
	typedef std::vector<int> Id;
	Id id() const
	{
		Id rt;
		rt.push_back( separatorAtStart );
		std::vector<TermDef>::const_iterator ei = terms.begin(), ee = terms.end();
		for (; ei != ee; ++ei)
		{
			rt.push_back( ei->termidx);
			rt.push_back( ei->separator);
		}
		return rt;
	}

	std::string tostring( const std::vector<std::string>& termstrings, const std::vector<std::string>& unknownTermstrings) const
	{
		std::string rt;
		if (separatorAtStart)
		{
			rt.push_back( separatorAtStart);
		}
		std::vector<TermDef>::const_iterator ei = terms.begin(), ee = terms.end();
		for (; ei != ee; ++ei)
		{
			rt.append( (ei->termidx >= 0) ? termstrings[ ei->termidx] : unknownTermstrings[ -ei->termidx]);
			if (ei->separator) rt.push_back( ei->separator);
		}
		return rt;
	}

	std::string querystring( const std::vector<std::string>& termstrings, const std::vector<std::string>& unknownTermstrings)
	{
		std::string rt;
		if (separatorAtStart)
		{
			rt.append( randomSeparatorMasking( separatorAtStart));
		}
		std::vector<TermDef>::iterator ei = terms.begin(), ee = terms.end();
		for (; ei != ee; ++ei)
		{
			rt.append( (ei->termidx >= 0) ? termstrings[ ei->termidx] : unknownTermstrings[ -ei->termidx]);
			std::string sep = randomSeparatorMasking( ei->separator);
			ei->split = !(sep.size() == 1 && (sep[0] == g_spaceSubst || sep[0] == g_delimiterSubst));
			rt.append( sep);
		}
		return rt;
	}

	bool operator < ( const TermSequenceDef& o) const {return compare( o) < 0;}
	bool operator == ( const TermSequenceDef& o) const {return compare( o) == 0;}

	int compare( const TermSequenceDef& o) const
	{
		if (separatorAtStart == o.separatorAtStart)
		{
			std::vector<TermDef>::const_iterator ai = terms.begin(), ae = terms.end(), bi = o.terms.begin(), be = o.terms.end();
			for (; ai != ae && bi != be && *ai == *bi; ++ai,++bi){}
			if (ai == ae && bi == be) return 0;
			if (ai == ae) return -1;
			if (bi == be) return +1;
			return ai->compare(*bi);
		}
		else return ((int)separatorAtStart - (int)o.separatorAtStart);
	}
	std::size_t size() const
	{
		return terms.size();
	}

	std::vector<TermDef> terms;
	char separatorAtStart;
};

struct FeatureDef
{
	FeatureDef( const TermSequenceDef& termseq_, const std::vector<int>& typeindices_)
		:termseq(termseq_),typeindices(typeindices_){}
	FeatureDef( const FeatureDef& o)
		:termseq(o.termseq),typeindices(o.typeindices){}
	FeatureDef()
		:termseq( false/*with unknowns*/, false/*with spaces*/),typeindices()
	{
		std::set<int> typeset;
		int ti = 0, te = 1+g_random.get( 0, 1+g_random.get( 0, 1+g_nofTypes));
		for (; ti != te; ++ti)
		{
			typeset.insert( g_random.get( 0, g_nofTypes));
		}
		typeindices.insert( typeindices.end(), typeset.begin(), typeset.end());
	}
	FeatureDef& operator = (const FeatureDef& o)
	{
		termseq = o.termseq;
		typeindices = o.typeindices;
		return *this;
	}

	std::string typestring() const
	{
		std::string rt;
		rt.push_back( '{');
		std::vector<int>::const_iterator ti = typeindices.begin(), te = typeindices.end();
		for (int tidx=0; ti != te; ++ti,++tidx)
		{
			if (tidx) rt.append(", ");
			rt.append( typeString( *ti));
		}
		rt.push_back( '}');
		return rt;
	}
	typedef std::vector<int> Id;
	Id id() const
	{
		return termseq.id();
	}

	bool operator < ( const FeatureDef& o) const	{return compare( o) < 0;}
	bool operator == ( const FeatureDef& o) const	{return compare( o) == 0;}

	int compareTypes( const FeatureDef& o) const
	{
		std::set<int> aa( typeindices.begin(), typeindices.end());
		std::set<int> bb( o.typeindices.begin(), o.typeindices.end());
		std::set<int>::const_iterator ai = aa.begin(), ae = aa.end(), bi = bb.begin(), be = bb.end();
		for (; ai != ae && bi != be && *ai == *bi; ++ai,++bi){}
		if (ai == ae && bi == be) return 0;
		if (ai == ae) return -1;
		if (bi == be) return +1;
		return *ai - *bi;
	}
	int compare( const FeatureDef& o) const
	{
		int cmp = termseq.compare( o.termseq);
		return cmp ? cmp : compareTypes( o);
	}

	TermSequenceDef termseq;
	std::vector<int> typeindices;
};

static int compareFeatureDefVectors( const std::vector<FeatureDef>& a, const std::vector<FeatureDef>& b)
{
	std::vector<FeatureDef>::const_iterator ai = a.begin(), ae = a.end();
	std::vector<FeatureDef>::const_iterator bi = b.begin(), be = b.end();
	for (; ai != ae && bi != be && *ai == *bi; ++ai,++bi){}
	if (ai == ae && bi == be) return 0;
	if (ai == ae) return -1;
	if (bi == be) return +1;
	return ai->compare( *bi);
}

struct QueryDef
	:public TermSequenceDef
{
	QueryDef( const QueryDef& o)
		:TermSequenceDef(o){}
	QueryDef()
		:TermSequenceDef( true/*with unknowns*/, true/*with spaces*/){}
	QueryDef( const FeatureDef& o)
		:TermSequenceDef(o.termseq){}

	QueryDef& operator = (const QueryDef& o)
	{
		TermSequenceDef::operator =(o);
		return *this;
	}

	static bool tryJoin( QueryDef& ths, const QueryDef& o)
	{
		bool match = false;
		if (ths.terms.empty())
		{
			match = (ths.separatorAtStart == o.separatorAtStart);
		}
		else if (ths.terms.back().separator == o.separatorAtStart)
		{
			match = true;
		}
		if (match)
		{
			ths.terms.insert( ths.terms.end(), o.terms.begin(), o.terms.end());
			return true;
		}
		return false;
	}
};

class TestData
{
public:
	TestData()
	{
		m_debugtrace = g_dbgtrace && g_verbose ? g_dbgtrace->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME) : NULL;

		std::set<std::string> termset;
		std::set<std::string> unknownTermset;

		if (g_verbose) std::cerr << "create random known terms ... " << std::endl;
		int nofTries = 0;
		int ti = 0, te = g_nofTerms;
		for (; ti != te; ++ti)
		{
			std::string term = randomTerm();
			if (termset.insert( term).second == false)
			{
				//... no insert, collision
				--ti;
				++nofTries;
				if (nofTries >= g_maxNofCollisions)
				{
					int newMaxTermLength = g_maxTermLength + g_maxTermLength / 2;
					std::cerr << strus::string_format( "maximum number of collisions (%d) reached, raising maximum term length from %d to %d to be able to create a random collection of terms", g_maxNofCollisions, g_maxTermLength, newMaxTermLength) << std::endl;
					g_maxTermLength = newMaxTermLength;
					nofTries = 0;
				}
				continue;
			}
			m_termmap[ term] = ti;
			m_terms.push_back( term);
			nofTries = 0;
		}

		if (g_verbose) std::cerr << "create random unknown terms ... " << std::endl;
		nofTries = 0;
		m_unknownTerms.push_back( std::string());
		ti = 0, te = g_nofTerms;
		for (; ti != te; ++ti)
		{
			std::string term = randomTerm();
			if (termset.find( term) != termset.end() || unknownTermset.insert( term).second == false)
			{
				//... no insert, collision
				--ti;
				++nofTries;
				if (nofTries >= g_maxNofCollisions)
				{
					int newMaxTermLength = g_maxTermLength + g_maxTermLength / 5;
					std::cerr << strus::string_format( "maximum number of collisions (%d) reached, raising maximum term length from %d to %d to be able to create a random collection of terms", g_maxNofCollisions, g_maxTermLength, newMaxTermLength) << std::endl;
					g_maxTermLength = newMaxTermLength;
					nofTries = 0;
				}
				continue;
			}
			m_unknownTermmap[ term] = -(int)m_unknownTerms.size();
			m_unknownTerms.push_back( term);
			nofTries = 0;
		}
		ti = 0, te = g_nofTypes;
		for (; ti != te; ++ti)
		{
			m_typemap[ typeString( ti)] = ti;
		}

		if (g_verbose) std::cerr << "create random features ... " << std::endl;
		std::set<TermSequenceDef::Id> fset;
		nofTries = 0;
		while (g_nofFeatures > (int)m_features.size())
		{
			FeatureDef fdef;
			if (fset.insert( fdef.id()).second != true)
			{
				//... no insert, collision
				++nofTries;
				if (nofTries >= g_maxNofCollisions)
				{
					int newMaxFeatureTerms = g_maxFeatureTerms + g_maxFeatureTerms / 2;
					std::cerr << strus::string_format( "maximum number of collisions (%d) reached, raising maximum feature terms from %d to %d to be able to create a random collection of features", g_maxNofCollisions, g_maxFeatureTerms, newMaxFeatureTerms) << std::endl;
					g_maxFeatureTerms = newMaxFeatureTerms;
					nofTries = 0;
				}
				continue;
			}
			m_featureStartMap.insert( std::pair<int,int>( fdef.termseq.terms[0].termidx, m_features.size()));
			m_features.push_back( fdef);
			while (g_nofFeatures > (int)m_features.size() && g_random.get( 0, 5) == 1)
			{
				fdef.termseq.modify( false/*with space*/);
				if (fset.insert( fdef.id()).second == true)
				{
					m_featureStartMap.insert( std::pair<int,int>( fdef.termseq.terms[0].termidx, m_features.size()));
					m_features.push_back( fdef);
				}
			}
			nofTries = 0;
		}

		if (g_verbose) std::cerr << "create random queries ... " << std::endl;
		int qi = 0, qe = g_nofFeatures;
		for (; qi != qe; ++qi)
		{
			if (qi == 0 || g_random.get( 0,3) == 1)
			{
				// ... Query derived from an existing feature
				int featidx = g_random.get( 0, 1+g_random.get( 0, m_features.size()));
				QueryDef qdef( m_features[ featidx]);
				m_queries.push_back( qdef);
			}
			else
			{
				// ... Random query existing
				QueryDef qdef;
				m_queries.push_back( qdef);
			}
			while (m_queries.size() > 10 && g_random.get( 0,3) == 1)
			{
				// ... Join query created with another query
				int joinidx = g_random.get( 0, 1+g_random.get( 0, m_queries.size()));
				QueryDef::tryJoin( m_queries.back(), m_queries[ joinidx]);
			}
			m_querystrings.push_back( m_queries.back().querystring( m_terms, m_unknownTerms));
		}
	}
	~TestData()
	{
		if (m_debugtrace) delete m_debugtrace;
	}

	const std::map<std::string,int>& termmap() const
	{
		return m_termmap;
	}
	const std::map<std::string,int>& typemap() const
	{
		return m_typemap;
	}
	const std::vector<std::string>& terms() const
	{
		return m_terms;
	}
	const std::vector<std::string>& unknownTerms() const
	{
		return m_unknownTerms;
	}
	const std::map<std::string,int>& unknownTermmap() const
	{
		return m_unknownTermmap;
	}
	int termIndex( const std::string& term) const
	{
		std::map<std::string,int>::const_iterator mi = m_termmap.find( term);
		return mi == m_termmap.end() ? -1 : mi->second;
	}
	int nofFeatures() const
	{
		return m_features.size();
	}
	std::string featureString( int featureIdx) const
	{
		return m_features[ featureIdx].termseq.tostring( m_terms, m_unknownTerms);
	}
	std::vector<std::string> featureTypes( int featureIdx) const
	{
		std::vector<std::string> rt;
		const FeatureDef& fdef = m_features[ featureIdx];
		std::vector<int>::const_iterator ti = fdef.typeindices.begin(), te = fdef.typeindices.end();
		for (; ti != te; ++ti)
		{
			rt.push_back( typeString( *ti));
		}
		return rt;
	}
	int nofQueries() const
	{
		return m_queries.size();
	}
	const std::string& queryString( int queryIdx) const
	{
		return m_querystrings[ queryIdx];
	}
	const QueryDef& query( int queryIdx) const
	{
		return m_queries[ queryIdx];
	}
	static bool match( const TermDef& query, const TermDef& feat)
	{
		if (query.termidx != feat.termidx) return false;
		if (query.separator != ' ' && query.separator != '\0' && query.separator != feat.separator) return false;
		return true;
	}

	struct Candidate
	{
		std::vector<TermDef>::const_iterator itr;
		std::vector<FeatureDef> features;
		int nofUntyped;
		int pos;

		Candidate( std::vector<TermDef>::const_iterator itr_, const std::vector<FeatureDef>& features_, int nofUntyped_, int pos_)
			:itr(itr_),features(features_),nofUntyped(nofUntyped_),pos(pos_){}
		Candidate( const Candidate& o)
			:itr(o.itr),features(o.features),nofUntyped(o.nofUntyped),pos(o.pos){}
		Candidate& operator = ( const Candidate& o)
		{
			itr = o.itr;
			features = o.features;
			nofUntyped = o.nofUntyped;
			pos = o.pos;
			return *this;
		}

		bool operator < (const Candidate& o) const
		{
			return compare( o) < 0;
		}
		int compare( const Candidate& o) const
		{
			return features.size() == o.features.size()
				? (pos == o.pos
					? (nofUntyped == o.nofUntyped
						? compareFeatureDefVectors( features, o.features)
						: nofUntyped - o.nofUntyped)
					: pos - o.pos)
				: (int)features.size() - (int)o.features.size();
		}
	};

	/// \brief Defines prunning of evaluation paths not minimizing the number of features detected
	/// \brief Defines a limit for prunning variants evaluated dependend on the minimum number of features of a found solution
	static int maxFeaturePrunning( int minNofFeatures)
	{
		enum {ArSize=16};
		static const int ar[ ArSize+1] = {0/*0*/,3/*1*/,4/*2*/,5/*3*/,7/*4*/,8/*5*/,10/*6*/,11/*7*/,12/*8*/,13/*9*/,14/*10*/,15/*11*/,16/*12*/,17/*13*/,18/*14*/,19/*15*/,21/*16*/};
		return (minNofFeatures <= ArSize) ? ar[ minNofFeatures] : (minNofFeatures + 5 + (minNofFeatures >> 4));
	}

	std::string featureListToString( const std::vector<FeatureDef>& features) const
	{
		std::string rt;
		std::vector<FeatureDef>::const_iterator fi = features.begin(), fe = features.end();
		for (; fi != fe; ++fi)
		{
			if (!rt.empty()) rt.push_back(' ');
			rt.append( fi->termseq.tostring( m_terms, m_unknownTerms));
		}
		return rt;
	}

	TermSequenceDef getDefaultTermSequenceDef( std::vector<TermDef>::const_iterator ai, const std::vector<TermDef>::const_iterator& ae, char separatorAtStart) const
	{
		std::vector<TermDef> tt;
		for (; ai != ae && !ai->split; ++ai)
		{
			tt.push_back( *ai);
		}
		if (ai != ae)
		{
			tt.push_back( TermDef( ai->termidx, '\0'));
		}
		return TermSequenceDef( tt, separatorAtStart);
	}

	std::vector<std::vector<FeatureDef> > findTermSequence( const TermSequenceDef& termdef) const
	{
		if (m_debugtrace)
		{
			m_debugtrace->open( "test");
		}
		if (m_debugtrace)
		{
			std::string fstr = termdef.tostring( m_terms, m_unknownTerms);
			m_debugtrace->event( "find", "term sequence '%s'", fstr.c_str());
		}
		std::vector<std::vector<FeatureDef> > rt;
		std::set<Candidate> candidates;
		std::vector<TermDef>::const_iterator itr = termdef.terms.begin(), itr_end = termdef.terms.end();
		if (itr != itr_end)
		{
			Candidate newCandidate( itr, std::vector<FeatureDef>(), 0, 0);
			candidates.insert( newCandidate);
		}
		int minNofUntyped = std::numeric_limits<int>::max();
		int minNofFeats = std::numeric_limits<int>::max();
		struct VisitCount
		{
			int nofFeatures;
			int cnt;

			VisitCount()
				:nofFeatures(0),cnt(0){}
			VisitCount( int nofFeatures_, int cnt_)
				:nofFeatures(nofFeatures_),cnt(cnt_){}
			VisitCount( const VisitCount& o)
				:nofFeatures(o.nofFeatures),cnt(o.cnt){}
		};
		std::vector<VisitCount> visitCountMap;

		typedef std::multimap<int,int>::const_iterator Iter;
		typedef std::pair<Iter,Iter> Range;

		while (!candidates.empty())
		{
			// Prunning by number of unknown features
			if (candidates.begin()->nofUntyped > minNofUntyped)
			{
				candidates.erase( candidates.begin());
				continue;
			}
			Candidate candidate = *candidates.begin();
			TermSequenceDef defaultTerms = getDefaultTermSequenceDef( candidate.itr, termdef.terms.end(), candidate.pos ? '\0' : termdef.separatorAtStart);
			bool hasDefault = true;
			candidates.erase( candidates.begin());

			if (m_debugtrace)
			{
				std::string tstr = candidate.itr->termidx < 0 ? m_unknownTerms[ -candidate.itr->termidx] : m_terms[ candidate.itr->termidx];
				std::string fliststr = featureListToString( candidate.features);
				m_debugtrace->event( "candidate", "features '%s', term '%s'", fliststr.c_str(), tstr.c_str());
			}
			// Prunning by visit count and expected features:
			int nofFeats = candidate.features.size();
			int expectedNofFeats = candidate.features.size()+1;
			while ((int)visitCountMap.size() <= candidate.pos) visitCountMap.push_back( VisitCount());
			VisitCount& vc = visitCountMap[ candidate.pos];
			if (vc.cnt == 0)
			{
				vc.nofFeatures = nofFeats;
				vc.cnt = 1;
			}
			else if (nofFeats < vc.nofFeatures)
			{
				vc.nofFeatures = nofFeats;
				expectedNofFeats = minNofFeats;
			}
			else
			{
				if (vc.cnt++ >= g_maxNofPositionVisits) continue;
				expectedNofFeats = nofFeats + minNofFeats - vc.nofFeatures;
			}
			if (minNofFeats != std::numeric_limits<int>::max() && expectedNofFeats > maxFeaturePrunning( minNofFeats)) continue;

			// Search matches:
			if (m_debugtrace)
			{
				const char* aa = candidate.itr->termidx < 0  ? "unknown term":"known term";
				std::string tstr = candidate.itr->termidx < 0 ? m_unknownTerms[ -candidate.itr->termidx] : m_terms[ candidate.itr->termidx];
				m_debugtrace->event( "findmatch", "%s '%s'", aa, tstr.c_str());
			}
			Range range = m_featureStartMap.equal_range( candidate.itr->termidx);
			Iter ri = range.first;
			Iter re = range.second;
			for (; ri != re; ++ri)
			{
				if (m_debugtrace)
				{
					std::string fstr = featureString( ri->second);
					m_debugtrace->event( "testmatch", "'%s'", fstr.c_str());
				}
				const FeatureDef& fdef = m_features[ ri->second];
				if (candidate.pos == 0)
				{
					if (termdef.separatorAtStart != fdef.termseq.separatorAtStart) continue;
				}
				else
				{
					if (fdef.termseq.separatorAtStart) continue;
				}
				std::vector<TermDef>::const_iterator
					ai = candidate.itr, ae = termdef.terms.end(),
					bi = fdef.termseq.terms.begin(), be = fdef.termseq.terms.end();
				int aidx = 0;
				bool split = false;
				for (; ai != ae && bi != be; ++ai,++bi,++aidx)
				{
					split = ai->split;
					if (split && aidx+1 == (int)fdef.termseq.terms.size())
					{
						if (!match( TermDef( ai->termidx, '\0'), *bi)) break;
					}
					else
					{
						if (!match(*ai,*bi)) break;
					}
				}
				if (bi != be) continue;
				if (!split && ai != ae) continue;

				std::vector<FeatureDef> features( candidate.features);
				features.push_back( fdef);
				if (m_debugtrace)
				{
					std::string typeliststr = fdef.typestring();
					std::string fstr = fdef.termseq.tostring( m_terms, m_unknownTerms);
					m_debugtrace->event( "feature", "'%s' %s", fstr.c_str(), typeliststr.c_str());
				}
				if (defaultTerms == fdef.termseq)
				{
					hasDefault = false;
				}
				if (ai != ae)
				{
					if (m_debugtrace)
					{
						std::string fliststr = featureListToString( features);
						m_debugtrace->event( "follow", "'%s' next pos %d", fliststr.c_str(), (int)(candidate.pos + aidx));
					}
					Candidate newCandidate( ai, features, candidate.nofUntyped, candidate.pos + aidx);
					candidates.insert( newCandidate);
				}
				else if ((minNofFeats == std::numeric_limits<int>::max() || (int)features.size() <= maxFeaturePrunning( minNofFeats))
				&&	(minNofUntyped == std::numeric_limits<int>::max() || candidate.nofUntyped <= minNofUntyped))
				{
					if (minNofUntyped > candidate.nofUntyped)
					{
						minNofUntyped = candidate.nofUntyped;
						if (m_debugtrace) m_debugtrace->event( "prunning", "clear %d results (nof untyped)", (int)rt.size());
						rt.clear();
					}
					if (minNofFeats > (int)features.size())
					{
						minNofFeats = features.size();
					}
					if (m_debugtrace)
					{
						std::string fliststr = featureListToString( features);
						m_debugtrace->event( "result", "'%s'", fliststr.c_str());
					}
					rt.push_back( features);
				}
			}

			// Add default unknown match if not aleady added:
			if (hasDefault)
			{
				std::vector<FeatureDef> features( candidate.features);
				features.push_back( FeatureDef( defaultTerms, std::vector<int>()));
				if (m_debugtrace)
				{
					std::string fstr = features.back().termseq.tostring( m_terms, m_unknownTerms);
					m_debugtrace->event( "feature", "'%s' (default)", fstr.c_str());
				}
				std::vector<TermDef>::const_iterator ai = candidate.itr;
				ai += defaultTerms.size();
				if (ai < termdef.terms.end())
				{
					if (m_debugtrace)
					{
						std::string fliststr = featureListToString( features);
						m_debugtrace->event( "follow", "'%s' (default) next pos %d", fliststr.c_str(), (int)(candidate.pos + 1));
					}
					Candidate newCandidate( ai, features, candidate.nofUntyped+1, candidate.pos + 1);
					candidates.insert( newCandidate);
				}
				else if ((minNofFeats == std::numeric_limits<int>::max() || (int)features.size() <= maxFeaturePrunning( minNofFeats))
				&&	(minNofUntyped == std::numeric_limits<int>::max() || candidate.nofUntyped+1 <= minNofUntyped))
				{
					if (minNofUntyped > candidate.nofUntyped+1)
					{
						minNofUntyped = candidate.nofUntyped+1;
						if (m_debugtrace) m_debugtrace->event( "prunning", "clear %d results (nof untyped)", (int)rt.size());
						rt.clear();
					}
					if (minNofFeats > (int)features.size())
					{
						minNofFeats = features.size();
					}
					if (m_debugtrace)
					{
						std::string fliststr = featureListToString( features);
						m_debugtrace->event( "result", "'%s'", fliststr.c_str());
					}
					rt.push_back( features);
				}
			}
		}
		if (m_debugtrace)
		{
			m_debugtrace->close();
		}
		return rt;
	}

	int getNofUntyped( const std::vector<FeatureDef>& feats)
	{
		int rt = 0;
		std::vector<FeatureDef>::const_iterator fi = feats.begin(), fe = feats.end();
		for (; fi != fe; ++fi)
		{
			if (fi->typeindices.empty()) ++rt;
		}
		return rt;
	}

	std::vector<std::vector<FeatureDef> > evaluateQuery( const QueryDef& qdef) const
	{
		return findTermSequence( qdef);
	}

private:
	strus::DebugTraceContextInterface* m_debugtrace;
	std::map<std::string,int> m_typemap;
	std::map<std::string,int> m_termmap;
	std::map<std::string,int> m_unknownTermmap;
	std::multimap<int,int> m_featureStartMap;
	std::vector<std::string> m_terms;
	std::vector<std::string> m_unknownTerms;
	std::vector<FeatureDef> m_features;
	std::vector<QueryDef> m_queries;
	std::vector<std::string> m_querystrings;
};

class TestResults
{
public:
	TestResults(){}

	struct AnswerElement
	{
		std::string feat;
		std::vector<std::string> types;

		AnswerElement()
			:feat(),types(){}
		AnswerElement( const std::string& feat_, const std::vector<std::string>& types_)
			:feat(feat_),types(types_){}
		AnswerElement( const AnswerElement& o)
			:feat(o.feat),types(o.types){}

		TermSequenceDef termseq( const std::map<std::string,int>& termmap, const std::map<std::string,int>& unknownTermmap) const
		{
			char const* si = feat.c_str();
			char separatorAtStart = '\0';
			if (*si == g_delimiterSubst || *si == g_spaceSubst)
			{
				separatorAtStart = *si++;
			}
			char const* start = si;
			std::vector<TermDef> terms;

			for (; *si; ++si)
			{
				if (*si == g_delimiterSubst || *si == g_spaceSubst)
				{
					std::string subterm( start, si - start);
					std::map<std::string,int>::const_iterator mi = termmap.find( subterm);
					if (mi != termmap.end())
					{
						terms.push_back( TermDef( mi->second, *si));
					}
					else
					{
						mi = unknownTermmap.find( subterm);
						if (mi != unknownTermmap.end())
						{
							terms.push_back( TermDef( mi->second, *si));
						}
						else
						{
							throw std::runtime_error( strus::string_format( "unknown term '%s' in answer", subterm.c_str()));
						}
					}
					start = si+1;
				}
			}
			if (start < si)
			{
				std::string subterm( start, si - start);
				std::map<std::string,int>::const_iterator mi = termmap.find( subterm);
				if (mi != termmap.end())
				{
					terms.push_back( TermDef( mi->second, '\0'));
				}
				else
				{
					mi = unknownTermmap.find( subterm);
					if (mi != unknownTermmap.end())
					{
						terms.push_back( TermDef( mi->second, *si));
					}
					else
					{
						throw std::runtime_error( strus::string_format( "unknown term '%s' in answer", subterm.c_str()));
					}
				}
			}
			return TermSequenceDef( terms, separatorAtStart);
		}

		FeatureDef feature( const std::map<std::string,int>& termmap, const std::map<std::string,int>& unknownTermmap, const std::map<std::string,int>& typemap) const
		{
			std::vector<int> typeindices_;
			std::vector<std::string>::const_iterator ti = types.begin(), te = types.end();
			for (; ti != te; ++ti)
			{
				std::map<std::string,int>::const_iterator mi = typemap.find( *ti);
				if (mi != termmap.end())
				{
					typeindices_.push_back( mi->second);
				}
				else
				{
					throw std::runtime_error( strus::string_format( "unknown term type '%s' in answer", ti->c_str()));
				}
			}
			return FeatureDef( termseq( termmap, unknownTermmap), typeindices_);
		}
	};

	struct Answer
	{
	public:
		explicit Answer( int queryidx_)
			:m_queryidx(queryidx_){}
		Answer( const Answer& o)
			:m_queryidx(o.m_queryidx),m_ar(o.m_ar){}

		void addElement( std::string feat, std::vector<std::string> types)
		{
			m_ar.push_back( AnswerElement( feat, types));
		}

		int queryidx() const		{return m_queryidx;}

		typedef std::vector<AnswerElement>::const_iterator const_iterator;
		const_iterator begin() const	{return m_ar.begin();}
		const_iterator end() const	{return m_ar.end();}

		std::vector<FeatureDef> features( const std::map<std::string,int>& termmap, const std::map<std::string,int>& unknownTermmap, const std::map<std::string,int>& typemap) const
		{
			std::vector<FeatureDef> rt;
			std::vector<AnswerElement>::const_iterator ai = m_ar.begin(), ae = m_ar.end();
			for (; ai != ae; ++ai)
			{
				rt.push_back( ai->feature( termmap, unknownTermmap, typemap));
			}
			return rt;
		}

	private:
		int m_queryidx;
		std::vector<AnswerElement> m_ar;
	};

	void addAnswer( Answer& answer)
	{
		m_answers.push_back( answer);
	}

	typedef std::vector<Answer>::const_iterator const_iterator;
	const_iterator begin() const	{return m_answers.begin();}
	const_iterator end() const	{return m_answers.end();}

private:
	std::vector<Answer> m_answers;
};

void instantiateLexer( strus::SentenceLexerInstanceInterface* lexer)
{
	std::vector<int>::const_iterator di = g_delimiters.begin(), de = g_delimiters.end();
	for (; di != de; ++di)
	{
		lexer->addLink( *di, g_delimiterSubst);
	}
	di = g_spaces.begin(), de = g_spaces.end();
	for (; di != de; ++di)
	{
		lexer->addSpace( *di);
	}
	lexer->addSpace( g_spaceSubst);
	lexer->addLink( g_spaceSubst, g_spaceSubst);
	lexer->addSeparator( '"');
	lexer->addSeparator( ';');
}

void insertTestData( strus::VectorStorageClientInterface* storage, const TestData& testdata)
{
	strus::local_ptr<strus::VectorStorageTransactionInterface> transaction( storage->createTransaction());
	if (!transaction.get()) throw std::runtime_error( "failed to create vector storage transaction");
	{
		int nofFeatureDefinitions = 0;
		if (g_verbose) std::cerr << "inserting test data (feature definitions) ... " << std::endl;
		int fi = 0, fe = testdata.nofFeatures();
		for (; fi != fe; ++fi)
		{
			std::string feat = testdata.featureString( fi);
			std::vector<std::string> types = testdata.featureTypes( fi);
			std::vector<std::string>::const_iterator ti = types.begin(), te = types.end();
			for (; ti != te; ++ti)
			{
				++nofFeatureDefinitions;
				transaction->defineFeature( *ti, feat);
				if (g_verbose) std::cerr << "insert feature '" << feat << "' type " << *ti << std::endl;
			}
		}
		transaction->commit();
		if (g_verbose) std::cerr << nofFeatureDefinitions << " features inserted" << std::endl;
	}
}

void runQueries( TestResults& results, const strus::SentenceLexerInstanceInterface* lexerinst, const TestData& testdata, int testidx)
{
	strus::local_ptr<strus::DebugTraceContextInterface> debugtraceobj( g_dbgtrace && g_verbose ? g_dbgtrace->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME) : NULL);
	strus::DebugTraceContextInterface* debugtrace = debugtraceobj.get();

	int qi = 0, qe = testdata.nofQueries();
	for (; qi != qe; ++qi)
	{
		if (testidx >= 0 && qi != testidx) continue;

		std::string querystr = testdata.queryString( qi);
		if (debugtrace) debugtrace->open( "query");
		if (debugtrace) debugtrace->event( "string", "%s", querystr.c_str());

		strus::local_ptr<strus::SentenceLexerContextInterface> lexer( lexerinst->createContext( querystr));
		if (!lexer.get()) throw std::runtime_error( "failed to create lexer context");
		bool hasMore = lexer->fetchFirstSplit();
		for (; hasMore; hasMore = lexer->fetchNextSplit())
		{
			TestResults::Answer answer( qi);
			if (debugtrace) debugtrace->open( "split");
			int ti = 0, te = lexer->nofTokens();
			for (; ti != te; ++ti)
			{
				std::string feat = lexer->featureValue( ti);
				std::vector<std::string> types = lexer->featureTypes( ti);
				answer.addElement( feat, types);
				if (debugtrace)
				{
					std::string typeliststr;
					std::vector<std::string>::const_iterator yi = types.begin(), ye = types.end();
					for (int yidx=0; yi != ye; ++yi,++yidx)
					{
						if (yidx) typeliststr.append( ",");
						typeliststr.append( *yi);
					}
					debugtrace->event( "token", "%d: '%s' {%s}", ti, feat.c_str(), typeliststr.c_str());
				}
			}
			if (debugtrace) debugtrace->close();
			results.addAnswer( answer);
		}
		if (debugtrace) debugtrace->close();
	}
}

template <class TYPE>
class ResultSet
{
public:
	ResultSet( const std::vector<TYPE>& list)
		:m_elements( list.begin(), list.end()){}
	ResultSet( const ResultSet& o)
		:m_elements(o.m_elements){}
	ResultSet()
		:m_elements(){}

	typedef typename std::set<TYPE>::const_iterator const_iterator;

	bool operator < (const ResultSet& o) const		{return compare(o) < 0;}
	bool operator == (const ResultSet& o) const		{return compare(o) == 0;}
	bool operator <= (const ResultSet& o) const		{return compare(o) <= 0;}
	bool operator >= (const ResultSet& o) const		{return compare(o) >= 0;}
	bool operator > (const ResultSet& o) const		{return compare(o) > 0;}
	bool operator != (const ResultSet& o) const		{return compare(o) != 0;}

	int compare( const ResultSet& o) const
	{
		const_iterator ai = m_elements.begin(), ae = m_elements.end();
		const_iterator bi = o.m_elements.begin(), be = o.m_elements.end();
		for (; ai != ae && bi != be && *ai == *bi; ++ai,++bi){}
		if (ai == ae && bi == be) return 0;
		if (ai == ae) return -1;
		if (bi == be) return +1;
		if (*bi < *ai) return +1;
		if (*ai < *bi) return -1;
		return 0;
	}

	const_iterator begin() const	{return m_elements.begin();}
	const_iterator end() const	{return m_elements.end();}

	void insert( const TYPE& elem)
	{
		m_elements.insert( elem);
	}
	void insert( const typename std::vector<TYPE>::const_iterator& li, const typename std::vector<TYPE>::const_iterator& le)
	{
		m_elements.insert( li, le);
	}
	void erase( const TYPE& elem)
	{
		m_elements.erase( elem);
	}
	bool contains( const TYPE& elem)
	{
		return m_elements.find( elem) != m_elements.end();
	}

private:
	std::set<TYPE> m_elements;
};

static ResultSet<ResultSet<FeatureDef> > createResultSetSet( const std::vector< std::vector<FeatureDef> >& orig)
{
	// Hack to make feature definitions with different split flags equal, turn all split flags to true:
	std::vector< std::vector<FeatureDef> > fvv( orig);
	std::vector< std::vector<FeatureDef> >::iterator fvvi = fvv.begin(), fvve = fvv.end();
	for (; fvvi != fvve; ++fvvi)
	{
		std::vector<FeatureDef>::iterator fvi = fvvi->begin(), fve = fvvi->end();
		for (; fvi != fve; ++fvi)
		{
			std::vector<TermDef>::iterator ti = fvi->termseq.terms.begin(), te = fvi->termseq.terms.end();
			for (; ti != te; ++ti)
			{
				ti->split = false;
			}
		}
	}
	// create the result set:
	ResultSet<ResultSet<FeatureDef> > rt;
	typename std::vector< std::vector<FeatureDef> >::const_iterator oi = fvv.begin(), oe = fvv.end();
	for (; oi != oe; ++oi)
	{
		rt.insert( ResultSet<FeatureDef>( *oi));
	}
	// HACK: Eliminate result without separator at end if the result with separator at the end is in the set:
	oi = fvv.begin();
	for (; oi != oe; ++oi)
	{
		if (!oi->empty() && !oi->back().termseq.terms.empty())
		{
			char sepch = oi->back().termseq.terms.back().separator;
			if (sepch == g_spaceSubst || sepch == g_delimiterSubst)
			{
				std::vector<FeatureDef> fdef = *oi;
				fdef.back().termseq.terms.back().separator = '\0';
				if (rt.contains( ResultSet<FeatureDef>( fdef)))
				{
					rt.erase( ResultSet<FeatureDef>( *oi));
				}
			}
		}
	}
	return rt;
}

template <class TYPE>
static std::pair<const TYPE*,const TYPE*> getFirstDiff( const ResultSet<TYPE>& aa, const ResultSet<TYPE>& bb)
{
	typedef std::pair<const TYPE*,const TYPE*> DiffType;
	typename ResultSet<TYPE>::const_iterator ai = aa.begin(), ae = aa.end();
	typename ResultSet<TYPE>::const_iterator bi = bb.begin(), be = bb.end();
	for (; ai != ae && bi != be && *ai == *bi; ++ai,++bi){}
	if (ai == ae)
	{
		return (bi == be) ? DiffType(0,0) : DiffType(0,&*bi);
	}
	else if (bi == be)
	{
		return (ai == ae) ? DiffType(0,0) : DiffType(&*ai,0);
	}
	else
	{
		return DiffType(&*ai,&*bi);
	}
}

bool isEqualTestResultExpected( const std::vector<std::vector<FeatureDef> >& result, const std::vector< std::vector<FeatureDef> >& expected, const TestData& testdata)
{
	ResultSet< ResultSet< FeatureDef> > ee( createResultSetSet( expected));
	ResultSet< ResultSet< FeatureDef> > rr( createResultSetSet( result));

	if (rr.compare( ee) != 0)
	{
		std::pair<const ResultSet< FeatureDef>*, const ResultSet< FeatureDef>*> firstSetDiff = getFirstDiff( rr, ee);
		if (!firstSetDiff.first)
		{
			std::cerr << "expected result not found" << std::endl;
			return false;
		}
		else if (!firstSetDiff.second)
		{
			std::cerr << "unexpected result found" << std::endl;
			return false;
		}
		std::pair<const FeatureDef*, const FeatureDef*> firstElementDiff = getFirstDiff( *firstSetDiff.first, *firstSetDiff.second);
		const FeatureDef* rptr = firstElementDiff.first;
		const FeatureDef* eptr = firstElementDiff.second;
		if (rptr && eptr)
		{
			if (*eptr < *rptr)
			{
				std::string termstr = eptr->termseq.tostring( testdata.terms(), testdata.unknownTerms());
				std::string typestr = eptr->typestring();
				std::cerr << strus::string_format( "expected result '%s' {%s} not found", termstr.c_str(), typestr.c_str()) << std::endl;
				return false;
			}
			else if (*rptr < *eptr)
			{
				std::string termstr = rptr->termseq.tostring( testdata.terms(), testdata.unknownTerms());
				std::string typestr = rptr->typestring();
				std::cerr << strus::string_format( "unexpected result '%s' {%s} found", termstr.c_str(), typestr.c_str()) << std::endl;
				return false;
			}
		}
		else if (eptr)
		{
			std::string termstr = eptr->termseq.tostring( testdata.terms(), testdata.unknownTerms());
			std::string typestr = eptr->typestring();
			std::cerr << strus::string_format( "expected result '%s' {%s} not found", termstr.c_str(), typestr.c_str()) << std::endl;
			return false;
		}
		else
		{
			std::string termstr = rptr->termseq.tostring( testdata.terms(), testdata.unknownTerms());
			std::string typestr = rptr->typestring();
			std::cerr << strus::string_format( "unexpected result '%s' {%s} found", termstr.c_str(), typestr.c_str()) << std::endl;
			return false;
		}
	}
	return true;
}

void printFeatureDefs( std::ostream& out, const std::vector<std::vector<FeatureDef> >& list, const TestData& testdata)
{
	typename std::vector<std::vector<FeatureDef> >::const_iterator li = list.begin(), le = list.end();
	int lidx = 0;
	for (; li != le; ++li,++lidx)
	{
		out << strus::string_format( "result %d:", lidx) << std::endl;
		std::vector<FeatureDef>::const_iterator fi = li->begin(), fe = li->end();
		int fidx = 0;
		for (; fi != fe; ++fi,++fidx)
		{
			std::string fstr = fi->termseq.tostring( testdata.terms(), testdata.unknownTerms());
			std::string tstr = fi->typestring();
			out << strus::string_format( "\tfeature %d: '%s' %s", fidx, fstr.c_str(), tstr.c_str()) << std::endl;
		}
	}
}

void verifyTestResults( const TestResults& results, const TestData& testdata, int testidx)
{
	TestResults::const_iterator ai = results.begin(), ae = results.end();
	int qi = 0, qe = testdata.nofQueries();
	for (; qi != qe; ++qi)
	{
		if (testidx >= 0 && testidx != qi) continue;
		for (; ai != ae && ai->queryidx() < qi; ++ai){}

		const QueryDef& qdef = testdata.query( qi);
		const std::string& qstr = testdata.queryString( qi);
		std::vector<std::vector<FeatureDef> > expected = testdata.evaluateQuery( qdef);
		std::vector<std::vector<FeatureDef> > result;

		for (; ai != ae && qi == ai->queryidx(); ++ai)
		{
			result.push_back( ai->features( testdata.termmap(), testdata.unknownTermmap(), testdata.typemap()));
		}
		std::sort( expected.begin(), expected.end());
		std::sort( result.begin(), result.end());

		if (isEqualTestResultExpected( result, expected, testdata))
		{
			if (g_verbose) std::cerr << strus::string_format( "successful test %d query '%s'", qi, qstr.c_str()) << std::endl;
		}
		else
		{
			if (g_verbose)
			{
				std::cerr << strus::string_format("query '%s'", qstr.c_str()) << std::endl;
				std::cerr << "result:" << std::endl;
				if (result.empty())
				{
					std::cerr << "(none)" << std::endl;
				}
				printFeatureDefs( std::cerr, result, testdata);
				std::cerr << "expected:" << std::endl;
				if (expected.empty())
				{
					std::cerr << "(none)" << std::endl;
				}
				printFeatureDefs( std::cerr, expected, testdata);
			}
			throw std::runtime_error( strus::string_format( "error in test %d, query '%s'", qi, qstr.c_str()));
		}
	}
}

int main( int argc, const char** argv)
{
	try
	{
		int rt = 0;
		g_dbgtrace = strus::createDebugTrace_standard( 2);
		if (!g_dbgtrace) {std::cerr << "FAILED " << "strus::createDebugTrace_standard" << std::endl; return -1;}
		g_errorhnd = strus::createErrorBuffer_standard( 0, 1, g_dbgtrace);
		if (!g_errorhnd) {std::cerr << "FAILED " << "strus::createErrorBuffer_standard" << std::endl; return -1;}
		g_fileLocator = strus::createFileLocator_std( g_errorhnd);
		if (!g_fileLocator) {std::cerr << "FAILED " << "strus::createFileLocator_std" << std::endl; return -1;}

		std::string configstr( DEFAULT_CONFIG);
		std::string workdir = "./";
		bool printUsageAndExit = false;

		// Parse parameters:
		int argidx = 1;
		int testidx = -1;
		bool finished_options = false;
		while (!finished_options && argc > argidx && argv[argidx][0] == '-')
		{
			if (0==std::strcmp( argv[argidx], "-h"))
			{
				printUsageAndExit = true;
			}
			else if (0==std::strcmp( argv[argidx], "-V"))
			{
				g_verbose = true;
			}
			else if (0==std::strcmp( argv[argidx], "-T"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -T needs argument (test index)" << std::endl;
					printUsageAndExit = true;
				}
				try
				{
					testidx = strus::numstring_conv::touint( argv[++argidx], std::numeric_limits<int>::max());
				}
				catch (const std::runtime_error& err)
				{
					std::cerr << "error parsing option -T: " << err.what() << std::endl;
				}
			}
			else if (0==std::strcmp( argv[argidx], "-s"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -s needs argument (configuration string)" << std::endl;
					printUsageAndExit = true;
				}
				configstr = argv[++argidx];
			}
			else if (0==std::strcmp( argv[argidx], "-G"))
			{
				if (argidx+1 == argc)
				{
					std::cerr << "option -G needs argument (configuration string)" << std::endl;
					printUsageAndExit = true;
				}
				if (!g_dbgtrace->enable( argv[++argidx]))
				{
					std::cerr << "too many debug trace items enabled" << std::endl;
					printUsageAndExit = true;
				}
			}
			else if (0==std::strcmp( argv[argidx], "--"))
			{
				finished_options = true;
			}
			else
			{
				std::cerr << "unknown option " << argv[argidx] << std::endl;
				printUsageAndExit = true;
			}
			++argidx;
		}
		if (argc > argidx)
		{
			workdir = argv[ argidx++];
		}
		if (argc > argidx)
		{
			g_nofTypes = strus::numstring_conv::touint( argv[ argidx++], MaxNofTypes);
			if (g_nofTypes == 0) throw std::runtime_error("nof types argument is 0");
		}
		if (argc > argidx)
		{
			g_nofTerms = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<strus::Index>::max());
			if (g_nofTerms == 0) throw std::runtime_error("nof terms argument is 0");
		}
		if (argc > argidx)
		{
			g_nofFeatures = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<strus::Index>::max());
			if (g_nofFeatures == 0) throw std::runtime_error("nof features argument is 0");
		}
		if (argc > argidx)
		{
			g_maxFeatureTerms = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
			if (g_maxFeatureTerms == 0) throw std::runtime_error("max feature terms argument is 0");
		}
		if (argc > argidx)
		{
			g_maxTermLength = strus::numstring_conv::touint( argv[ argidx++], std::numeric_limits<int>::max());
			if (g_maxTermLength == 0) throw std::runtime_error("max term length argument is 0");
		}
		if (argc > argidx)
		{
			std::cerr << "too many arguments (maximum 6 expected)" << std::endl;
			rt = 1;
			printUsageAndExit = true;
		}

		if (printUsageAndExit)
		{
			std::cerr << "Usage: " << argv[0] << " [<options>] [<workdir>] [<number of types> [<number of terms> [<number of features> [<max feature terms> [<max term length>]]]]]" << std::endl;
			std::cerr << "options:" << std::endl;
			std::cerr << "-h                     : print this usage" << std::endl;
			std::cerr << "-V                     : verbose output to stderr" << std::endl;
			std::cerr << "-s <CONFIG>            :specify test configuration string as <CONFIG>" << std::endl;
			std::cerr << "-T <QIDX>              :evaluate only test case with index <QIDX> starting with 0" << std::endl;
			std::cerr << "-G <DEBUG>             :enable debug trace for <DEBUG>" << std::endl;
			std::cerr << "<workdir>              :working directory, default './'" << std::endl;
			std::cerr << "<number of types>      :number of types, default 2" << std::endl;
			std::cerr << "<number of terms>      :number of terms, default 1000" << std::endl;
			std::cerr << "<number of features>   :number of features, default 10000" << std::endl;
			std::cerr << "<max feature terms>    :maximum number of terms in a feature, default 5" << std::endl;
			std::cerr << "<max term length>      :maximum length of a term, default 30" << std::endl;
			return rt;
		}
		if (g_errorhnd->hasError()) throw std::runtime_error("error in test configuration");

		if (g_verbose)
		{
			std::cerr << "model config: " << configstr << std::endl;
			std::cerr << "number of types: " << g_nofTypes  << std::endl;
			std::cerr << "number of terms: " << g_nofTerms  << std::endl;
			std::cerr << "number of features: " << g_nofFeatures  << std::endl;
			std::cerr << "maximum number of feature terms: " << g_maxFeatureTerms  << std::endl;
			std::cerr << "maximum length of a term: " << g_maxTermLength  << std::endl;
			std::cerr << "random seed: " << g_random.seed() << std::endl;
		}
		if (testidx >= 0 && testidx >= g_nofFeatures)
		{
			throw std::runtime_error( "test index selected with option -T is out of range");
		}
		// Build all objects:
		strus::local_ptr<strus::DatabaseInterface> dbi( strus::createDatabaseType_leveldb( g_fileLocator, g_errorhnd));
		strus::local_ptr<strus::VectorStorageInterface> sti( strus::createVectorStorage_std( g_fileLocator, g_errorhnd));
		if (!dbi.get() || !sti.get() || g_errorhnd->hasError()) throw std::runtime_error( g_errorhnd->fetchError());

		// Remove traces of old test model before creating a new one:
		if (g_verbose) std::cerr << "create test repository ..." << std::endl;
		if (!dbi->destroyDatabase( configstr))
		{
			(void)g_errorhnd->fetchError();
		}
		// Creating the vector storage:
		if (!sti->createStorage( configstr, dbi.get()))
		{
			throw std::runtime_error( g_errorhnd->fetchError());
		}
		strus::local_ptr<strus::VectorStorageClientInterface> storage( sti->createClient( configstr, dbi.get()));
		if (!storage.get()) throw std::runtime_error( g_errorhnd->fetchError());

		// Insert the feature definitions:
		if (g_verbose) std::cerr << "create test data ..." << std::endl;
		TestData testData;
		if (g_verbose) std::cerr << "insert test data ..." << std::endl;
		insertTestData( storage.get(), testData);

		// Creating the lexer:
		strus::local_ptr<strus::SentenceLexerInstanceInterface> lexer( storage->createSentenceLexer());
		if (!lexer.get()) throw std::runtime_error( "failed to create sentence lexer of vector storage");
		instantiateLexer( lexer.get());

		// Run the tests:
		if (g_verbose) std::cerr << "run queries..." << std::endl;
		TestResults testResults;
		runQueries( testResults, lexer.get(), testData, testidx);
		if (g_verbose) std::cerr << "verify results ..." << std::endl;
		verifyTestResults( testResults, testData, testidx);

		// Debug output dump:
		if (!strus::dumpDebugTrace( g_dbgtrace, NULL/*filename (stderr)*/))
		{
			throw std::runtime_error( "failed to dump the debug trace");
		}
		std::cerr << "done" << std::endl;

		delete g_fileLocator;
		delete g_errorhnd;
		return 0;
	}
	catch (const std::runtime_error& err)
	{
		if (g_dbgtrace) (void)strus::dumpDebugTrace( g_dbgtrace, NULL/*filename (stderr)*/);

		std::string msg;
		if (g_errorhnd && g_errorhnd->hasError())
		{
			msg.append( " (");
			msg.append( g_errorhnd->fetchError());
			msg.append( ")");
		}
		std::cerr << "error: " << err.what() << msg << std::endl;

		delete g_fileLocator;
		delete g_errorhnd;
		return 1;
	}
	catch (const std::bad_alloc& )
	{
		std::cerr << "out of memory" << std::endl;

		delete g_fileLocator;
		delete g_errorhnd;
		return 2;
	}
	catch (const std::logic_error& err)
	{
		std::string msg;
		if (g_errorhnd && g_errorhnd->hasError())
		{
			msg.append( " (");
			msg.append( g_errorhnd->fetchError());
			msg.append( ")");
		}
		std::cerr << "error: " << err.what() << msg << std::endl;

		delete g_fileLocator;
		delete g_errorhnd;
		return 3;
	}
}



