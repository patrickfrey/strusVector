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
#include <set>
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

static std::size_t initKeyBuf( char* keyBuf, std::size_t keyBufSize, const char* fieldPtr, std::size_t fieldSize, std::size_t pos)
{
	std::size_t keySize = fieldSize - pos;
	if (keySize >= keyBufSize) keySize = keyBufSize-1;
	std::memcpy( keyBuf, fieldPtr + pos, keySize);
	keyBuf[ keySize] = 0;
	return keySize;
}

struct SolutionElement
{
	strus::Index featno;
	int startpos;
	int endpos;
	int predidx;

	SolutionElement( strus::Index featno_, int startpos_, int endpos_, int predidx_)
		:featno(featno_),startpos(startpos_),endpos(endpos_),predidx(predidx_){}
	SolutionElement( const SolutionElement& o)
		:featno(o.featno),startpos(o.startpos),endpos(o.endpos),predidx(o.predidx){}
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
	int size;

	QueueElement( int nofUnresolved_, int pos_, int predidx_, int size_)
		:nofUnresolved(nofUnresolved_),pos(pos_),predidx(predidx_),size(size_){}
	QueueElement( const QueueElement& o)
		:nofUnresolved(o.nofUnresolved),pos(o.pos),predidx(o.predidx),size(o.size){}

	bool operator < (const QueueElement& o) const
	{
		return nofUnresolved == o.nofUnresolved
				? pos == o.pos
					? size == o.size
						? predidx < o.predidx
						: size < o.size
					: pos > o.pos

				: nofUnresolved < o.nofUnresolved;
	}
};


struct KeyCursor
{
	enum {
		MaxKeyLen=256
	};

	KeyCursor( const std::string& field_, int curpos_, char spaceSubst_, char linkSubst_)
		:keySize(0),fieldPtr(field_.c_str()),fieldSize(field_.size()),curpos(curpos_)
		,kitr(0),key(0),spaceSubst(spaceSubst_),linkSubst(linkSubst_)
	{
		keySize = initKeyBuf( keyBuf, MaxKeyLen, fieldPtr, fieldSize, curpos);
		kitr = key = keyBuf;
	}
	KeyCursor( const KeyCursor& o)
		:keySize(o.keySize),fieldPtr(o.fieldPtr),fieldSize(o.fieldSize),curpos(o.curpos)
		,kitr(o.kitr),key(o.key),spaceSubst(o.spaceSubst),linkSubst(o.linkSubst)
	{
		std::memcpy( keyBuf, o.keyBuf, o.keySize+1);
	}

	void skipToken()
	{
		if (*kitr == linkSubst)
		{
			++kitr;
		}
		else
		{
			if (*kitr == spaceSubst) ++kitr;
			for (;*kitr && *kitr != spaceSubst && *kitr != linkSubst; ++kitr){}
		}
	}

	bool isSpace() const
	{
		return *kitr == spaceSubst;
	}

	bool isSpaceAt( std::size_t pos) const
	{
		return key[pos] == spaceSubst;
	}

	bool isLink() const
	{
		return *kitr == linkSubst;
	}

	bool isLinkAt( std::size_t pos) const
	{
		return key[pos] == linkSubst;
	}

	bool isSeparator() const
	{
		return *kitr == linkSubst || *kitr == spaceSubst; 
	}

	bool isSeparatorAt( std::size_t pos) const
	{
		return key[pos] == linkSubst || key[pos] == spaceSubst;
	}

	void changeSpaceToLink()
	{
		if (!isSpace()) throw std::runtime_error(_TXT("logic error: invalid operation"));
		*kitr = linkSubst;
	}

	bool isEqualField( char const* c1, char const* c2, std::size_t csize) const
	{
		for (; csize > 0; ++c1,++c2,--csize)
		{
			if (*c1 != *c2)
			{
				if (*c1 == spaceSubst && *c2 == linkSubst) continue;
				if (*c2 == spaceSubst && *c1 == linkSubst) continue;
				return false;
			}
		}
		return true;
	}

	bool cursorSkipPrefix( DatabaseAdapter::FeatureCursor& cursor, int kofs, std::string& loadbuf)
	{
		return cursor.skipPrefix( key, kofs, loadbuf);
	}

