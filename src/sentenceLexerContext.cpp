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

using namespace strus;

#define MODULENAME "sentence lexer context (vector)"
#define STRUS_DBGTRACE_COMPONENT_NAME "sentence"

typedef SentenceLexerInstance::SeparatorDef SeparatorDef;
typedef SentenceLexerInstance::LinkDef LinkDef;
typedef SentenceLexerInstance::LinkChar LinkChar;
typedef SentenceLexerContext::AlternativeSplit AlternativeSplit;

struct LinkSplit
{
	int splitpos;
	int priority;

	LinkSplit( int splitpos_, int priority_)
		:splitpos(splitpos_),priority(priority_){}
	LinkSplit( const LinkSplit& o)
		:splitpos(o.splitpos),priority(o.priority){}

	bool operator<( const LinkSplit& o) const
	{
		return splitpos == o.splitpos
			? priority > o.priority
			: splitpos < o.splitpos;
	}
};

struct DelimiterDef
{
	int pos;
	int priority;

	DelimiterDef( int pos_, int priority_)
		:pos(pos_),priority(priority_){}
	DelimiterDef( const DelimiterDef& o)
		:pos(o.pos),priority(o.priority){}
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
	bool defined() const
	{
		return !key.empty();
	}
};

static NormalizedField normalizeField( const std::string& source, const std::vector<LinkDef>& linkDefs);
static std::vector<std::string> getFields( const std::string& source, const std::vector<SeparatorDef>& separatorDefs);
static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const std::vector<NormalizedField>& fields, const std::vector<LinkChar>& linkChars);


#define SEPARATOR_CHR_SPLIT '\1'


SentenceLexerContext::SentenceLexerContext(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_,
		const std::vector<SeparatorDef>& separators,
		const std::vector<LinkDef>& linkDefs,
		const std::vector<LinkChar>& linkChars,
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
	m_splits = getAlternativeSplits( vstorage_, database_, normalizedFields, linkChars);

	if (m_debugtrace)
	{
		m_debugtrace->event( "sentence", "%s", source.c_str());
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
				std::string tok( ni->key.c_str() + pi, di->pos - pi);
				m_debugtrace->event( "token", "'%s'", tok.c_str());
				m_debugtrace->event( "delim", "pos  %d, priority %d", di->pos, di->priority);
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
				std::string typelist;
				std::vector<std::string>::const_iterator ti = ei->types.begin(), te = ei->types.end();
				for (; ti != te; ++ti)
				{
					if (!typelist.empty()) typelist.append( ",");
					typelist.append( *ti);
				}
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
			split.insert( LinkSplit( splitpos, pi->priority));
		}
	}
	
	int pos = 0;
	std::string key;
	std::set<LinkSplit>::const_iterator si = split.begin(), se = split.end();
	while (si != se)
	{
		int splitpos = si->splitpos;
		char priority = si->priority;
		for (++si; si != se && si->splitpos != splitpos; ++si)
		{
			if (si->priority > priority)
			{
				priority = si->priority;
			}
		}
		rt.key.append( source.c_str() + pos, splitpos - pos);
		rt.delimiters.push_back( DelimiterDef( pos, priority));
		pos += strus::utf8charlen( source[ pos]);
		rt.key.push_back( SEPARATOR_CHR_SPLIT);
	}
	rt.key.append( source.c_str() + pos, source.size() - pos);
	return rt;
}


class KeyIterator
{
public:
	KeyIterator( const DatabaseClientInterface* database, const NormalizedField& field_, const std::vector<LinkChar>& linkChars_)
		:field(field_),keysize(0),keyfound(),cursor(database),splitstate(),linkChars(linkChars_)
	{
		if (field.delimiters.empty())
		{
			keysize = field.key.size();
		}
		else
		{
			std::vector<DelimiterDef>::const_iterator di = field.delimiters.begin(), de = field.delimiters.end();
			for (; di != de; ++di)
			{
				splitstate.push_back( State( di->pos, 0, di->priority));
				if (!keysize) keysize = di->pos;
				field.key[ di->pos] = 0;
			}
		}
	}

	KeyIterator( const KeyIterator& o)
		:field(o.field),keysize(o.keysize),keyfound(o.keyfound),cursor(o.cursor),splitstate(o.splitstate),linkChars(o.linkChars){}

	bool first()
	{
		return skipToNextKey();
	}

	bool next()
	{
		if (!nextState(false/*not backTrack*/))
		{
			return false;
		}
		return skipToNextKey();
	}

	const std::string& getKey() const
	{
		return keyfound;
	}

