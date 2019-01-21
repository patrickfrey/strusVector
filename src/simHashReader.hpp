/*
 * Copyright (c) 2018 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Reader to access LSH values from storage or memory
#ifndef _STRUS_VECTOR_SIMHASH_READER_HPP_INCLUDED
#define _STRUS_VECTOR_SIMHASH_READER_HPP_INCLUDED
#include "strus/index.hpp"
#include "databaseAdapter.hpp"
#include "simHash.hpp"
#include <string>

namespace strus {

/// \brief Reader interface to access LSH values from storage or memory
class SimHashReaderInterface
{
public:
	virtual const SimHash* loadFirst()=0;
	virtual const SimHash* loadNext()=0;
	virtual SimHash load( const Index& id) const=0;
};


class SimHashReaderDatabase
	:public SimHashReaderInterface
{
public:
	SimHashReaderDatabase( const DatabaseAdapter* database_, const std::string& type_);

	virtual const SimHash* loadFirst();
	virtual const SimHash* loadNext();
	virtual SimHash load( const Index& featno) const;

private:
	enum {ReadChunkSize=1024};
	const DatabaseAdapter* m_database;
	std::string m_type;
	Index m_typeno;
	std::size_t m_aridx;
	std::vector<SimHash> m_ar;
};

}//namespace
#endif

