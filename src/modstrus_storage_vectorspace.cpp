/*
 * Copyright (c) 2014 Patrick P. Frey
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#include "strus/lib/vectorspace_std.hpp"
#include "strus/errorBufferInterface.hpp"
#include "strus/base/dll_tags.hpp"
#include "strus/storageModule.hpp"
#include "internationalization.hpp"
#include "errorUtils.hpp"

static const strus::VectorSpaceModelReference standard_vsmodel =
{
	"default", strus::createVectorSpaceModel_std
};

extern "C" DLL_PUBLIC strus::StorageModule entryPoint;

strus::StorageModule entryPoint( &standard_vsmodel);




