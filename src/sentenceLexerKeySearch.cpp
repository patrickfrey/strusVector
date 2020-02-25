/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for searching alternative splits of a field
/// \file sentenceLexerKeySearch.cpp
#include "sentenceLexerKeySearch.hpp"
#include "databaseAdapter.hpp"
#include "vectorStorageClient.hpp"
#include <vector>
#include <string>
#include <cstring>
#include <algorithm>

using namespace strus;

SentenceLexerKeySearch::SentenceLexerKeySearch(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_,
		ErrorBufferInterface* errorhnd_,
		char spaceSubst_,
		char linkSubst_)
	:m_errorhnd(errorhnd_),m_vstorage(vstorage_),m_cursor(database_)
	,m_spaceSubst(spaceSubst_),m_linkSubst(linkSubst_)
{}

static std::size_t initKeyBuf( char* keybuf, std::size_t keybufsize, const char* fieldptr, std::size_t fieldsize, std::size_t pos)
{
	std::size_t keysize = fieldsize - pos;
	if (keysize >= keybufsize) keysize = keybufsize-1;
	std::memcpy( keybuf, fieldptr + pos, keysize);
	keybuf[ keysize] = 0;
	return keysize;
}

struct SolutionElement
{
	int startpos;
	int endpos;
	int predidx;
	bool resolved;

	SolutionElement( int startpos_, int endpos_, int predidx_, bool resolved_)
		:startpos(startpos_),endpos(endpos_),predidx(predidx_),resolved(resolved_){}
	SolutionElement( const SolutionElement& o)
		:startpos(o.startpos),endpos(o.endpos),predidx(o.predidx),resolved(o.resolved){}
};

struct Solution
{
	int idx;
	int nofUnresolved;

	Solution( int idx_, int nofUnresolved_)
		:idx(idx_),nofUnresolved(nofUnresolved_){}
	Solution( const Solution& o)
		:idx(o.idx),nofUnresolved(o.nofUnresolved){}
};

struct QueueElement
{
	int nofUnresolved;
	int pos;
	int predidx;

	QueueElement( int nofUnresolved_, int pos_, int predidx_)
		:nofUnresolved(nofUnresolved_),pos(pos_),predidx(predidx_){}
	QueueElement( const QueueElement& o)
		:nofUnresolved(o.nofUnresolved),pos(o.pos),predidx(o.predidx){}

	bool operator < (const QueueElement& o) const
	{
		return nofUnresolved == o.nofUnresolved
				? pos == o.pos
					? predidx < o.predidx
					: pos > o.pos
				: nofUnresolved < o.nofUnresolved;
	}
};


struct KeyCursor
{
	enum {
		MaxKeyLen=1024
	};

	KeyCursor( const std::string& field_, int curpos_, char spaceSubst_, char linkSubst_)
		:keysize(0),fieldptr(field_.c_str()),fieldsize(field_.size()),curpos(curpos_)
		,kitr(0),key(0),spaceSubst(spaceSubst_),linkSubst(linkSubst_)
	{
		keysize = initKeyBuf( keybuf, MaxKeyLen, fieldptr, fieldsize, curpos);
		kitr = key = keybuf;
	}

	void skipToken()
	{
		if (*kitr == linkSubst)
		{
			++kitr;
		}
		else
		{
			for (;*kitr && *kitr != spaceSubst && *kitr != linkSubst; ++kitr){}
		}
	}

	bool isSpace() const
	{
		return *kitr == spaceSubst;
	}

	bool isLink() const
	{
		return *kitr == linkSubst;
	}

	bool tryLoad( DatabaseAdapter::FeatureCursor& cursor, std::string& loadbuf)
	{
		return (cursor.skipPrefix( key, kitr-key, loadbuf)
			&& loadbuf.size() <= fieldsize
			&& 0==std::memcmp( fieldptr, loadbuf.c_str(), loadbuf.size())
			&& (fieldsize == loadbuf.size()
				|| fieldptr[ loadbuf.size()] == spaceSubst
				|| fieldptr[ loadbuf.size()] == linkSubst));
	}

