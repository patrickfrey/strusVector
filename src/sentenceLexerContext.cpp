/*
 * Copyright (c) 2019 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Context for parsing a sentence
/// \file sentenceLexerContext.cpp
#include "sentenceLexerContext.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/debugTraceInterface.hpp"
#include "strus/base/utf8.hpp"
#include "strus/base/string_format.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include <algorithm>
#include <set>
#include <map>

using namespace strus;

#define MODULENAME "sentence lexer context (vector)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"

typedef SentenceLexerInstance::SeparatorDef SeparatorDef;
typedef SentenceLexerInstance::LinkDef LinkDef;
typedef SentenceLexerContext::AlternativeSplit AlternativeSplit;

struct LinkSplit
{
	int splitpos;
	char substchr;

	LinkSplit( int splitpos_, char substchr_)
		:splitpos(splitpos_),substchr(substchr_){}
	LinkSplit( const LinkSplit& o)
		:splitpos(o.splitpos),substchr(o.substchr){}

	bool operator<( const LinkSplit& o) const
	{
		return splitpos == o.splitpos
			? (unsigned char)substchr > (unsigned char)o.substchr
			: splitpos < o.splitpos;
	}
};

struct DelimiterDef
{
	int pos;
	char substchr;

	DelimiterDef( int pos_, char substchr_)
		:pos(pos_),substchr(substchr_){}
	DelimiterDef( const DelimiterDef& o)
		:pos(o.pos),substchr(o.substchr){}
};

struct NormalizedField
{
	std::string key;
	std::vector<DelimiterDef> delimiters;

	NormalizedField()
		:key(),delimiters(){}
	NormalizedField( const std::string& key_, const std::vector<DelimiterDef>& delimiters_)
		:key(key_),delimiters(delimiters_){}
	NormalizedField( const NormalizedField& o)
		:key(o.key),delimiters(o.delimiters){}
#if __cplusplus >= 201103L
	NormalizedField( NormalizedField&& o) :key(std::move(o.key)),delimiters(std::move(o.delimiters)){}
	NormalizedField& operator=( NormalizedField&& o) {key=std::move(o.key); delimiters=std::move(o.delimiters); return *this;}
#endif

	bool defined() const
	{
		return !key.empty();
	}
};

static NormalizedField normalizeField( const std::string& source, const std::vector<LinkDef>& linkDefs);
static std::vector<std::string> getFields( const std::string& source, const std::vector<SeparatorDef>& separatorDefs);
static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const std::vector<NormalizedField>& fields, const std::vector<char>& linkChars, DebugTraceContextInterface* debugtrace);
static std::string typeListString( const std::vector<std::string>& types, const char* sep);

#define SEPARATOR_CHR_SPLIT '\1'


SentenceLexerContext::SentenceLexerContext(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_,
		const std::vector<SeparatorDef>& separators,
		const std::vector<LinkDef>& linkDefs,
		const std::vector<char>& linkChars,
		const std::string& source,
		ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_vstorage(vstorage_),m_database(database_),m_splits(),m_splitidx(-1)
{
	DebugTraceInterface* dbgi = m_errorhnd->debugTrace();
	if (dbgi) m_debugtrace = dbgi->createTraceContext( STRUS_DBGTRACE_COMPONENT_NAME);

	std::vector<std::string> fields = getFields( source, separators);
	std::vector<NormalizedField> normalizedFields;
	std::vector<std::string>::const_iterator fi = fields.begin(), fe = fields.end();
	for (; fi != fe; ++fi)
	{
		normalizedFields.push_back( normalizeField( *fi, linkDefs));
	}
	m_splits = getAlternativeSplits( vstorage_, database_, normalizedFields, linkChars, m_debugtrace);

	if (m_debugtrace)
	{
		m_debugtrace->event( "sentence", "'%s'", source.c_str());
		m_debugtrace->open( "split");
		fi = fields.begin(), fe = fields.end();
		for (; fi != fe; ++fi)
		{
			m_debugtrace->event( "field", "content '%s'", fi->c_str());
		}
		m_debugtrace->close();
		m_debugtrace->open( "normalized");
		std::vector<NormalizedField>::const_iterator ni = normalizedFields.begin(), ne = normalizedFields.end();
		for (; ni != ne; ++ni)
		{
			m_debugtrace->open( "field");
			std::vector<DelimiterDef>::const_iterator di = ni->delimiters.begin(), de = ni->delimiters.end();
			int pi = 0;
			for (; di != de; ++di)
			{
				if (di->pos > pi)
				{
					std::string tok( ni->key.c_str() + pi, di->pos - pi);
					m_debugtrace->event( "token", "'%s'", tok.c_str());
				}
				m_debugtrace->event( "delim", "pos  %d, substitution char '%c'", di->pos, di->substchr);
				pi = di->pos + 1;
			}
			if ((int)ni->key.size() > pi)
			{
				std::string tok( ni->key.c_str() + pi, ni->key.size() - pi);
				m_debugtrace->event( "token", "'%s'", tok.c_str());
			}
			m_debugtrace->close();
		}
		m_debugtrace->close();
		m_debugtrace->open("alternatives");
		std::vector<AlternativeSplit>::const_iterator ai = m_splits.begin(), ae = m_splits.end();
		for (; ai != ae; ++ai)
		{
			typedef AlternativeSplit::Element Element;
			std::string elemlist;
			std::vector<Element>::const_iterator ei = ai->ar.begin(), ee = ai->ar.end();
			for (; ei != ee; ++ei)
			{
				std::string typelist = typeListString( ei->types, ",");
				if (!elemlist.empty()) elemlist.append( ", ");
				elemlist.append( strus::string_format( "'%s' {%s}", ei->feat.c_str(), typelist.c_str()));
			}
			m_debugtrace->event( "query", "%s", elemlist.c_str());
		}
		m_debugtrace->close();
	}
}

SentenceLexerContext::~SentenceLexerContext()
{
	if (m_debugtrace) delete m_debugtrace;
}

static std::string typeListString( const std::vector<std::string>& types, const char* sep)
{
	std::string rt;
	std::vector<std::string>::const_iterator ti = types.begin(), te = types.end();
	for (; ti != te; ++ti)
	{
		if (!rt.empty()) rt.append( sep);
		rt.append( *ti);
	}
	return rt;
}

static std::vector<std::string> getFields( const std::string& source, const std::vector<SeparatorDef>& separatorDefs)
{
	std::vector<std::string> rt;
	std::set<int> split;
	std::vector<SeparatorDef>::const_iterator pi = separatorDefs.begin(), pe = separatorDefs.end();
	for (; pi != pe; ++pi)
	{
		char const* si = std::strstr( source.c_str(), pi->uchr);
		for (; si; si = std::strstr( si + strus::utf8charlen( *si), pi->uchr))
		{
			int splitpos = si - source.c_str();
			split.insert( splitpos);
		}
	}
	int pos = 0;
	std::string key;
	std::set<int>::const_iterator si = split.begin(), se = split.end();
	for (; si != se; ++si)
	{
		int chrlen = strus::utf8charlen( source[ *si]);
		if (pos < *si)
		{
			rt.push_back( std::string( source.c_str() + pos, *si - pos));
		}
		pos = *si + chrlen;
	}
	if (pos < (int)source.size())
	{
		rt.push_back( std::string( source.c_str() + pos, source.size() - pos));
	}
	return rt;
}

static NormalizedField normalizeField( const std::string& source, const std::vector<LinkDef>& linkDefs)
{
	NormalizedField rt;
	std::set<LinkSplit> split;
	std::vector<LinkDef>::const_iterator pi = linkDefs.begin(), pe = linkDefs.end();
	for (; pi != pe; ++pi)
	{
		char const* si = std::strstr( source.c_str(), pi->uchr);
		for (; si; si = std::strstr( si + strus::utf8charlen( *si), pi->uchr))
		{
			int splitpos = si - source.c_str();
			split.insert( LinkSplit( splitpos, pi->substchr));
		}
	}
	int pos = 0;
	std::string key;
	std::set<LinkSplit>::const_iterator si = split.begin(), se = split.end();
	while (si != se && si->splitpos == pos && si->substchr == ' ')
	{
		pos += strus::utf8charlen( source[ si->splitpos]);
		for (++si; si != se && si->splitpos < pos; ++si){}
	}
	split.erase( split.begin(), si);

	si = split.begin(), se = split.end();
	while (si != se)
	{
		int splitpos = si->splitpos;
		int nextpos = splitpos + strus::utf8charlen( source[ splitpos]);
		rt.key.append( source.c_str() + pos, splitpos - pos);
		bool isSpaceSplit = si->substchr == ' ';
		char substchr = si->substchr;
		for (++si; si != se && si->splitpos <= nextpos; ++si)
		{
			if (si->splitpos > splitpos)
			{
				splitpos = si->splitpos;
				nextpos = splitpos + strus::utf8charlen( source[ splitpos]);
			}
			if (si->substchr != ' ')
			{
				if (isSpaceSplit)
				{
					isSpaceSplit = false;
					substchr = si->substchr;
				}
				else
				{
					substchr = ' ';
				}
			}
		}
		rt.delimiters.push_back( DelimiterDef( rt.key.size(), substchr));
		rt.key.push_back( SEPARATOR_CHR_SPLIT);
		pos = nextpos;
	}
	rt.key.append( source.c_str() + pos, source.size() - pos);
	if (!rt.key.empty() && rt.key[ rt.key.size()-1] == SEPARATOR_CHR_SPLIT && rt.delimiters.back().substchr == ' ')
	{
		rt.key.resize( rt.key.size()-1);
		rt.delimiters.pop_back();
	}
	return rt;
}

struct KeyCandidate
{
	std::size_t keylen;	///< lenght of key in bytes
	int editref;		///< reference to edit list
	int editpos;		///< position of edits
	int followpos;		///< following token position

	explicit KeyCandidate( std::size_t keylen_)
		:keylen(keylen_),editref(-1),editpos(-1),followpos(-1){}
	KeyCandidate( std::size_t keylen_, int editref_, int editpos_, int followpos_)
		:keylen(keylen_),editref(editref_),editpos(editpos_),followpos(followpos_){}
	KeyCandidate( const KeyCandidate& o)
		:keylen(o.keylen),editref(o.editref),editpos(o.editpos),followpos(o.followpos){}
};

static void collectFollowKeyCandidates( std::vector<KeyCandidate>& candidates, std::string& edits, const NormalizedField& field, const std::vector<char>& linkChars)
{
	std::vector<DelimiterDef>::const_iterator di = field.delimiters.begin(), de = field.delimiters.end();
	for (; di != de; ++di)
	{
		candidates.push_back( KeyCandidate( di->pos/*keylen*/, -1/*no edits*/, -1/*no edit pos*/, di->pos+1));
		edits.push_back( '\0');
		int editref = edits.size();
		if (di->substchr == ' ')
		{
			std::vector<char>::const_iterator li = linkChars.begin(), le = linkChars.end();
			for (; li != le; ++li)
			{
				if (*li != ' ')
				{
					edits.push_back( *li);
				}
			}
		}
		else
		{
			edits.push_back( di->substchr);
		}

		candidates.push_back( KeyCandidate( di->pos+1/*keylen*/, editref, di->pos/*editpos*/, di->pos+1/*followpos*/));
	}
	candidates.push_back( KeyCandidate( field.key.size()));
}

