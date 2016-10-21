/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
/// \brief Loading and storing of structures of the strus standard vector space model
#include "vectorSpaceModelFiles.hpp"
#include "strus/base/fileio.hpp"
#include "strus/base/hton.hpp"
#include "strus/versionVector.hpp"
#include "internationalization.hpp"

using namespace strus;

struct VectorSpaceModelHdr
{
	enum {FILEID=0x3ff3};
	char name[58];
	unsigned short _id;
	unsigned short version_major;
	unsigned short version_minor;

	VectorSpaceModelHdr()
	{
		std::memcpy( name, "strus standard vector space model bin file\n\0", sizeof(name));
		_id = FILEID;
		version_major = STRUS_VECTOR_VERSION_MAJOR;
		version_minor = STRUS_VECTOR_VERSION_MINOR;
	}

	void check()
	{
		if (_id != FILEID) throw strus::runtime_error(_TXT("unknown file type, expected strus standard vector space model binary file"));
		if (version_major != STRUS_VECTOR_VERSION_MAJOR) throw strus::runtime_error(_TXT("major version (%u) of loaded strus standard vector space model binary file does not match"), version_major);
		if (version_minor > STRUS_VECTOR_VERSION_MINOR) throw strus::runtime_error(_TXT("minor version (%u) of loaded strus standard vector space model binary file does not match (newer than your version of strus)"), version_minor);
	}

	void hton()
	{
		_id = ByteOrder<unsigned short>::hton( _id);
		version_major = ByteOrder<unsigned short>::hton( version_major);
		version_minor = ByteOrder<unsigned short>::hton( version_minor);
	}

	void ntoh()
	{
		_id = ByteOrder<unsigned short>::ntoh( _id);
		version_major = ByteOrder<unsigned short>::ntoh( version_major);
		version_minor = ByteOrder<unsigned short>::ntoh( version_minor);
	}
};

void strus::checkVersionFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load model version from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	VectorSpaceModelHdr hdr;
	if (content.size() != sizeof(hdr)) throw strus::runtime_error(_TXT("unknown file format"));
	std::memcpy( &hdr, content.c_str(), sizeof(hdr));
	hdr.ntoh();
	hdr.check();
}

void strus::writeVersionToFile( const std::string& filename)
{
	std::string content;
	VectorSpaceModelHdr hdr;
	hdr.hton();
	content.append( (const char*)&hdr, sizeof(hdr));
	unsigned int ec = writeFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to write model version to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

void strus::writeDumpToFile( const std::string& content, const std::string& filename)
{
	unsigned int ec = writeFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to write text dump to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

std::vector<SimHash> strus::readSimHashVectorFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load lsh values from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return SimHash::createFromSerialization( content);
}

void strus::writeSimHashVectorToFile( const std::vector<SimHash>& ar, const std::string& filename)
{
	unsigned int ec = writeFile( filename, SimHash::serialization( ar));
	if (ec) throw strus::runtime_error(_TXT("failed to write lsh values to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

VectorSpaceModelConfig strus::readConfigurationFromFile( const std::string& filename, ErrorBufferInterface* errorhnd)
{
	VectorSpaceModelConfig rt;
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load config from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	try
	{
		rt = VectorSpaceModelConfig( content, errorhnd);
	}
	catch (const std::runtime_error& err)
	{
		throw strus::runtime_error(_TXT("error in vector space model configuration: %s"), err.what());
	}
	return rt;
}

void strus::writeConfigurationToFile( const VectorSpaceModelConfig& config, const std::string& filename)
{
	unsigned int ec = writeFile( filename, config.tostring());
	if (ec) throw strus::runtime_error(_TXT("failed to configuration to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

LshModel strus::readLshModelFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to load model from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return LshModel::fromSerialization( content);
}

void strus::writeLshModelToFile( const LshModel& model, const std::string& filename)
{
	unsigned int ec = writeFile( filename, model.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write lsh values to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

SimRelationMap strus::readSimRelationMapFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to similarity relation map from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return SimRelationMap::fromSerialization( content);
}

void strus::writeSimRelationMapToFile( const SimRelationMap& simrelmap, const std::string& filename)
{
	unsigned int ec = writeFile( filename, simrelmap.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write similarity relation map to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

SampleFeatureIndexMap strus::readSampleFeatureIndexMapFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to read sample feature index map from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return SampleFeatureIndexMap::fromSerialization( content);
}

void strus::writeSampleFeatureIndexMapToFile( const SampleFeatureIndexMap& map, const std::string& filename)
{
	unsigned int ec = writeFile( filename, map.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write sample feature index map to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

FeatureSampleIndexMap strus::readFeatureSampleIndexMapFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to read sample feature index map from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return FeatureSampleIndexMap::fromSerialization( content);
}

void strus::writeFeatureSampleIndexMapToFile( const FeatureSampleIndexMap& map, const std::string& filename)
{
	unsigned int ec = writeFile( filename, map.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write sample feature index map to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

StringList strus::readSampleNamesFromFile( const std::string& filename)
{
	std::string content;
	unsigned int ec = readFile( filename, content);
	if (ec) throw strus::runtime_error(_TXT("failed to read sample name list from file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
	return StringList::fromSerialization( content);
}

void strus::writeSampleNamesToFile( const StringList& names, const std::string& filename)
{
	unsigned int ec = writeFile( filename, names.serialization());
	if (ec) throw strus::runtime_error(_TXT("failed to write sample name list to file '%s' (errno %u): %s"), filename.c_str(), ec, ::strerror(ec));
}