	bool tryLoad( DatabaseAdapter::FeatureCursor& cursor, std::string& loadbuf, strus::Index& featno, int& keylen)
	{
		int kofs = kitr-key;
		while (cursorSkipPrefix( cursor, kofs, loadbuf))
		{
			if (loadbuf.size() <= (fieldSize-curpos))
			{
				keylen = loadbuf.size();
				if (isEqualField( fieldPtr+curpos, loadbuf.c_str(), loadbuf.size())
				&& ((fieldSize-curpos) == loadbuf.size()
					|| fieldPtr[ keylen] == spaceSubst
					|| fieldPtr[ keylen] == linkSubst
					|| fieldPtr[ keylen-1] == spaceSubst
					|| fieldPtr[ keylen-1] == linkSubst))
				{
					featno = cursor.getCurrentFeatureIndex();
					return true;
				}
			}
			else if (loadbuf.size() == (fieldSize-curpos+1))
			{
				keylen = loadbuf.size()-1;
				if (isEqualField( fieldPtr+curpos, loadbuf.c_str(), keylen)
				&&  loadbuf[ keylen] == linkSubst)
				{
					featno = cursor.getCurrentFeatureIndex();
					return true;
				}
			}
			if (!key[kofs]) break;
			++kofs;
		}
		featno = 0;
		keylen = 0;
		return false;
	}