class KeyIterator
{
public:
	KeyIterator( const DatabaseClientInterface* database, const NormalizedField& field_, const std::vector<char>& linkChars_)
		:m_field(field_),m_edits(),m_candidates(),m_states(),m_candidateidx(0),m_keyfound(),m_linkChars(linkChars_),m_cursor(database)
	{
		collectFollowKeyCandidates( m_candidates, m_edits, m_field, m_linkChars);
		m_states.assign( m_candidates.size(), 0);
	}

	KeyIterator( const KeyIterator& o)
		:m_field(o.m_field),m_candidates(o.m_candidates),m_candidateidx(o.m_candidateidx),m_keyfound(o.m_keyfound),m_linkChars(o.m_linkChars),m_cursor(o.m_cursor){}

	std::vector<std::string> keys( int maxsize)
	{
		std::vector<std::string> rt;
		int idx = 0;
		for (; idx < maxsize && editState( idx); ++idx)
		{
			rt.push_back( std::string( m_field.key.c_str(), m_candidates[ idx].keylen));
		}
		return rt;
	}

	bool first()
	{
		return editState( 0) && skipToNextKey();
	}

	bool next()
	{
		return editState( m_candidateidx+1) && skipToNextKey();
	}

	const std::string& getKey() const
	{
		return m_keyfound;
	}

