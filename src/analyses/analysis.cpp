///////////////////////////////////////////////////////////////////////////////
// Name:        analysis.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "analysis.h"
#include "descriptivestatistics.h"

namespace Wisteria::Analyses
    {
    //-------------------------------------------
    std::unique_ptr<Analysis> Analysis::Create(const wxString& typeName)
        {
        if (typeName.CmpNoCase(DescriptiveStatistics::GetTypeNameStatic()) == 0)
            {
            return std::make_unique<DescriptiveStatistics>();
            }
        return nullptr;
        }
    } // namespace Wisteria::Analyses
