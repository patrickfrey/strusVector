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
#include "strus/base/minimalCover.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"
#include <algorithm>
#include <limits>
#include <set>
#include <map>

using namespace strus;

#define MODULENAME "sentence lexer context (vector)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"
#define SEPARATOR_CHR_SPLIT '\1'

typedef SentenceLexerInstance::SeparatorDef SeparatorDef;
typedef SentenceLexerInstance::LinkDef LinkDef;
typedef SentenceLexerContext::AlternativeSplit AlternativeSplit;

struct LinkSplit
{
	int splitpos;
	char substchr;
	bool mapsToItself;

	LinkSplit( int splitpos_, char substchr_, bool mapsToItself_)
		:splitpos(splitpos_),substchr(substchr_),mapsToItself(mapsToItself_){}
	LinkSplit( const LinkSplit& o)
		:splitpos(o.splitpos),substchr(o.substchr),mapsToItself(o.mapsToItself){}

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
	std::string tostring( char sep) const
	{
		std::string rt( key);
		std::string::iterator ri = rt.begin(), re = rt.end();
		for (; ri != re; ++ri)
		{
			if (*ri == SEPARATOR_CHR_SPLIT)
			{
				*ri = sep;
			}
		}
		return rt;
	}
};

static NormalizedField normalizeField( const std::string& source, const std::vector<LinkDef>& linkDefs);
static std::vector<std::string> getFields( const std::string& source, const std::vector<SeparatorDef>& separatorDefs);
static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const std::vector<NormalizedField>& fields, const std::vector<char>& linkChars, int max_pos_visits, DebugTraceContextInterface* debugtrace);
static std::string typeListString( const std::vector<std::string>& types, const char* sep);