	std::string defaultKey() const
	{
		return m_candidates.empty() ? std::string() : std::string( m_field.key.c_str(), m_candidates[0].keylen);
	}
	int defaultPosIncr() const
	{
		return m_candidates.empty() ? 0 : m_candidates[ 0].followpos;
	}

	int followPosIncr() const
	{
		return m_candidateidx >= m_candidates.size() ? 0 : m_candidates[ m_candidateidx].followpos;
	}

	NormalizedField followField( int posincr) const
	{
		if (posincr == -1 || posincr >= (int)m_field.key.size())
		{
			return NormalizedField();
		}
		else
		{
			std::vector<DelimiterDef> delim;
			std::vector<DelimiterDef>::const_iterator di = m_field.delimiters.begin(), de = m_field.delimiters.end();
			for (; di != de && di->pos < posincr; ++di){}
			for (; di != de; ++di)
			{
				delim.push_back( DelimiterDef( di->pos - posincr, di->substchr));
			}
			return NormalizedField( std::string( m_field.key.c_str() + posincr), delim);
		}
	}

private:
	bool editState( std::size_t idx)
	{
		if (idx >= m_candidates.size()) return false;
		if (idx == 0 || idx == m_candidateidx + 1 || idx == m_candidateidx)
		{
			m_candidateidx = idx;
			const KeyCandidate& cd = m_candidates[ m_candidateidx];
			if (cd.editref >= 0)
			{
				int ei = m_states[ m_candidateidx];
				const char* edit = m_edits.c_str() + cd.editref + ei;
				if (*edit)
				{
					m_field.key[ cd.editpos] = *edit;
				}
				else
				{
					return false;
				}
			}
			return true;
		}
		else
		{
			throw strus::runtime_error(_TXT("illegal state switch (%d -> %d) in key interator"), (int)m_candidateidx, (int)idx);
		}
	}

