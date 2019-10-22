/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/vector_std.hpp"
#include "strus/errorBufferInterface.hpp"
#include "vectorStorage.hpp"
#include "strus/base/dll_tags.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"

using namespace strus;

DLL_PUBLIC VectorStorageInterface* strus::createVectorStorage_std( const FileLocatorInterface* filelocator, ErrorBufferInterface* errorhnd)
{
	try
	{
		static bool intl_initialized = false;
		if (!intl_initialized)
		{
			strus::initMessageTextDomain();
			intl_initialized = true;
		}
		return new VectorStorage( filelocator, errorhnd);
	}
	CATCH_ERROR_MAP_RETURN( _TXT("error creating vector storage: %s"), *errorhnd, 0);
}

