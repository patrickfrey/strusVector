/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Vector repository transaction implementation
#include "vectorStorageTransaction.hpp"
#include "vectorStorageClient.hpp"
#include "getSimhashValues.hpp"
#include "errorUtils.hpp"
#include "internationalization.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/base/string_conv.hpp"
#include <algorithm>

#define MODULENAME   "standard vector storage"

using namespace strus;

VectorStorageTransaction::VectorStorageTransaction(
		VectorStorageClient* storage_,
		Reference<DatabaseAdapter>& database_,
		const VectorStorageConfig& config_,
		ErrorBufferInterface* errorhnd_)
	:m_errorhnd(errorhnd_),m_storage(storage_),m_config(config_)
	,m_database(database_),m_transaction(database_->createTransaction())
{
	if (!m_transaction.get())
	{
		throw strus::runtime_error( "%s", _TXT("failed to create transaction"));
	}
}

void VectorStorageTransaction::addFeature( const std::string& name, const std::vector<double>& vec)
{
	try
	{
		m_vecar.push_back( vec);
		StringConvError err = StringConvOk;
		m_namear.push_back( strus::utf8clean( name, err));
		if (err != StringConvOk) throw strus::stringconv_exception( err);
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error adding feature to '%s': %s"), MODULENAME, *m_errorhnd);
}

void VectorStorageTransaction::defineFeatureConceptRelation( const std::string& relationTypeName, const Index& featidx, const Index& conidx)
{
	try
	{
		ConceptTypeMap::const_iterator ci = m_conceptTypeMap.find( relationTypeName);
		unsigned int cidx;
		if (ci == m_conceptTypeMap.end())
		{
			m_conceptTypeMap[ relationTypeName] = cidx = m_conceptFeatureRelationList.size();
			m_conceptFeatureRelationList.push_back( Relation());
			m_featureConceptRelationList.push_back( Relation());
		}
		else
		{
			cidx = ci->second;
		}
		m_conceptFeatureRelationList[ cidx].insert( RelationDef( conidx, featidx));
		m_featureConceptRelationList[ cidx].insert( RelationDef( featidx, conidx));
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error defining feature concept relation in '%s': %s"), MODULENAME, *m_errorhnd);
}

bool VectorStorageTransaction::commit()
{
	try
	{
		VectorStorageClient::TransactionLock lock( m_storage);
		//... we need a lock because transactions need to be sequentialized

		unsigned int lastSampleIdx = m_database->readNofSamples();
		if (!m_vecar.empty())
		{
			LshModel lshmodel( m_database->readLshModel());
			std::vector<SimHash> samplear = strus::getSimhashValues( lshmodel, m_vecar, m_config.threads, m_errorhnd);
			std::size_t si = 0, se = m_vecar.size();
			for (; si != se; ++si)
			{
				m_transaction->writeSample( lastSampleIdx + si, m_namear[si], m_vecar[si], samplear[si]);
			}
			m_transaction->writeNofSamples( lastSampleIdx + si);
		}
		std::vector<std::string> clnames = m_database->readConceptClassNames();
		std::set<std::string> clset( clnames.begin(), clnames.end());
		bool conceptClassesUpdated = false;
		ConceptTypeMap::const_iterator ti = m_conceptTypeMap.begin(), te = m_conceptTypeMap.end();
		for (; ti != te; ++ti)
		{
			if (clset.find( ti->first) == clset.end())
			{
				clset.insert( ti->first);
				conceptClassesUpdated = true;
			}
		}
		if (conceptClassesUpdated)
		{
			m_transaction->writeConceptClassNames( std::vector<std::string>( clset.begin(), clset.end()));
		}
		ti = m_conceptTypeMap.begin();
		for (; ti != te; ++ti)
		{
			{
				const std::string& clname = ti->first;
				const Relation& insertset = m_conceptFeatureRelationList[ ti->second];
				Relation::const_iterator ri = insertset.begin(), re = insertset.end();
				while (ri != re)
				{
					std::vector<SampleIndex> new_sar;
					Relation::const_iterator rf = ri;
					for (; ri != re && rf->first == ri->first; ++ri)
					{
						new_sar.push_back( ri->second);
					}
					std::vector<SampleIndex> old_sar = m_database->readConceptSampleIndices( clname, rf->first);
					std::vector<SampleIndex> join_sar;
					std::merge( new_sar.begin(), new_sar.end(), old_sar.begin(), old_sar.end(),  std::back_inserter( join_sar));
					m_transaction->writeConceptSampleIndices( clname, rf->first, join_sar);
				}
			}
			{
				const std::string& clname = ti->first;
				const Relation& insertset = m_featureConceptRelationList[ti->second];
				Relation::const_iterator ri = insertset.begin(), re = insertset.end();
				while (ri != re)
				{
					std::vector<SampleIndex> new_sar;
					Relation::const_iterator rf = ri;
					for (; ri != re && rf->first == ri->first; ++ri)
					{
						new_sar.push_back( ri->second);
					}
					std::vector<SampleIndex> old_sar = m_database->readSampleConceptIndices( clname, rf->first);
					std::vector<SampleIndex> join_sar;
					std::merge( new_sar.begin(), new_sar.end(), old_sar.begin(), old_sar.end(),  std::back_inserter(join_sar));
					m_transaction->writeSampleConceptIndices( clname, rf->first, join_sar);
				}
			}
		}
		if (m_transaction->commit())
		{
			m_conceptTypeMap.clear();
			m_conceptFeatureRelationList.clear();
			m_featureConceptRelationList.clear();
			m_vecar.clear();
			m_namear.clear();
			return true;
		}
		else
		{
			return false;
		}
	}
	CATCH_ERROR_ARG1_MAP_RETURN( _TXT("error in commit of '%s' transaction: %s"), MODULENAME, *m_errorhnd, false);
}

void VectorStorageTransaction::rollback()
{
	try
	{
		m_transaction->rollback();
		m_conceptTypeMap.clear();
		m_conceptFeatureRelationList.clear();
		m_featureConceptRelationList.clear();
		m_vecar.clear();
		m_namear.clear();
	}
	CATCH_ERROR_ARG1_MAP( _TXT("error in rollback of '%s' transaction: %s"), MODULENAME, *m_errorhnd);
}