	static int findLinkChar( const std::vector<char>& linkChars, char chr)
	{
		std::vector<char>::const_iterator li = std::find( linkChars.begin(), linkChars.end(), chr);
		return li == linkChars.end() ? 0 : (li - linkChars.begin() + 1);
	}


	bool checkKeyFound( const char* keyptr, std::size_t keylen)
	{
		if (m_keyfound.size() == keylen)
		{
			return true;
		}
		else if (m_keyfound.size() == keylen+1 && 0<=findLinkChar( m_linkChars, m_keyfound[keylen]))
		{
			return true;
		}
		return false;
	}

	bool backtrackStateUpperBound( int ci)
	{
		m_candidateidx = ci;
		for (; ci >= 0; --ci)
		{
			const KeyCandidate& cd = m_candidates[ ci];
			int ei = m_states[ ci];
			if (cd.editref < 0) continue;
			if (m_edits[ cd.editref+ei] && m_edits[ cd.editref+ei+1])
			{
				m_states[ ci] = ei + 1;
				m_candidateidx = ci;
				editState( ci);
				return true;
			}
			else
			{
				m_states[ ci] = 0;
				continue;
			}
		}
		return false;
	}

	enum AdjustStateResult {AdjustStateMis,AdjustStateUpperBound,AdjustStateMatch};
	/// \brief Set the state as first element in the upper bound of the search key
	AdjustStateResult adjustStateUpperBound()
	{
		int ci = m_candidateidx, ce = m_candidates.size();
		int pos = 0;
		for (; ci < ce; ++ci)
		{
			const KeyCandidate& cd = m_candidates[ ci];
			int newpos = (cd.editpos < 0) ? cd.keylen : cd.editpos;
			if (newpos > (int)m_keyfound.size())
			{
				int cmp = std::memcmp( m_keyfound.c_str()+pos, m_field.key.c_str()+pos, m_keyfound.size() - pos);
				if (cmp < 0)
				{
					return editState( ci) ? AdjustStateUpperBound : AdjustStateMis;
				}
				else if (cmp > 0)
				{
					return backtrackStateUpperBound( ci) ? AdjustStateUpperBound : AdjustStateMis;
				}
				else if (newpos >= (int)m_keyfound.size())
				{
					return editState( ci) ? AdjustStateUpperBound : AdjustStateMis;
				}
				else
				{
					return backtrackStateUpperBound( ci) ? AdjustStateUpperBound : AdjustStateMis;
				}
			}
			else
			{
				int cmp = std::memcmp( m_keyfound.c_str()+pos, m_field.key.c_str()+pos, newpos - pos);
				if (cmp < 0)
				{
					return editState( ci) ? AdjustStateUpperBound : AdjustStateMis;
				}
				else if (cmp > 0)
				{
					return backtrackStateUpperBound( ci) ? AdjustStateUpperBound : AdjustStateMis;
				}
				else if (cd.editref >= 0)
				{
					char const* edit = m_edits.c_str() + cd.editref;
					int ei=0;
					for (;edit[ei] && edit[ei] < m_keyfound[ cd.editpos]; ++ei){}
					if (edit[ei])
					{
						bool foundUpperBound = edit[ei] > m_keyfound[ cd.editpos];
						m_states[ ci] = ei - cd.editref;
						m_field.key[ cd.editpos] = edit[ei];
						m_candidateidx = ci;
						if (foundUpperBound) return AdjustStateUpperBound;
						pos = cd.editpos+1;
					}
					else
					{
						return backtrackStateUpperBound( ci) ? AdjustStateUpperBound : AdjustStateMis;
					}
				}
				else if (checkKeyFound( m_field.key.c_str(), cd.keylen))
				{
					m_candidateidx = ci;
					return AdjustStateMatch;
				}
			}
		}
		m_candidateidx = m_candidates.size();
		return AdjustStateMis;
	}