SentenceLexerContext::SentenceLexerContext(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_,
		const std::vector<SeparatorDef>& separators,
		const std::vector<LinkDef>& linkDefs,
		const std::vector<char>& linkChars,
		const std::string& source,
		int max_pos_visits,
		ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_debugtrace(0),m_vstorage(vstorage_),m_database(database_)
	,m_splits(),m_splitidx(-1)
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
	m_splits = getAlternativeSplits( vstorage_, database_, normalizedFields, linkChars, max_pos_visits, m_debugtrace);

	if (m_debugtrace)
	{
		m_debugtrace->open( "lexer");
		m_debugtrace->event( "sentence", "'%s'", source.c_str());
		std::size_t fidx = 0;
		for (; fidx != normalizedFields.size(); ++fidx)
		{
			std::string fstr = normalizedFields[ fidx].tostring(' ');
			m_debugtrace->event( "field", "content '%s'", fstr.c_str());
			const NormalizedField& normField = normalizedFields[ fidx];

			std::string fieldstr;
			std::vector<DelimiterDef>::const_iterator di = normField.delimiters.begin(), de = normField.delimiters.end();
			int pi = 0;
			for (; di != de; ++di)
			{
				if (di->pos > pi)
				{
					if (!fieldstr.empty()) fieldstr.append( ", ");
					std::string tok( normField.key.c_str() + pi, di->pos - pi);
					fieldstr.append( strus::string_format( "'%s'", tok.c_str()));
				}
				fieldstr.append( strus::string_format( "|%d'%c'", di->pos, di->substchr));
				pi = di->pos + 1;
			}
			if ((int)normField.key.size() > pi)
			{
				if (!fieldstr.empty()) fieldstr.append( ", ");
				std::string tok( normField.key.c_str() + pi, normField.key.size() - pi);
				fieldstr.append( strus::string_format( "'%s'", tok.c_str()));
			}
			m_debugtrace->event( "normalized", "%s", fieldstr.c_str());
		}

		std::vector<AlternativeSplit>::const_iterator ai = m_splits.begin(), ae = m_splits.end();
		for (; ai != ae; ++ai)
		{
			typedef AlternativeSplit::Element Element;
			std::string elemlist;
			std::vector<Element>::const_iterator ei = ai->ar.begin(), ee = ai->ar.end();
			for (; ei != ee; ++ei)
			{
				std::string typelist = typeListString( m_vstorage->getTypeNames( ei->featno) , ",");
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
		int charlen = 0;
		for (; si; si = std::strstr( si + charlen, pi->uchr))
		{
			charlen = strus::utf8charlen( *si);
			int splitpos = si - source.c_str();
			bool mapsToItself = (charlen == 1 && pi->uchr[0] == pi->substchr && pi->substchr != ' ');
			split.insert( LinkSplit( splitpos, pi->substchr, mapsToItself));
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
		bool mapsToItself = si->mapsToItself;
		char substchr = si->substchr;
		int cnt = 1;
		for (++si; si != se && si->splitpos <= nextpos; ++si)
		{
			if (!isSpaceSplit && si->substchr != ' ')
			{
				substchr = si->substchr;
			}
			if (si->splitpos > splitpos)
			{
				++cnt;
				splitpos = si->splitpos;
				nextpos = splitpos + strus::utf8charlen( source[ splitpos]);
			}
		}
		if (cnt > 1)
		{
			mapsToItself = false;
		}
		if (mapsToItself)
		{
			rt.key.push_back( substchr);
		}
		else
		{
			rt.delimiters.push_back( DelimiterDef( rt.key.size(), substchr));
			rt.key.push_back( SEPARATOR_CHR_SPLIT);
		}
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
	bool useAsDefault;	///< true if this candidate is a valable defaule value

	explicit KeyCandidate( std::size_t keylen_)
		:keylen(keylen_),editref(-1),editpos(-1),followpos(-1),useAsDefault(true){}
	KeyCandidate( std::size_t keylen_, int editref_, int editpos_, int followpos_, bool useAsDefault_)
		:keylen(keylen_),editref(editref_),editpos(editpos_),followpos(followpos_),useAsDefault(useAsDefault_){}
	KeyCandidate( const KeyCandidate& o)
		:keylen(o.keylen),editref(o.editref),editpos(o.editpos),followpos(o.followpos),useAsDefault(o.useAsDefault){}
};

static int nextEditStr( const std::string& edits, int pos)
{
	char const* ptr = std::strchr( edits.c_str()+pos, '\0');
	int rt = (ptr - edits.c_str());
	return (rt >= (int)edits.size()) ? 0 : rt+1;
}

static int findEditRef( const std::string& edits, int editref)
{
	int ei = nextEditStr( edits, 0);
	for (; ei && ei < editref; ei = nextEditStr( edits, ei))
	{
		if (0==std::strcmp( edits.c_str()+ei, edits.c_str() + editref))
		{
			return ei;
		}
	}
	return 0;
}

static void collectFollowKeyCandidates( std::vector<KeyCandidate>& candidates, std::string& edits, const NormalizedField& field, const std::vector<char>& linkChars)
{
	std::vector<DelimiterDef>::const_iterator di = field.delimiters.begin(), de = field.delimiters.end();
	for (; di != de; ++di)
	{
		if (di->pos != 0)
		{
			candidates.push_back( KeyCandidate( di->pos/*keylen*/, -1/*no edits*/, -1/*no edit pos*/, di->pos+1, true/*use as default*/));
		}
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
		int prev_editref = findEditRef( edits, editref);
		if (prev_editref)
		{
			edits.resize( editref-1);
			editref = prev_editref;
		}
		candidates.push_back( KeyCandidate( di->pos+1/*keylen*/, editref, di->pos/*editpos*/, di->pos+1/*followpos*/,di->substchr == ' '/*use as default*/));
	}
	if (!field.key.empty())
	{
		candidates.push_back( KeyCandidate( field.key.size()));
	}
}

class KeyIterator
{
public:
	struct DefaultElement
	{
		std::string key;
		int followPosIncr;

		DefaultElement()
			:key(),followPosIncr(0){}
		DefaultElement( const std::string& key_, int followPosIncr_)
			:key(key_),followPosIncr(followPosIncr_){}
		DefaultElement( const DefaultElement& o)
			:key(o.key),followPosIncr(o.followPosIncr){}
		DefaultElement& operator = ( const DefaultElement& o)
			{key = o.key; followPosIncr = o.followPosIncr; return *this;}

		bool defined() const
		{
			return !key.empty();
		}
	};

public:
	KeyIterator( const DatabaseClientInterface* database, const NormalizedField& field_, const std::vector<char>& linkChars_, strus::DebugTraceContextInterface* debugtrace_)
		:m_field(field_),m_edits(),m_candidates(),m_states()
		,m_candidateidx(0),m_keyfound(),m_linkChars(linkChars_)
		,m_cursor(database),m_defaultElement(),m_debugtrace(debugtrace_)
	{
		collectFollowKeyCandidates( m_candidates, m_edits, m_field, m_linkChars);
		m_states.assign( m_candidates.size(), 0);

		if (!m_candidates.empty())
		{
			std::vector<KeyCandidate>::const_iterator ci = m_candidates.begin(), ce = m_candidates.end();
			for (int cidx=0; ci != ce; ++ci,++cidx)
			{
				editState( cidx);
				if (ci->useAsDefault)
				{
					m_defaultElement = DefaultElement( std::string( m_field.key.c_str(), ci->keylen), ci->followpos);
					break;
				}
			}
			editState( 0);
		}
	}

	KeyIterator( const KeyIterator& o)
		:m_field(o.m_field),m_edits(o.m_edits),m_candidates(o.m_candidates),m_states(o.m_states)
		,m_candidateidx(o.m_candidateidx),m_keyfound(o.m_keyfound),m_linkChars(o.m_linkChars)
		,m_cursor(o.m_cursor),m_defaultElement(o.m_defaultElement),m_debugtrace(o.m_debugtrace){}

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

	std::string keyStrings( const char* delim, int maxsize)
	{
		std::string rt;
		int cnt = 0;
		if (m_candidates.empty()) return std::string();
		for (; cnt < maxsize; ++cnt)
		{
			if (cnt) rt.append( delim);
			rt.append( m_field.key.c_str(), m_candidates[ m_candidateidx].keylen);
			if (m_candidateidx+1 == m_candidates.size())
			{
				if (!backtrackStateUpperBound()) return rt;
			}
			else
			{
				if (!editState( m_candidateidx+1)) return rt;
			}
		}
		return rt;
	}

	bool first()
	{
		return editState( 0) && skipToNextKey();
	}

	bool next()
	{
		if (m_candidateidx+1 == m_candidates.size())
		{
			if (!backtrackStateUpperBound()) return false;
		}
		else
		{
			if (!editState( m_candidateidx+1)) return false;
		}
		return skipToNextKey();
	}

	const std::string& getKey() const
	{
		return m_keyfound;
	}

	const DefaultElement& defaultElement() const
	{
		return m_defaultElement;
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
			if (idx == m_candidateidx + 1)
			{
				m_states[ idx] = 0;
			}
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
		return li == linkChars.end() ? -1 : (li - linkChars.begin());
	}

	bool backtrackStateUpperBound()
	{
		int ci = m_candidateidx;
		for (; ci >= 0; --ci)
		{
			m_candidateidx = ci;
			const KeyCandidate& cd = m_candidates[ m_candidateidx];
			if (cd.editref < 0) continue;
			int ei = m_states[ m_candidateidx];
			if (m_edits[ cd.editref+ei] && m_edits[ cd.editref+ei+1])
			{
				m_states[ m_candidateidx] = ei + 1;
				editState( m_candidateidx);
				return true;
			}
			else
			{
				m_states[ m_candidateidx] = 0;
				editState( m_candidateidx);
				continue;
			}
		}
		return false;
	}

	enum KeyUpperBoundResult {KeyUpperBoundMiss,KeyUpperBoundFound,KeyUpperBoundMatch};
	/// \brief Set the search key to the first upper bound of the found key
	KeyUpperBoundResult skipToKeyUpperBound()
	{
		for (;;)
		{
			const KeyCandidate& cd = m_candidates[ m_candidateidx];
			if (cd.keylen >= m_keyfound.size())
			{
				int cmp = std::memcmp( m_keyfound.c_str(), m_field.key.c_str(), m_keyfound.size());
				if (cmp < 0)
				{
					//... we found a key that is alphabetically greater than the searched one
					return KeyUpperBoundFound;
				}
				else if (cmp > 0)
				{
					//... the key is still alphabetically greater than the searched key and making the key longer does not change this situation
					if (backtrackStateUpperBound())
					{
						continue;
					}
					else
					{
						return KeyUpperBoundMiss;
					}
				}
				else if (cd.keylen == m_keyfound.size())
				{
					//... we found an exact match
					return KeyUpperBoundMatch;
				}
				else
				{
					//... the key tested is not an exact match but an upperbound
					return KeyUpperBoundFound;
				}
			}
			else
			{
				int cmp = std::memcmp( m_keyfound.c_str(), m_field.key.c_str(), cd.keylen);
				if (cmp < 0)
				{
					//... we found a key that is alphabetically greater than the searched one
					return KeyUpperBoundFound;
				}
				else if (cmp > 0)
				{
					//... the key is still alphabetically greater than the searched key and making the key longer does not change this situation
					if (backtrackStateUpperBound())
					{
						continue;
					}
					else
					{
						return KeyUpperBoundMiss;
					}
				}
				else if (cd.keylen+1 == m_keyfound.size() && 0<=findLinkChar( m_linkChars, m_keyfound[cd.keylen]))
				{
					//... we found a match with a link character at the end (what we count as an exact match)
					return KeyUpperBoundMatch;
				}
				else if (editState( m_candidateidx+1))
				{
					//... try to test a longer key
					continue;
				}
				else
				{
					//... no longer keys left, try to backtrack to get a greater key
					if (backtrackStateUpperBound())
					{
						continue;
					}
					else
					{
						//... no backtracking possible any more, expanded the whole tree of key candidates
						return KeyUpperBoundMiss;
					}
				}
			}
		}
	}

	std::string currentKeyString() const
	{
		return m_candidateidx < m_candidates.size() ? std::string( m_field.key.c_str(), m_candidates[ m_candidateidx].keylen) : std::string();
	}

	bool skipToNextKey()
	{
		/// Algorithm: Start with the first key.
		/// We search an upperbound with the key as prefix.
		/// If we do not find an upper bound the search fails.
		/// If the upper bound is a match we are finished (we have the next key)
		/// Otherwise, we start the search again with this upperbound key as candidate.
		for(;;)
		{
			char const* keyptr = m_field.key.c_str();
			std::size_t keylen = m_candidates[ m_candidateidx].keylen;
			if (m_cursor.skipPrefix( keyptr, keylen, m_keyfound))
			{
				if (m_debugtrace)
				{
					std::string keystr( keyptr, keylen);
					m_debugtrace->event( "key", "search '%s' got '%s'", keystr.c_str(), m_keyfound.c_str());
				}
				switch (skipToKeyUpperBound())
				{
					case KeyUpperBoundMiss:
						return false;
					case KeyUpperBoundFound:
						if (m_debugtrace)
						{
							std::string keystr = currentKeyString();
							m_debugtrace->event( "key", "skip to upper bound '%s'", keystr.c_str());
						}
						continue;
					case KeyUpperBoundMatch:
						if (m_debugtrace)
						{
							std::string keystr = currentKeyString();
							m_debugtrace->event( "key", "match '%s'", keystr.c_str());
						}
						return true;
				}
			}
			else if (backtrackStateUpperBound())
			{
				if (m_debugtrace)
				{
					std::string keystr( m_field.key.c_str(), m_candidates[ m_candidateidx].keylen);
					m_debugtrace->event( "key", "backtrack to '%s'", keystr.c_str());
				}
			}
			else
			{
				return false;
			}
		}
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
	DefaultElement m_defaultElement;
	strus::DebugTraceContextInterface* m_debugtrace;
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
		:index(0),nofUntyped(0),nofFeatures(0),pos(-1){}
	KeyIteratorPathRef( std::size_t index_, int nofUntyped_, int nofFeatures_, int pos_)
		:index(index_),nofUntyped(nofUntyped_),nofFeatures(nofFeatures_),pos(pos_){}
	KeyIteratorPathRef( const KeyIteratorPathRef& o)
		:index(o.index),nofUntyped(o.nofUntyped),nofFeatures(o.nofFeatures),pos(o.pos){}
	KeyIteratorPathRef& operator = ( const KeyIteratorPathRef& o)
		{index=o.index; nofUntyped=o.nofUntyped; nofFeatures = o.nofFeatures; pos=o.pos; return *this;}

	bool operator < (const KeyIteratorPathRef& o) const
	{
		return nofFeatures == o.nofFeatures
				? (pos == o.pos
					? (nofUntyped == o.nofUntyped
						? index < o.index
						: nofUntyped < o.nofUntyped)
					: pos > o.pos)
				: nofFeatures < o.nofFeatures ;
	}
};

struct AlternativeSplitQueue
{
public:
	AlternativeSplitQueue( const NormalizedField& origfield, int max_pos_visits_, DebugTraceContextInterface* debugtrace_)
		:m_results(),m_paths(),m_pathqueue(),m_top(),m_elements(),m_elementlists(),m_pos2minNofFeaturesMap()
		,m_minNofUntyped(std::numeric_limits<int>::max())
		,m_minNofFeatures(std::numeric_limits<int>::max())
		,m_max_pos_visits(max_pos_visits_)
		,m_debugtrace(debugtrace_)
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
				if (m_debugtrace) m_debugtrace->event( "prunning", "pos %d, nof untyped %d, min nof untyped %d", pqitr->pos, pqitr->nofUntyped, m_minNofUntyped);
				m_pathqueue.erase( pqitr);
				continue;
			}
			m_top = *pqitr;
			m_pathqueue.erase( pqitr);
			return &m_paths[ m_top.index].field;
		}
		m_top = KeyIteratorPathRef();
		return NULL;
	}

	std::string elementStackToString( int eref) const
	{
		if (eref < 0) return std::string();
		const AlternativeSplitElementRef& elem = m_elementlists[ eref];
		std::string rt = elementStackToString( elem.next);
		if (!rt.empty()) rt.push_back( '/');
		rt.append( m_elements[ elem.eidx].feat);
		return rt;
	}

	void push( const NormalizedField& followfield, const AlternativeSplit::Element& element, int followPosIncr)
	{
		int nofUntyped = m_top.nofUntyped + (element.featno ? 0:1);
		int nofFeatures = m_top.nofFeatures + 1;

		if (followfield.defined())
		{
			int expectedNofFeatures = nofFeatures + 1;
			int nextpos = m_top.pos + followPosIncr;
			std::pair<Pos2minNofFeaturesMap::iterator,bool> ins =
				m_pos2minNofFeaturesMap.insert( Pos2minNofFeaturesMap::value_type( nextpos, VisitCount( nofFeatures, 0)));
			if (ins.second == false)
			{
				Pos2minNofFeaturesMap::iterator itr = ins.first;
				VisitCount& vc = itr->second;
				if (nofFeatures < vc.nofFeatures)
				{
					vc.nofFeatures = nofFeatures;
					if (m_minNofFeatures != std::numeric_limits<int>::max())
					{
						expectedNofFeatures = m_minNofFeatures;
					}
				}
				else
				{
					if (++vc.cnt >= m_max_pos_visits)
					{
						if (m_minNofFeatures != std::numeric_limits<int>::max())
						{
							if (m_debugtrace) m_debugtrace->event( "prunning", "pos %d, node visits %d", nextpos, vc.cnt);
							return;
							/// ... rule out this path, because we visited it already a 'm_max_pos_visits' times without improvement
						}
					}
					else if (m_minNofFeatures < std::numeric_limits<int>::max())
					{
						int numberOfFeaturesMoreThanMinumum = nofFeatures - vc.nofFeatures;
						expectedNofFeatures = m_minNofFeatures + numberOfFeaturesMoreThanMinumum;
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
				if (m_debugtrace)
				{
					std::string fieldstr = followfield.tostring(' ');
					std::string stack = elementStackToString( eref);

					m_debugtrace->event( "follow", "found '%s', next '%s'", stack.c_str(), fieldstr.c_str());
				}
			}
			else
			{
				if (m_debugtrace) m_debugtrace->event( "prunning", "pos %d, expected nof features %d, min nof features %d (accepted %d)", nextpos, expectedNofFeatures, m_minNofFeatures, SentenceLexerContext::maxFeaturePrunning( m_minNofFeatures));
			}
		}
		else if (m_minNofFeatures != std::numeric_limits<int>::max() && nofFeatures > SentenceLexerContext::maxFeaturePrunning( m_minNofFeatures))
		{
			if (m_debugtrace) m_debugtrace->event( "prunning", "result with %d features, min nof features %d (accepted %d)", nofFeatures, m_minNofFeatures, SentenceLexerContext::maxFeaturePrunning( m_minNofFeatures));
		}
		else if (m_minNofUntyped != std::numeric_limits<int>::max() && nofUntyped > m_minNofUntyped)
		{
			if (m_debugtrace) m_debugtrace->event( "prunning", "result with %d untyped, min nof untyped %d", nofUntyped, m_minNofUntyped);
		}
		else
		{
			if (nofUntyped < m_minNofUntyped)
			{
				m_minNofUntyped = nofUntyped;
				if (m_debugtrace) m_debugtrace->event( "prunning", "clear %d results (nof untyped)", (int)m_results.size());
				m_results.clear();
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

	std::string stackStateString() const
	{
		if (m_top.pos >= 0)
		{
			return strus::string_format("size %d, top pos %d", (int)m_pathqueue.size(), m_top.pos);
		}
		else
		{
			return strus::string_format("size %d", (int)m_pathqueue.size());
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
	int m_max_pos_visits;
	DebugTraceContextInterface* m_debugtrace;
};

static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const NormalizedField& origfield, const std::vector<char>& linkChars, int max_pos_visits, DebugTraceContextInterface* debugtrace)
{
	AlternativeSplitQueue queue( origfield, max_pos_visits, debugtrace);

	const NormalizedField* field = queue.nextField();
	for (; field; field=queue.nextField())
	{
		KeyIterator kitr( database, *field, linkChars, debugtrace);
		if (debugtrace)
		{
			std::string stkstr = queue.stackStateString();
			debugtrace->event( "stack", "%s", stkstr.c_str());
			debugtrace->open( "field");

			std::string fstr = field->tostring(' ');
			debugtrace->event( "string", "%s", fstr.c_str());
			std::string keystr = kitr.keyStrings( "','", 20);
			debugtrace->event( "candidates", "'%s'", keystr.c_str());
		}
		bool hasMore = kitr.first();
		bool useDefault = kitr.defaultElement().defined();

		for (;hasMore; hasMore = kitr.next())
		{
			AlternativeSplit::Element element( kitr.getKey(), vstorage->getFeatNo( kitr.getKey()));
			int fpos = kitr.followPosIncr();
			if (debugtrace)
			{
				std::string typeliststr = typeListString( vstorage->getTypeNames( element.featno), ",");
				debugtrace->event( "element", "follow pos %d, feat '%s', types {%s}", fpos, element.feat.c_str(), typeliststr.c_str());
			}
			queue.push( kitr.followField( fpos), element, fpos);
			if (element.feat == kitr.defaultElement().key) useDefault = false;
		}
		if (useDefault)
		{
			AlternativeSplit::Element element( kitr.defaultElement().key, 0/*featno*/);
			queue.push( kitr.followField( kitr.defaultElement().followPosIncr), element, kitr.defaultElement().followPosIncr);
			if (debugtrace)
			{
				debugtrace->event( "default", "%s", kitr.defaultElement().key.c_str());
			}
		}
		if (debugtrace)
		{
			debugtrace->close();
		}
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

static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const std::vector<NormalizedField>& fields, const std::vector<char>& linkChars, int max_pos_visits, DebugTraceContextInterface* debugtrace)
{
	if (debugtrace) debugtrace->event( "fields", "%d", (int)fields.size());
	if (fields.empty()) return std::vector<AlternativeSplit>();
	std::vector<NormalizedField>::const_iterator fi = fields.begin(), fe = fields.end();
	std::vector<AlternativeSplit> rt = getAlternativeSplits( vstorage, database, *fi, linkChars, max_pos_visits, debugtrace);

	for (++fi; fi != fe; ++fi)
	{
		std::vector<AlternativeSplit> nextsplits = getAlternativeSplits( vstorage, database, *fi, linkChars, max_pos_visits, debugtrace);
		joinAlternativeSplits( rt, nextsplits);
	}
	return rt;
}

std::string SentenceLexerContext::featureValue( int idx) const
{
	try
	{
		if (m_splitidx >= 0 && m_splitidx < (int)m_splits.size()) return m_splits[ m_splitidx].ar[ idx].feat;
		throw std::runtime_error(_TXT("array bound read"));
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' getting feature value in current split: %s"), MODULENAME, *m_errorhnd, std::string());
}

std::vector<std::string> SentenceLexerContext::featureTypes( int idx) const
{
	try
	{
		if (m_splitidx >= 0 && m_splitidx < (int)m_splits.size()) return m_vstorage->getTypeNames( m_splits[ m_splitidx].ar[ idx].featno);
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
	if (m_splitidx >= 0 && m_splitidx < (int)m_splits.size())
	{
		return m_splits[ m_splitidx].ar.size();
	}
	else
	{
		return 0;
	}
}

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

typedef int GroupId;
typedef std::vector<GroupId> Group;

class SimGroupData
{
public:
	explicit SimGroupData( const VectorStorageClient* vstorage_)
		:m_vstorage(vstorage_),m_featnomap(),m_typenomap(),m_vectors(),m_groups(),m_featmap(){}

	FeatNum getOrCreateFeatNum( const SentenceTerm& term);
	GroupId getOrCreateFeatGroup( const FeatNum& featnum);

	const std::vector<Group>& groups() const
	{
		return m_groups;
	}

private:
	const VectorStorageClient* m_vstorage;
	std::map<std::string,strus::Index> m_featnomap;
	std::map<std::string,strus::Index> m_typenomap;
	std::vector<WordVector> m_vectors;
	std::vector<Group> m_groups;
	std::map<FeatNum,GroupId> m_featmap;
};

#define SIMILIARITY_DISTANCE 0.5

FeatNum SimGroupData::getOrCreateFeatNum( const SentenceTerm& term)
{
	typedef std::map<std::string,strus::Index> Map;
	strus::Index featno = 0;
	strus::Index typeno = 0;
	{
		Map::const_iterator mi = m_featnomap.find( term.value());
		if (mi == m_featnomap.end())
		{
			m_featnomap.insert( Map::value_type( term.value(), featno=m_vstorage->getFeatNo( term.value())));
		}
		else
		{
			featno = mi->second;
		}
	}{
		Map::const_iterator mi = m_typenomap.find( term.type());
		if (mi == m_typenomap.end())
		{
			m_typenomap.insert( Map::value_type( term.type(), typeno=m_vstorage->getTypeNo( term.type())));
		}
		else
		{
			typeno = mi->second;
		}
	}
	return FeatNum( typeno, featno);
}

GroupId SimGroupData::getOrCreateFeatGroup( const FeatNum& featnum)
{
	typedef std::map<FeatNum,GroupId> Map;
	std::pair<Map::iterator,bool> ins = m_featmap.insert( Map::value_type( featnum, m_groups.size()));
	if (ins.second /*insert took place*/)
	{
		m_vectors.push_back( m_vstorage->getVector( featnum.typeno, featnum.typeno));
		m_groups.push_back( Group());
		m_groups.back().push_back( m_groups.size());

		std::vector<Group>::iterator gi = m_groups.begin(), ge = m_groups.end();
		GroupId gidx = 0, last_gidx = m_groups.size();
		m_groups.back().push_back( last_gidx);

		for (--ge/*without new group added*/; gi != ge; ++gi,++gidx)
		{
			double sim = m_vstorage->vectorSimilarity( m_vectors.back(), m_vectors[ gidx]);
			if (sim > SIMILIARITY_DISTANCE)
			{
				m_groups.back().push_back( gidx);
				gi->push_back( last_gidx);
			}
		}
		return last_gidx;
	}
	else
	{
		return ins.first->second;
	}
}


std::vector<SentenceGuess> SentenceLexerContext::rankSentences( const std::vector<SentenceGuess>& sentences, int maxNofResults) const
{
	try
	{
		if (sentences.empty()) return std::vector<SentenceGuess>();
		struct Rank
		{
			int idx;
			double weight;

			Rank( int idx_, double weight_)
				:idx(idx_),weight(weight_){}
			Rank( const Rank& o)
				:idx(o.idx),weight(o.weight){}

			bool operator < (const Rank& o) const
			{
				return (std::abs( weight - o.weight) <= std::numeric_limits<float>::epsilon()) ? idx < o.idx : weight < o.weight;
			}
		};
		struct Range
		{
			int min;
			int max;
		
			Range( int min_, int max_) :min(min_),max(max_){}
			Range( const Range& o) :min(o.min),max(o.max){}
		};

		struct Local
		{
			static int getWeight( const Range& hirange, const Range& lorange, int hival, int loval)
			{
				int lodiff = lorange.max - lorange.min;
				hival -= hirange.min;
				loval -= lorange.min;
				return hival * (lodiff + 1) + loval;
			}
		};

		// Map features to sets of integers with the property that A * B != {}, if A ~ B
		// Each set gets an integer assigned
		// Assign group of such integers to sentences
		// The minimal cover of a group is used to calculate the weight of the candidate
		SimGroupData simGroupData( m_vstorage);
		std::vector<std::vector<GroupId> > sentence_groups;
		sentence_groups.reserve( sentences.size());

		std::vector<SentenceGuess>::const_iterator si = sentences.begin(), se = sentences.end();
		for (; si != se; ++si)
		{
			sentence_groups.push_back( std::vector<GroupId>());
			SentenceTermList::const_iterator ti = si->terms().begin(), te = si->terms().end();
			for (; ti != te; ++ti)
			{
				FeatNum featnum = simGroupData.getOrCreateFeatNum( *ti);
				if (featnum.valid())
				{
					GroupId gid = simGroupData.getOrCreateFeatGroup( featnum);
					sentence_groups.back().push_back( gid);
				}
			}
		}

		// Calculate minimal cover approximations and value boundaries for mapping pairs of (minimal cover size, nof terms) to an integer:
		MinimalCoverData minimalCoverData( simGroupData.groups());
		std::vector<Rank> ranks;
		ranks.reserve( sentences.size());

		Range coverRange( std::numeric_limits<int>::max(), 0);
		Range sentsizeRange( std::numeric_limits<int>::max(), 0);

		std::vector<std::vector<GroupId> >::const_iterator gi = sentence_groups.begin(), ge = sentence_groups.end();
		for (int gidx=0; gi != ge; ++gi,++gidx)
		{
			int minimalCoverSize = minimalCoverData.minimalCoverSizeApproximation( *gi);
			Rank rank( gidx, minimalCoverSize);
			ranks.push_back( rank);

			if (minimalCoverSize < coverRange.min) coverRange.min = minimalCoverSize;
			if (minimalCoverSize > coverRange.max) coverRange.max = minimalCoverSize;
			int nofTerms = sentences[gidx].terms().size();
			if (nofTerms < sentsizeRange.min) sentsizeRange.min = nofTerms;
			if (nofTerms > sentsizeRange.max) sentsizeRange.max = nofTerms;
		}

		// Calculate the weights of the ranks by dividing the integer corresponding to the pair (minimal cover size, nof terms) by the maximum value 
		//	to get normalized values between 0.0 and 1.0
		double maxWeight = 0.0;
		std::vector<Rank>::iterator ri = ranks.begin(), re = ranks.end();
		for (; ri != re; ++ri)
		{
			const std::vector<GroupId>& group = sentence_groups[ ri->idx];
			ri->weight = Local::getWeight( coverRange, sentsizeRange, (int)ri->weight, group.size());
			if (ri->weight > maxWeight) maxWeight = ri->weight;
		}
		ri = ranks.begin(), re = ranks.end();
		for (; ri != re; ++ri)
		{
			ri->weight /= maxWeight;
		}

		// Select the best N (weight) of the ranks and return them
		if (maxNofResults < 0 || maxNofResults > (int)ranks.size())
		{
			maxNofResults = ranks.size();
		}
		std::nth_element( ranks.begin(), ranks.begin() + maxNofResults, ranks.end());
		std::sort( ranks.begin(), ranks.begin() + maxNofResults);

		std::vector<SentenceGuess> rt;
		rt.reserve( maxNofResults);
		ri = ranks.begin();
		re = ranks.begin() + maxNofResults;
		for (; ri != re; ++ri)
		{
			const SentenceGuess& sentence = sentences[ ri->idx];
			rt.push_back( SentenceGuess( sentence.classname(), sentence.terms(), ri->weight));
		}
		if (m_debugtrace)
		{
			m_debugtrace->open( "ranklist");
			std::string sentstr = termListString( si->terms(), ", ");
			m_debugtrace->event( "sentence", "weight %.3f class %s %s", si->weight(), si->classname().c_str(), sentstr.c_str());
			m_debugtrace->close();
		}
		return rt;
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in '%s' getting ranked list of sentence guesses: %s"), MODULENAME, *m_errorhnd, std::vector<SentenceGuess>());
}