	bool tryLoadNext( DatabaseAdapter::FeatureCursor& cursor, std::string& loadbuf, strus::Index& featno, int& keylen)
	{
		skipToken();
		return tryLoad( cursor, loadbuf, featno, keylen);
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

	char keyBuf[ MaxKeyLen];
	std::size_t keySize;
	const char* fieldPtr;
	std::size_t fieldSize;
	int curpos;
	char* kitr;
	char* key;
	char spaceSubst;
	char linkSubst;
};

std::vector<SentenceLexerKeySearch::ItemList> SentenceLexerKeySearch::scanField( const std::string& field)
{
	std::set<ItemList> resultItemList;
	std::vector<SolutionElement> elemar;
	std::set<QueueElement> queue;
	std::vector<Solution> solutions;
	std::vector<KeyCursor> keyCursorStack;
	std::string loadkey;
	int minsize = -1;

	// Process candidates in queue:
	queue.insert( QueueElement( 0, 0, -1, 0));
	while (!queue.empty())
	{
		QueueElement cur = *queue.begin();
		queue.erase( queue.begin());

		if (!solutions.empty())
		{
			if (solutions[0].nofUnresolved < cur.nofUnresolved)
			{
				// ... no solution available anymore with less unresolved as queue is sorted ascending by nofUnresolved
				break;
			}
			if (minsize != -1 && cur.size > minsize + 1)
			{
				continue;
			}
		}
		KeyCursor keyCursor( field, cur.pos, m_spaceSubst, m_linkSubst);
		if (!keyCursor.hasMore())
		{
			if (minsize == -1 || minsize > cur.size)
			{
				minsize = cur.size;
			}
			solutions.push_back( Solution( cur.predidx, cur.nofUnresolved));
		}
		else
		{
			strus::Index featno = 0;
			int keylen;
			keyCursor.skipToken();

			if (keyCursor.tryLoad( m_cursor, loadkey, featno, keylen))
			{
				// ... found feature with the key, then examine all variants

				// Push found element and feed the queue with the successor:
				int endTokenPos = cur.pos + keylen;
				int successorPos = (keyCursor.isSeparatorAt( keylen)) ? endTokenPos+1 : endTokenPos;
				if (successorPos > (int)field.size()) throw strus::runtime_error(_TXT("logic error: field position out of range: %d"), successorPos);
				queue.insert( QueueElement( cur.nofUnresolved, successorPos, elemar.size(), cur.size+1));
				elemar.push_back( SolutionElement( featno, cur.pos, endTokenPos, cur.predidx));

				if (!keyCursor.currentTokenIsWord())
				{
					//... skip lonely link char
					endTokenPos = cur.pos + keyCursor.keypos();
					successorPos = endTokenPos;
					if (successorPos > (int)field.size()) throw strus::runtime_error(_TXT("logic error: field position out of range: %d"), successorPos);
					queue.insert( QueueElement( cur.nofUnresolved, successorPos, cur.predidx, cur.size));
				}
				// Set the key position to the found key length and
				//	push the key with the first space changed to a link 
				//	on the alternative variant stack:
				keyCursorStack.clear();
				keyCursorStack.push_back( keyCursor);
				keyCursor.setPosition( keylen);
				while (keyCursorStack.back().keypos() < keyCursor.keypos())
				{
					if (keyCursorStack.back().isSpace())
					{
						// ... in case of a space we also search keys combined with a link at this place
						keyCursorStack.back().changeSpaceToLink();
						break;
					}
					keyCursorStack.back().skipToken();
				}
				if (keyCursorStack.back().keypos() == keyCursor.keypos())
				{
					keyCursorStack.pop_back();
				}
				if (keyCursor.isSpace())
				{
					keyCursorStack.push_back( keyCursor);
					keyCursorStack.back().changeSpaceToLink();
				}

				// Expand the key and all its variants with the a space changed to a link on the key variant stack
				bool hasMoreKeyCursors = true;
				while (hasMoreKeyCursors)
				{
					while (keyCursor.hasMore() && keyCursor.tryLoadNext( m_cursor, loadkey, featno, keylen))
					{
						// Push found element and feed the queue with the successor:
						endTokenPos = cur.pos + keylen;
						successorPos = (keyCursor.isSeparatorAt(keylen)) ? endTokenPos+1 : endTokenPos;
						if (successorPos > (int)field.size()) throw strus::runtime_error(_TXT("logic error: field position out of range: %d"), successorPos);
						queue.insert( QueueElement( cur.nofUnresolved, successorPos, elemar.size(), cur.size+1));
						elemar.push_back( SolutionElement( featno, cur.pos, endTokenPos, cur.predidx));

						// Set the key position to the found key length and
						//	push the key with the first space changed to a link 
						//	on the alternative variant stack:
						keyCursorStack.push_back( keyCursor);
						keyCursor.setPosition( keylen);
						while (keyCursorStack.back().keypos() < keyCursor.keypos())
						{
							if (keyCursorStack.back().isSpace())
							{
								// ... in case of a space we also search keys combined with a link at this place
								keyCursorStack.back().changeSpaceToLink();
								break;
							}
							keyCursorStack.back().skipToken();
						}
						if (keyCursorStack.back().keypos() == keyCursor.keypos())
						{
							keyCursorStack.pop_back();
						}
						if (keyCursor.isSpace())
						{
							keyCursorStack.push_back( keyCursor);
							keyCursorStack.back().changeSpaceToLink();
						}
					}
					if (!keyCursorStack.empty())
					{
						keyCursor = keyCursorStack.back();
						keyCursorStack.pop_back();
						hasMoreKeyCursors = true;
					}
					else
					{
						hasMoreKeyCursors = false;
					}
				}
			}
			else
			{
				//... No feature found with this key, then push it as unknown
				int endTokenPos = cur.pos + keyCursor.keypos();
				int successorPos = (keyCursor.isSeparator()) ? endTokenPos+1 : endTokenPos;
				if (successorPos > (int)field.size()) throw strus::runtime_error(_TXT("logic error: field position out of range: %d"), successorPos);
				if (keyCursor.currentTokenIsWord())
				{
					queue.insert( QueueElement( cur.nofUnresolved+1, successorPos, elemar.size(), cur.size+1));
					elemar.push_back( SolutionElement( 0/*featno (unresolved)*/, cur.pos, endTokenPos, cur.predidx));
				}
				else
				{
					queue.insert( QueueElement( cur.nofUnresolved, successorPos, cur.predidx, cur.size));
				}
			}
		}
	}
	// Build the solution list:
	std::vector<Solution>::const_iterator si = solutions.begin(), se = solutions.end();
	for (; si != se; ++si)
	{
		if (si->idx < 0) continue;

		ItemList sl;
		int ei = si->idx;
		while (ei >= 0)
		{
			const SolutionElement& elem = elemar[ ei];
			ei = elem.predidx;
			sl.push_back( Item( elem.featno, elem.startpos, elem.endpos));
		}
		std::reverse( sl.begin(), sl.end());
		resultItemList.insert( sl);
	}
	return std::vector<ItemList>( resultItemList.begin(), resultItemList.end());
}