	bool skipToNextKey()
	{
		/// Algorithm: Start with the first key.
		/// We search an upperbound with the key as prefix.
		/// With the upperbound found try to find upperbound candidate for the found key by performing edit operations on the search key.
		/// If the upperbound gets equal to the found key, we are finished (we have the next key).
		/// If the upperbound is bigger than the found key, we start the search again with this upperbound key as candidate.
		while (m_candidateidx < m_candidates.size())
		{
			char const* keyptr = m_field.key.c_str();
			std::size_t keylen = m_candidates[ m_candidateidx].keylen;
			if (m_cursor.skipPrefix( keyptr, keylen, m_keyfound))
			{
				if (checkKeyFound( keyptr, keylen))
				{
					return true;
				}
				else switch (adjustStateUpperBound())
				{
					case AdjustStateMis:
						return false;
					case AdjustStateUpperBound:
						continue;
					case AdjustStateMatch:
						return true;
				}
			}
			else if (!backtrackStateUpperBound( m_candidateidx))
			{
				return false;
			}
		}
		m_keyfound.clear();
		return false;
	}

private:
	NormalizedField m_field;
	std::string m_edits;
	std::vector<KeyCandidate> m_candidates;
	std::vector<int> m_states;
	std::size_t m_candidateidx;
	std::string m_keyfound;
	std::vector<char> m_linkChars;
	DatabaseAdapter::FeatureCursor m_cursor;
};