	std::string defaultKey() const
	{
		if (field.delimiters.empty())
		{
			return field.key;
		}
		else
		{
			return std::string( field.key.c_str(), field.delimiters[0].pos);
		}
	}
	int followPosIncr() const
	{
		if (keyfound.empty())
		{
			if (field.delimiters.empty())
			{
				return field.key.size();
			}
			else
			{
				return field.delimiters[0].pos+1;
			}
		}
		else
		{
			return keysize;
		}
	}
	NormalizedField follow() const
	{
		if (field.delimiters.empty())
		{
			return NormalizedField();
		}
		if (keyfound.empty())
		{
			int pos = field.delimiters[0].pos;
			std::vector<DelimiterDef> delim;
			std::vector<DelimiterDef>::const_iterator di = field.delimiters.begin(), de = field.delimiters.end();
			for (++di; di != de; ++di)
			{
				delim.push_back( DelimiterDef( di->pos - pos, di->priority));
			}
			return NormalizedField( std::string(field.key.c_str() + pos + 1), delim);
		}
		else
		{
			std::vector<DelimiterDef>::const_iterator di = field.delimiters.begin(), de = field.delimiters.end();
			for (++di; di != de && di->pos < (int)keysize; ++di){}
			if (di == de)
			{
				return NormalizedField();
			}
			else
			{
				int pos = di->pos;
				std::vector<DelimiterDef> delim;
				for (++di; di != de; ++di)
				{
					delim.push_back( DelimiterDef( di->pos - pos, di->priority));
				}
				return NormalizedField( std::string(field.key.c_str() + pos + 1), delim);
			}
		}
	}

private:
	struct State
	{
		State( int pos_, int idx_, int priority_)
			:idx(idx_),pos(pos_),priority(priority_){}
		State( const State& o)
			:idx(o.idx),pos(o.pos),priority(o.priority){}
		int idx;
		int pos;
		int priority;
	};

	bool nextState( bool backtrack)
	{
		if (splitstate.empty()) return false;
		std::vector<State>::iterator si = splitstate.begin(), se = splitstate.end();
		for (; si != se && si->idx != 0; ++si){}
		if (si == se)
		{
			--si;
			if (si == splitstate.begin())
			{
				return false;
			}
		}
		for (;;)
		{
			if (backtrack) for (; si != splitstate.begin() && si->idx == 0; --si){}
			for (++si->idx; si->idx <= (int)linkChars.size() && linkChars[si->idx].priority < si->priority; ++si->idx){}

			if (si->idx > (int)linkChars.size())
			{
				si->idx = 0;
				if (si == splitstate.begin()) return false;
				--si;
			}
			else
			{
				field.key[ si->pos] = linkChars[ si->idx-1].chr;
				++si;
				if (si != se)
				{
					keysize = si->pos;
					for (; si != se; ++si)
					{
						field.key[ si->pos] = 0;
						si->idx = 0;
					}
				}
				else
				{
					keysize = field.key.size();
				}
				return true;
			}
		}
	}
	static int findLinkChar( const std::vector<LinkChar>& linkChars, char chr, int priority)
	{
		int lidx=0;
		std::vector<LinkChar>::const_iterator li = linkChars.begin(), le = linkChars.end();
		for (; li != le; ++li,++lidx)
		{
			if (li->priority >= priority && li->chr == chr)
			{
				break;
			}
		}
		return (li == le) ? -1 : lidx;
	}

	bool tryMatchTail()
	{
		if (keyfound.size() > field.key.size()+1)
		{
			return false;
		}
		std::vector<State>::iterator si = splitstate.begin(), se = splitstate.end();
		for (; si != se && si->pos < (int)keyfound.size(); ++si)
		{
			int lidx = findLinkChar( linkChars, keyfound[ si->pos], si->priority);
			if (lidx < 0) return false;
			si->idx = lidx;
			field.key[ si->pos] = linkChars[lidx].chr;
		}
		if (si != se)
		{
			if (si->pos == (int)keyfound.size())
			{
				keysize = si->pos;
				field.key[ keysize] = '\0';
				for (; si != se; ++si)
				{
					field.key[ si->pos] = 0;
					si->idx = 0;
				}
			}
			else
			{
				return false;
			}
		}
		else
		{
			keysize = field.key.size();
		}
		if (keysize == keyfound.size()
			&& 0==std::memcmp( field.key.c_str(), keyfound.c_str(), keyfound.size()))
		{
			return true;
		}
		else if (keysize+1 == (keyfound.size())
			 && 0<=findLinkChar( linkChars, keyfound[keysize], -1)
			 && 0==std::memcmp( field.key.c_str(), keyfound.c_str(), keysize))
		{
			return true;
		}
		else
		{
			return false;
		}
	}

	bool skipToNextKey()
	{
		for (;;)
		{
			if (keysize && cursor.skipPrefix( field.key.c_str(), keysize, keyfound))
			{
				if (keyfound.size() == keysize)
				{
					return true;
				}
				else if (keyfound.size() == keysize+1 && 0<=findLinkChar( linkChars, keyfound[keysize], -1))
				{
					return true;
				}
				else if (keyfound.size() > field.key.size()+1)
				{
					keyfound.clear();
					return false;
				}
				else if (tryMatchTail())
				{
					return true;
				}
				else if (nextState( false/*not backtrack*/))
				{
					continue;
				}
				else
				{
					keyfound.clear();
					return false;
				}
			}
			else if (nextState( true/*backtrack*/))
			{
				continue;
			}
			else
			{
				keyfound.clear();
				return false;
			}
		}
	}

private:
	NormalizedField field;
	std::size_t keysize;
	std::string keyfound;
	DatabaseAdapter::FeatureCursor cursor;
	std::vector<State> splitstate;
	std::vector<LinkChar> linkChars;
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
	int pos;

