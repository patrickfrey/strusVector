/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Database abstraction (wrapper) for the standard strus vector storage
#ifndef _STRUS_VECTOR_STORAGE_DATABASE_ADAPTER_HPP_INCLUDED
#define _STRUS_VECTOR_STORAGE_DATABASE_ADAPTER_HPP_INCLUDED
#include "strus/databaseInterface.hpp"
#include "strus/databaseClientInterface.hpp"
#include "strus/databaseTransactionInterface.hpp"
#include "strus/databaseCursorInterface.hpp"
#include "strus/reference.hpp"
#include "strus/base/stdint.h"
#include "vectorStorageConfig.hpp"
#include "simHash.hpp"
#include "simRelationMap.hpp"
#include "genModel.hpp"
#include "lshModel.hpp"
#include "stringList.hpp"
#include <vector>
#include <string>
#include <iostream>

namespace strus {

/// \brief Forward declaration
class ErrorBufferInterface;

class DatabaseAdapter
{
public:
	DatabaseAdapter( const DatabaseInterface* database_, const std::string& config, ErrorBufferInterface* errorhnd_);
	DatabaseAdapter( const DatabaseAdapter& o);
	~DatabaseAdapter(){}

	void checkVersion();

	typedef std::vector<double> Vector;
	Vector readSampleVector( const SampleIndex& sidx) const;
	std::string readSampleName( const SampleIndex& sidx) const;
	SampleIndex readSampleIndex( const std::string& name) const;
	SampleIndex readNofSamples() const;
	SimHash readSampleSimhash( const SampleIndex& sidx) const;
	std::vector<SimHash> readSampleSimhashVector( const SampleIndex& range_from, const SampleIndex& range_to) const;

	ConceptIndex readNofConcepts( const std::string& clname) const;
	std::vector<std::pair<std::string,uint64_t> > readVariables() const;

	std::vector<std::string> readConceptClassNames() const;

	SampleIndex readNofSimRelations() const;
	SampleIndex readLastSimRelationIndex() const;
	std::vector<SimRelationMap::Element> readSimRelations( const SampleIndex& sidx) const;
	SimRelationMap readSimRelationMap() const;
	std::vector<SampleIndex> readSimSingletons() const;

	std::vector<SampleIndex> readConceptSampleIndices( const std::string& clname, const ConceptIndex& cidx) const;
	ConceptSampleIndexMap readConceptSampleIndexMap( const std::string& clname) const;
	std::vector<ConceptIndex> readSampleConceptIndices( const std::string& clname, const SampleIndex& sidx) const;
	SampleConceptIndexMap readSampleConceptIndexMap( const std::string& clname) const;
	std::vector<SampleIndex> readConceptSingletons( const std::string& clname) const;

	VectorStorageConfig readConfig() const;
	LshModel readLshModel() const;
	bool isempty();
	void close();

public:
	enum KeyPrefix
	{
		KeyVersion = 'V',
		KeyVariable = '*',
		KeyConceptClassNames = 'K',
		KeySampleVector = '$',
		KeySampleName = '@',
		KeySampleNameInv = '#',
		KeySampleSimHash = 'S',
		KeyConfig = 'C',
		KeyLshModel = 'L',
		KeySimRelationMap = 'M',
		KeySampleConceptIndexMap = 'f',
		KeyConceptSampleIndexMap = 's'
	};

	class Transaction
	{
	public:
		Transaction( DatabaseClientInterface* database, ErrorBufferInterface* errorhnd_);

		void writeVersion();
		void writeNofSamples( const SampleIndex& nofSamples);
		void writeSample( const SampleIndex& sidx, const std::string& name, const Vector& vec, const SimHash& simHash);

		void writeNofConcepts( const std::string& clname, const ConceptIndex& nofConcepts);
		void writeConceptClassNames( const std::vector<std::string>& clnames);

		void writeNofSimRelations( const SampleIndex& nofSamples);
		void writeSimRelationMap( const SimRelationMap& simrelmap);
		void writeSimRelationRow( const SampleIndex& sidx, const SimRelationMap::Row& row);

		void writeSampleConceptIndexMap( const std::string& clname, const SampleConceptIndexMap& sfmap);
		void writeConceptSampleIndexMap( const std::string& clname, const ConceptSampleIndexMap& fsmap);
		void writeSampleConceptIndices( const std::string& clname, const SampleIndex& sidx, const std::vector<ConceptIndex>& ar);
		void writeConceptSampleIndices( const std::string& clname, const ConceptIndex& cidx, const std::vector<SampleIndex>& ar);

		void writeConfig( const VectorStorageConfig& config);
		void writeLshModel( const LshModel& model);
	
		void deleteData();
	
		void deleteConfig();
		void deleteVariables();
		void deleteConceptClassNames();

		void deleteSamples();
		void deleteSampleSimhashVectors();

		void deleteSimRelationMap();
		void deleteSampleConceptIndexMaps();
		void deleteConceptSampleIndexMaps();

		void deleteLshModel();

		bool commit();
		void rollback();

	private:
		void writeSimhash( const KeyPrefix& prefix, const SampleIndex& sidx, const SimHash& simHash);
		void writeSimhashVector( const KeyPrefix& prefix, const std::vector<SimHash>& ar);
		void writeSampleIndex( const SampleIndex& sidx, const std::string& name);
		void writeSampleName( const SampleIndex& sidx, const std::string& name);
		void writeSampleVector( const SampleIndex& sidx, const Vector& vec);
		void writeVariable( const std::string& name, unsigned int value);
	
		void deleteSubTree( const KeyPrefix& prefix);

	private:
		ErrorBufferInterface* m_errorhnd;
		Reference<DatabaseTransactionInterface> m_transaction;
	};

	Transaction* createTransaction()
	{
		return new Transaction( m_database.get(), m_errorhnd);
	}
	std::string config() const		{return m_database->config();}

	class DumpIterator
	{
	public:
		DumpIterator( const DatabaseClientInterface* database, const std::string& cursorkey_, ErrorBufferInterface* errorhnd_);

		bool dumpNext( std::ostream& out);

	private:
		void dumpKeyValue( std::ostream& out, const strus::DatabaseCursorInterface::Slice& key, const strus::DatabaseCursorInterface::Slice& value);

	private:
		ErrorBufferInterface* m_errorhnd;
		Reference<DatabaseCursorInterface> m_cursor;
		std::string m_cursorkey;
		bool m_first;
	};

	DumpIterator* createDumpIterator( const std::string& cursorkey_) const
	{
		return new DumpIterator( m_database.get(), cursorkey_, m_errorhnd);
	}

private:
	SimHash readSimhash( const KeyPrefix& prefix, const SampleIndex& sidx) const;
	std::vector<SimHash> readSimhashVector( const KeyPrefix& prefix, const SampleIndex& range_from, const SampleIndex& range_to) const;
	unsigned int readVariable( const std::string& name) const;
	IndexListMap<strus::Index,strus::Index> readIndexListMap( const KeyPrefix& prefix, const std::string& clname) const;

private:
	Reference<DatabaseClientInterface> m_database;
	ErrorBufferInterface* m_errorhnd;
};


} //namespace
#endif