struct AlternativeSplitElementRef
{
	std::size_t eidx;
	int next;

	explicit AlternativeSplitElementRef( std::size_t eidx_, int next_=-1)
		:eidx(eidx_),next(next_){}
	AlternativeSplitElementRef( const AlternativeSplitElementRef& o)
		:eidx(o.eidx),next(o.next){}
};

struct KeyIteratorPath
{
public:
	explicit KeyIteratorPath( const NormalizedField& field_)
		:field(field_),eref(-1){}
	KeyIteratorPath( const NormalizedField& field_, int eref_)
		:field(field_),eref(eref_){}
	KeyIteratorPath( const KeyIteratorPath& o)
		:field(o.field),eref(o.eref){}
#if __cplusplus >= 201103L
	KeyIteratorPath( KeyIteratorPath&& o) :field(std::move(o.field)),eref(o.eref){}
	KeyIteratorPath& operator=( KeyIteratorPath&& o) {field=std::move(o.field);eref=o.eref; return *this;}
#endif
	NormalizedField field;
	int eref;
};

struct KeyIteratorPathRef
{
public:
	std::size_t index;
	int nofUntyped;
	int nofFeatures;
	int pos;

	KeyIteratorPathRef()
		:index(0),nofUntyped(0),nofFeatures(0),pos(0){}
	KeyIteratorPathRef( std::size_t index_, int nofUntyped_, int nofFeatures_, int pos_)
		:index(index_),nofUntyped(nofUntyped_),nofFeatures(nofFeatures_),pos(pos_){}
	KeyIteratorPathRef( const KeyIteratorPathRef& o)
		:index(o.index),nofUntyped(o.nofUntyped),nofFeatures(o.nofFeatures),pos(o.pos){}
	KeyIteratorPathRef& operator = ( const KeyIteratorPathRef& o)
		{index=o.index; nofUntyped=o.nofUntyped; nofFeatures = o.nofFeatures; pos=o.pos; return *this;}

	bool operator < (const KeyIteratorPathRef& o) const
	{
		return pos == o.pos
			? (nofUntyped == o.nofUntyped
				? (nofFeatures == o.nofFeatures
					? index < o.index
					: nofFeatures < o.nofFeatures)
				: nofUntyped < o.nofUntyped)
			: pos > o.pos;
	}
};

struct AlternativeSplitQueue
{
public:
	explicit AlternativeSplitQueue( const NormalizedField& origfield)
		:m_results(),m_paths(),m_pathqueue(),m_top(),m_elements(),m_elementlists(),m_pos2minNofFeaturesMap()
		,m_minNofUntyped(std::numeric_limits<int>::max())
		,m_minNofFeatures(std::numeric_limits<int>::max())
	{
		m_paths.push_back( KeyIteratorPath( origfield));
		m_pathqueue.insert( KeyIteratorPathRef());
	}

	const NormalizedField* nextField()
	{
		while (!m_pathqueue.empty())
		{
			std::set<KeyIteratorPathRef>::iterator pqitr = m_pathqueue.begin();
			if (pqitr->nofUntyped > m_minNofUntyped)
			{
				m_pathqueue.erase( pqitr);
				continue;
			}
			m_top = *pqitr;
			m_pathqueue.erase( pqitr);
			return &m_paths[ m_top.index].field;
		}
		return NULL;
	}