	KeyIteratorPathRef( std::size_t index_, int nofUntyped_, int pos_)
		:index(index_),nofUntyped(nofUntyped_),pos(pos_){}
	KeyIteratorPathRef( const KeyIteratorPathRef& o)
		:index(o.index),nofUntyped(o.nofUntyped),pos(o.pos){}

	bool operator < (const KeyIteratorPathRef& o) const
	{
		return pos == o.pos
			? (nofUntyped == o.nofUntyped
				? index < o.index
				: nofUntyped < o.nofUntyped)
			: pos > o.pos;
	}
};

struct AlternativeSplitQueue
{
public:
	AlternativeSplitQueue( const NormalizedField& origfield)
		:m_results(),m_paths(),m_pathqueue(),m_elements(),m_elementlists(),m_minNofUntyped(std::numeric_limits<int>::max())

	{
		m_paths.push_back( KeyIteratorPath( origfield));
		m_pathqueue.insert( KeyIteratorPathRef( 0/*index*/, 0/*nofUntyped*/, 0/*pos*/));
	}

	const NormalizedField* firstField()
	{
		while (!m_pathqueue.empty())
		{
			std::set<KeyIteratorPathRef>::iterator pqitr = m_pathqueue.begin();
			if (pqitr->nofUntyped > m_minNofUntyped)
			{
				m_pathqueue.erase( pqitr);
				continue;
			}
			return &m_paths[ pqitr->index].field;
		}
		return NULL;
	}

	const NormalizedField* nextField()
	{
		if (m_pathqueue.empty()) return NULL;
		m_pathqueue.erase( m_pathqueue.begin());
		return firstField();
	}

	void push( const NormalizedField& followfield, const AlternativeSplit::Element& element, int followPosIncr)
	{
		std::set<KeyIteratorPathRef>::iterator pqitr = m_pathqueue.begin();
		int nofUntyped = pqitr->nofUntyped + (element.types.empty() ? 1:0);

		if (followfield.defined())
		{
			if (nofUntyped <= m_minNofUntyped)
			{
				int eref = m_elementlists.size();
				m_elementlists.push_back( AlternativeSplitElementRef( m_elements.size(), m_paths[ pqitr->index].eref));
				m_elements.push_back( element);
	
				KeyIteratorPath follow( followfield, eref);
				m_pathqueue.insert( KeyIteratorPathRef( m_paths.size()/*index*/, nofUntyped, pqitr->pos + followPosIncr));
				m_paths.push_back( follow);
			}
		}
		else
		{
			if (nofUntyped < m_minNofUntyped)
			{
				m_minNofUntyped = nofUntyped;
			}
			int eitr = m_paths[ pqitr->index].eref;
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
	std::vector<AlternativeSplit::Element> m_elements;
	std::vector<AlternativeSplitElementRef> m_elementlists;
	int m_minNofUntyped;
};

static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const NormalizedField& origfield, const std::vector<LinkChar>& linkChars)
{
	AlternativeSplitQueue queue( origfield);

	const NormalizedField* field = queue.firstField();
	for (; field; field=queue.nextField())
	{
		KeyIterator kitr( database, *field, linkChars);
		bool hasMore = kitr.first();
		std::string defaultKey = kitr.defaultKey();
		bool useDefault = !defaultKey.empty();

		for (;hasMore; hasMore = kitr.next())
		{
			AlternativeSplit::Element element( kitr.getKey(), vstorage->featureTypes( kitr.getKey()));
			/*[-]*/std::cerr << "++visit '" << element.feat << "'" << std::endl;
			queue.push( kitr.follow(), element, kitr.followPosIncr());
			if (element.feat == defaultKey) useDefault = false;
		}
		if (useDefault)
		{
			/*[-]*/std::cerr << "++default key '" << defaultKey << "'" << std::endl;
			AlternativeSplit::Element element( kitr.defaultKey(), std::vector<std::string>());
			queue.push( kitr.follow(), element, kitr.followPosIncr());
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

static std::vector<AlternativeSplit> getAlternativeSplits( const VectorStorageClient* vstorage, const DatabaseClientInterface* database, const std::vector<NormalizedField>& fields, const std::vector<LinkChar>& linkChars)
{
	if (fields.empty()) return std::vector<AlternativeSplit>();
	std::vector<NormalizedField>::const_iterator fi = fields.begin(), fe = fields.end();
	std::vector<AlternativeSplit> rt = getAlternativeSplits( vstorage, database, *fi, linkChars);

	for (++fi; fi != fe; ++fi)
	{
		std::vector<AlternativeSplit> nextsplits = getAlternativeSplits( vstorage, database, *fi, linkChars);
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


