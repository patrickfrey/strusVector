/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Implementation of the vector storage interface
#ifndef _STRUS_VECTOR_STORAGE_IMPLEMENTATION_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_IMPLEMENTATION_HPP_INCLUDED
#include "strus/vectorStorageInterface.hpp"

namespace strus {

/// \brief Forward declaration
class VectorStorageClientInterface;
/// \brief Forward declaration
class VectorStorageDumpInterface;
/// \brief Forward declaration
class ErrorBufferInterface;
/// \brief Forward declaration
class DatabaseInterface;


/// \brief Standart vector storage based on LHS with sampling of representants with a genetic algorithm
class VectorStorage
	:public VectorStorageInterface
{
public:
	virtual ~VectorStorage(){}

	VectorStorage( ErrorBufferInterface* errorhnd_);

	virtual bool createStorage( const std::string& configsource, const DatabaseInterface* database) const;

	virtual VectorStorageClientInterface* createClient( const std::string& configsrc, const DatabaseInterface* database) const;
	virtual VectorStorageDumpInterface* createDump( const std::string& configsource, const DatabaseInterface* database, const std::string& keyprefix) const;

	virtual bool runBuild( const std::string& commands, const std::string& configsource, const DatabaseInterface* database);

private:
	ErrorBufferInterface* m_errorhnd;	///< buffer for reporting errors
};

}//namespace
#endif

