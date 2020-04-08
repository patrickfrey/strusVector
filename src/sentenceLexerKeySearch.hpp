/*
 * Copyright (c) 2020 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Structure for searching alternative splits of a field
/// \file sentenceLexerKeySearch.hpp
#ifndef _STRUS_SENTENCE_LEXER_KEY_SEARCH_HPP_INCLUDED
#define _STRUS_SENTENCE_LEXER_KEY_SEARCH_HPP_INCLUDED
#include "databaseAdapter.hpp"
#include "vectorStorageClient.hpp"
#include <vector>
#include <string>
#include <algorithm>

namespace strus {

/// \brief Forward declaration
class DatabaseClientInterface;
/// \brief Forward declaration
class ErrorBufferInterface;

struct SentenceLexerKeySearch
{
public:
	struct Item
	{
		strus::Index featno;
		int startpos;
		int endpos;

		Item( strus::Index featno_, int startpos_, int endpos_)
			:featno(featno_),startpos(startpos_),endpos(endpos_){}
		Item( const Item& o)
			:featno(o.featno),startpos(o.startpos),endpos(o.endpos){}

		bool operator < (const Item& o) const
		{
			return featno == o.featno
				? startpos == o.startpos
					? endpos < o.endpos
					: startpos < o.startpos
				: featno < o.featno;
		}
	};
	typedef std::vector<Item> ItemList;

	SentenceLexerKeySearch(
		const VectorStorageClient* vstorage_,
		const DatabaseClientInterface* database_,
		ErrorBufferInterface* errorhnd_,
		char spaceSubst_,
		char linkSubst_);

	std::vector<ItemList> scanField( const std::string& field);

private:
	ErrorBufferInterface* m_errorhnd;
	const VectorStorageClient* m_vstorage;
	DatabaseAdapter::FeatureCursor m_cursor;
	char m_spaceSubst;
	char m_linkSubst;
};

}//namespace
#endif



