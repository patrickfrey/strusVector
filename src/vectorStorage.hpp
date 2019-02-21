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
#include "strus/debugTraceInterface.hpp"
#include <string>

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
	enum DefaultConfigValues
	{
		DefaultDim = 300,
		DefaultBits = 64,
		DefaultVariations = 32,
		DefaultSim = 340,
		DefaultProbSim = 640
	};

	struct Config
	{
		int vecdim;
		int bits;
		int variations;
		int simdist;
		int probsimdist;

		Config( const Config& o)
			:vecdim(o.vecdim),bits(o.bits),variations(o.variations),simdist(o.simdist),probsimdist(o.probsimdist){}
		Config()
			:vecdim(DefaultDim),bits(DefaultBits),variations(DefaultVariations),simdist(DefaultSim),probsimdist(DefaultProbSim){}
		explicit Config( int vecdim_)
			:vecdim(vecdim_)
			,bits(bitsFromVecdim(vecdim_))
			,variations(variationsFromVecdim(vecdim_))
			,simdist(simdistFromVecdim(vecdim_))
			,probsimdist(probsimdistFromVecdim(vecdim_/9))
		{
			while (vecdim/2 < bits && bits > 1)
			{
				bits /= 2;
				variations *= 2;
			}
		}

		static int bitsFromVecdim( int vecdim_)
		{
			int rt = 64;
			while (vecdim_/2 < rt && rt > 1)
			{
				rt /= 2;
			}
			return rt;
		}
		static int variationsFromVecdim( int vecdim_)
		{
			int bits_ = bitsFromVecdim( vecdim_);
			return (vecdim_ * 640) / (93 * bits_);
		}
		static int simdistFromVecdim( int vecdim_)
		{
			return (vecdim_ + vecdim_/9);
		}
		static int probsimdistFromVecdim( int vecdim_)
		{
			return (2 * vecdim_ + vecdim_/9);
		}
	};

	VectorStorage( const std::string& workdir_, ErrorBufferInterface* errorhnd_);
	virtual ~VectorStorage();

	virtual bool createStorage( const std::string& configsource, const DatabaseInterface* database) const;

	virtual VectorStorageClientInterface* createClient( const std::string& configsource, const DatabaseInterface* database) const;
	virtual VectorStorageDumpInterface* createDump( const std::string& configsource, const DatabaseInterface* database) const;

	virtual const char* getConfigDescription( const ConfigType& type) const;
	virtual const char** getConfigParameters( const ConfigType& type) const;

private:
	ErrorBufferInterface* m_errorhnd;		///< buffer for reporting errors
	DebugTraceContextInterface* m_debugtrace;	///< debug trace interface
	std::string m_workdir;				///< working directory
};

}//namespace
#endif