	void push( const NormalizedField& followfield, const AlternativeSplit::Element& element, int followPosIncr)
	{
		int nofUntyped = m_top.nofUntyped + (element.types.empty() ? 1:0);
		int nofFeatures = m_top.nofFeatures + 1;

		if (followfield.defined())
		{
			int expectedNofFeatures = nofFeatures;
			int nextpos = m_top.pos + followPosIncr;
			std::pair<Pos2minNofFeaturesMap::iterator,bool> ins =
				m_pos2minNofFeaturesMap.insert( Pos2minNofFeaturesMap::value_type( nextpos, VisitCount( nofFeatures, 0)));
			if (ins.second == false)
			{
				Pos2minNofFeaturesMap::iterator itr = ins.first;
				if (nofFeatures < itr->second.nofFeatures)
				{
					itr->second.nofFeatures = nofFeatures;
					expectedNofFeatures = m_minNofFeatures;
				}
				else
				{
					if (++itr->second.cnt >= SentenceLexerContext::MaxPositionVisits)
					{
						expectedNofFeatures = std::numeric_limits<int>::max();
						/// ... rule out this path, because we visited it already a MaxPositionVisits times without improvement
					}
					else if (m_minNofFeatures < std::numeric_limits<int>::max())
					{
						expectedNofFeatures = nofFeatures + m_minNofFeatures - ins.second;
					}
				}
			}
			if (m_minNofFeatures == std::numeric_limits<int>::max() || expectedNofFeatures <= SentenceLexerContext::maxFeaturePrunning( m_minNofFeatures))
			{
				int eref = m_elementlists.size();
				m_elementlists.push_back( AlternativeSplitElementRef( m_elements.size(), m_paths[ m_top.index].eref));
				m_elements.push_back( element);
	
				KeyIteratorPath follow( followfield, eref);
				m_pathqueue.insert( KeyIteratorPathRef( m_paths.size()/*index*/, nofUntyped, nofFeatures, nextpos));
				m_paths.push_back( follow);
			}
		}
		else
		{
			if (nofUntyped < m_minNofUntyped)
			{
				m_minNofUntyped = nofUntyped;
			}
			if (nofFeatures < m_minNofFeatures)
			{
				m_minNofFeatures = nofFeatures;
			}
			int eitr = m_paths[ m_top.index].eref;
			AlternativeSplit split;
			split.ar.push_back( element);

			for (;eitr >= 0; eitr = m_elementlists[ eitr].next)
			{
				split.ar.push_back( m_elements[ m_elementlists[ eitr].eidx]);
			}
			std::reverse( split.ar.begin(), split.ar.end());
			m_results.push_back( split);
		}
	}

	const std::vector<AlternativeSplit>& results() const
	{
		return m_results;
	}

private:
	std::vector<AlternativeSplit> m_results;
	std::vector<KeyIteratorPath> m_paths;
	std::set<KeyIteratorPathRef> m_pathqueue;
	KeyIteratorPathRef m_top;
	std::vector<AlternativeSplit::Element> m_elements;
	std::vector<AlternativeSplitElementRef> m_elementlists;
	struct VisitCount
	{
		int nofFeatures;
		int cnt;
		
		VisitCount( int nofFeatures_, int cnt_)
			:nofFeatures(nofFeatures_),cnt(cnt_){}
		VisitCount( const VisitCount& o)
			:nofFeatures(o.nofFeatures),cnt(o.cnt){}
	};
	typedef std::map<int,VisitCount> Pos2minNofFeaturesMap;
	Pos2minNofFeaturesMap m_pos2minNofFeaturesMap;
	int m_minNofUntyped;
	int m_minNofFeatures;
};