	void setPosition( std::size_t pos)
	{
		kitr = key + pos;
	}

	std::size_t keypos() const
	{
		return kitr - key;
	}

	bool hasMore() const
	{
		return *kitr != 0;
	}

	bool currentTokenIsWord()
	{
		char const* ki = key;
		const char* ke = kitr;
		for (; ki != ke && (*ki == spaceSubst || *ki == linkSubst); ++ki){}
		return ki != ke;
	}

	char keybuf[ MaxKeyLen];
	std::size_t keysize;
	const char* fieldptr;
	std::size_t fieldsize;
	int curpos;
	char* kitr;
	char* key;
	char spaceSubst;
	char linkSubst;
};

std::vector<SentenceLexerKeySearch::ItemList> SentenceLexerKeySearch::scanField( const std::string& field)
{
	std::vector<ItemList> rt;
	std::vector<SolutionElement> elemar;
	std::set<QueueElement> queue;
	std::vector<Solution> solutions;
	std::string loadkey;

	// Process candidates in queue:
	queue.insert( QueueElement( 0, 0, -1));
	while (!queue.empty())
	{
		QueueElement cur = *queue.begin();
		queue.erase( queue.begin());

		if (!solutions.empty() && solutions[0].nofUnresolved < cur.nofUnresolved)
		{
			// ... no solution available anymore with less unresolved as queue is sorted ascending by nofUnresolved
			break;
		}
		KeyCursor keyCursor( field, cur.pos, m_spaceSubst, m_linkSubst);
		if (!keyCursor.hasMore())
		{
			solutions.push_back( Solution( cur.predidx, cur.nofUnresolved));
		}
		else
		{
			keyCursor.skipToken();
			if (keyCursor.tryLoad( m_cursor, loadkey))
			{
				int endTokenPos = cur.pos + loadkey.size();
				int successorPos = (keyCursor.isSpace()) ? endTokenPos+1 : endTokenPos;
				queue.insert( QueueElement( cur.nofUnresolved, successorPos, elemar.size()));
				elemar.push_back( SolutionElement( cur.pos, endTokenPos, cur.predidx, true));
				keyCursor.setPosition( loadkey.size());

				while (keyCursor.hasMore())
				{
					keyCursor.skipToken();
					if (keyCursor.tryLoad( m_cursor, loadkey))
					{
						endTokenPos = cur.pos + loadkey.size();
						successorPos = (keyCursor.isSpace()) ? endTokenPos+1 : endTokenPos;
						queue.insert( QueueElement( cur.nofUnresolved, successorPos, elemar.size()));
						elemar.push_back( SolutionElement( cur.pos, endTokenPos, cur.predidx, true));
						keyCursor.setPosition( loadkey.size());
					}
				}
			}
			else
			{
				int endTokenPos = cur.pos + keyCursor.keypos();
				int successorPos = (keyCursor.isSpace()) ? endTokenPos+1 : endTokenPos;
				if (keyCursor.currentTokenIsWord())
				{
					queue.insert( QueueElement( cur.nofUnresolved+1, successorPos, elemar.size()));
					elemar.push_back( SolutionElement( cur.pos, endTokenPos, cur.predidx, false));
				}
				else
				{
					queue.insert( QueueElement( cur.nofUnresolved, successorPos, cur.predidx));
				}
			}
		}
	}
	// Build the solution list:
	std::vector<Solution>::const_iterator si = solutions.begin(), se = solutions.end();
	for (; si != se; ++si)
	{
		rt.push_back( ItemList());
		ItemList& sl = rt.back();
		int ei = si->idx;
		while (ei >= 0)
		{
			const SolutionElement& elem = elemar[ ei];
			ei = elem.predidx;
			sl.push_back( Item( elem.startpos, elem.endpos, elem.resolved));
		}
		std::reverse( sl.begin(), sl.end());
	}
	return rt;
}



