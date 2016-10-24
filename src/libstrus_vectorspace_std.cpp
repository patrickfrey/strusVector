/*
 * Copyright (c) 2016 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/vectorspace_std.hpp"
#include "strus/errorBufferInterface.hpp"
#include "vectorSpaceModel.hpp"
#include "strus/base/dll_tags.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"

using namespace strus;

DLL_PUBLIC VectorSpaceModelInterface* strus::createVectorSpaceModel_std( ErrorBufferInterface* errorhnd)
{
	try
	{
		static bool intl_initialized = false;
		if (!intl_initialized)
		{
			strus::initMessageTextDomain();
			intl_initialized = true;
		}
		return new VectorSpaceModel( errorhnd);
	}
	CATCH_ERROR_MAP_RETURN( _TXT("error creating standard vector space model: %s"), *errorhnd, 0);
}