static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const NormalizedField& origfield, const std::vector<char>& linkChars, DebugTraceContextInterface* debugtrace)
{
	AlternativeSplitQueue queue( origfield);

	const NormalizedField* field = queue.nextField();
	for (; field; field=queue.nextField())
	{
		if (debugtrace) debugtrace->open( "field");
		KeyIterator kitr( database, *field, linkChars);
		if (debugtrace)
		{
			debugtrace->event( "key", "%s", field->key.c_str());
			std::vector<std::string> cd = kitr.keys( 100);
			std::vector<std::string>::const_iterator ci = cd.begin(), ce = cd.end();
			for (; ci != ce; ++ci)
			{
				debugtrace->event( "candidate", "%s", ci->c_str());
			}
		}
		bool hasMore = kitr.first();
		std::string defaultKey = kitr.defaultKey();
		bool useDefault = !defaultKey.empty();

		for (;hasMore; hasMore = kitr.next())
		{
			AlternativeSplit::Element element( kitr.getKey(), vstorage->featureTypes( kitr.getKey()));
			if (debugtrace)
			{
				std::string typeliststr = typeListString( element.types, ",");
				debugtrace->event( "element", "%s  {%s}", element.feat.c_str(), typeliststr.c_str());
			}
			int fpos = kitr.followPosIncr();
			queue.push( kitr.followField( fpos), element, fpos);
			if (element.feat == defaultKey) useDefault = false;
		}
		if (useDefault)
		{
			AlternativeSplit::Element element( defaultKey, std::vector<std::string>());
			int fpos = kitr.defaultPosIncr();
			
			queue.push( kitr.followField( fpos), element, fpos);
			if (debugtrace)
			{
				debugtrace->event( "default", "%s", defaultKey.c_str());
			}
		}
		if (debugtrace) debugtrace->close();
	}
	return queue.results();
}

static void joinAlternativeSplits( std::vector<AlternativeSplit>& split, const std::vector<AlternativeSplit>& add)
{
	if (add.empty()) return;
	if (split.empty())
	{
		split = add;
	}
	else
	{
		std::size_t si = 0, se = split.size();
		std::size_t ai = 0, ae = add.size();
		for (; ai != ae-1; ++ai)
		{
			for (si=0; si != se; ++si)
			{
				split.push_back( split[ si]);
			}
		}
		for (si=0; si != se; ++si)
		{
			for (ai=0; ai != ae; ++ai)
			{
				std::vector<AlternativeSplit::Element>& ar =  split[ si + ai * se].ar;
				ar.insert( ar.end(), add[ ai].ar.begin(), add[ ai].ar.begin());
			}
		}
	}
}

static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const std::vector<NormalizedField>& fields, const std::vector<char>& linkChars, DebugTraceContextInterface* debugtrace)
{
	if (fields.empty()) return std::vector<AlternativeSplit>();
	std::vector<NormalizedField>::const_iterator fi = fields.begin(), fe = fields.end();
	std::vector<AlternativeSplit> rt = getAlternativeSplits( vstorage, database, *fi, linkChars, debugtrace);

	for (++fi; fi != fe; ++fi)
	{
		std::vector<AlternativeSplit> nextsplits = getAlternativeSplits( vstorage, database, *fi, linkChars, debugtrace);
		joinAlternativeSplits( rt, nextsplits);
	}
	return rt;
}

std::string SentenceLexerContext::featureValue( int idx)
{
	try
	{
		if (m_splitidx >= 0 && m_splitidx < (int)m_splits.size()) return m_splits[ m_splitidx].ar[ idx].feat;
		throw std::runtime_error(_TXT("array bound read"));
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' getting feature value in current split: %s"), MODULENAME, *m_errorhnd, std::string());
}

std::vector<std::string> SentenceLexerContext::featureTypes( int idx)
{
	try
	{
		if (m_splitidx >= 0 && m_splitidx < (int)m_splits.size()) return m_splits[ m_splitidx].ar[ idx].types;
		throw std::runtime_error(_TXT("array bound read"));
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' getting list of alternative feature types in current split: %s"), MODULENAME, *m_errorhnd, std::vector<std::string>());
}

bool SentenceLexerContext::fetchFirstSplit()
{
	m_splitidx = 0;
	return m_splitidx < (int)m_splits.size();
}

bool SentenceLexerContext::fetchNextSplit()
{
	if (++m_splitidx < (int)m_splits.size())
	{
		return true;
	}
	else
	{
		--m_splitidx;
		return false;
	}
}

int SentenceLexerContext::nofTokens() const
{
	if (m_splitidx >= 0 && m_splitidx < (int)m_splits.size()) return m_splits[ m_splitidx].ar.size();
	return 0;
}


