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
#include "strus/storage/index.hpp"
#include "databaseAdapter.hpp"
#include "simHash.hpp"
#include <string>
#include <map>

namespace strus {

/// \brief Reader interface to access LSH values from storage or memory
class SimHashReaderInterface
{
public:
	virtual ~SimHashReaderInterface(){}
	/// \brief Loads the first LSH value (for iteration)
	/// \note not thead-safe
	virtual const SimHash* loadFirst()=0;
	/// \brief Loads the next LSH value (for iteration)
	/// \note not thead-safe
	virtual const SimHash* loadNext()=0;

	/// \brief Loads a specific LSH value
	/// \param[in] id feature number of LSH value to retrieve
	/// \param[out] buf buffer to use for value read if needed, not necessarily used
	/// \return pointer to value loaded (value not necessarily in buf, depends on implementation)
	/// \note thead-safe
	virtual const SimHash* load( const Index& id, SimHash& buf) const=0;
};


class SimHashReaderDatabase
	:public SimHashReaderInterface
{
public:
	SimHashReaderDatabase( const DatabaseAdapter* database_, const std::string& type_);
	virtual ~SimHashReaderDatabase(){}

	virtual const SimHash* loadFirst();
	virtual const SimHash* loadNext();
	virtual const SimHash* load( const Index& featno, SimHash& buf) const;

private:
	enum {ReadChunkSize=1024};
	const DatabaseAdapter* m_database;
	std::string m_type;
	Index m_typeno;
	std::size_t m_aridx;
	std::vector<SimHash> m_ar;
	SimHash m_value;
};

class SimHashReaderMemory
	:public SimHashReaderInterface
{
public:
	SimHashReaderMemory( const DatabaseAdapter* database_, const std::string& type_);
	virtual ~SimHashReaderMemory(){}

	virtual const SimHash* loadFirst();
	virtual const SimHash* loadNext();
	virtual const SimHash* load( const Index& featno, SimHash& buf) const;

private:
	const DatabaseAdapter* m_database;
	std::string m_type;
	Index m_typeno;
	std::size_t m_aridx;
	std::vector<SimHash> m_ar;
	std::map<Index,std::size_t> m_indexmap;
};

}//namespace
#endif

