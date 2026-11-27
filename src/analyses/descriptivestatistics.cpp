///////////////////////////////////////////////////////////////////////////////
// Name:        descriptivestatistics.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "descriptivestatistics.h"
#include "../math/statistics.h"
#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <map>
#include <stdexcept>

namespace Wisteria::Analyses
    {
    //-------------------------------------------
    std::vector<AnalysisResultTable>
    DescriptiveStatistics::Run(const Data::Dataset& source, std::vector<wxString>& warnings) const
        {
        // skip the columns that aren't continuous columns in the dataset
        std::vector<wxString> foundColumns;
        for (const auto& colName : m_columns)
            {
            if (source.GetContinuousColumn(colName) == source.GetContinuousColumns().cend())
                {
                warnings.push_back(
                    wxString::Format(_(L"'%s': continuous column not found in dataset '%s'. "
                                       "It was left out of the '%s' analysis."),
                                     colName, GetSourceDatasetName(), GetName()));
                }
            else
                {
                foundColumns.push_back(colName);
                }
            }
        if (foundColumns.empty())
            {
            throw std::runtime_error(
                wxString::Format(_(L"None of the columns for the '%s' analysis were found "
                                   "in dataset '%s'."),
                                 GetName(), GetSourceDatasetName())
                    .utf8_string());
            }
        const auto groupColumn = m_groupColumn.empty() ? source.GetCategoricalColumns().cend() :
                                                         source.GetCategoricalColumn(m_groupColumn);
        const bool isGrouped = !m_groupColumn.empty();
        if (isGrouped && groupColumn == source.GetCategoricalColumns().cend())
            {
            throw std::runtime_error(
                wxString::Format(_(L"'%s': categorical column not found in dataset '%s'."),
                                 m_groupColumn, GetSourceDatasetName())
                    .utf8_string());
            }

        auto results = std::make_shared<Data::Dataset>();
        results->GetIdColumn().SetName(_(L"Variable"));
        if (isGrouped)
            {
            results->AddCategoricalColumn(groupColumn->GetName(), groupColumn->GetStringTable());
            }
        if (m_includeN)
            {
            results->AddContinuousColumn(_(L"N"));
            }
        if (m_includeMissing)
            {
            results->AddContinuousColumn(_(L"Missing"));
            }
        if (m_includeMean)
            {
            results->AddContinuousColumn(_(L"Mean"));
            }
        if (m_includeStdDev)
            {
            // TRANSLATORS: Abbreviation for "Standard Deviation."
            results->AddContinuousColumn(_(L"Std. Dev."));
            }
        if (m_includeMin)
            {
            results->AddContinuousColumn(_(L"Min"));
            }
        if (m_includeQuartiles)
            {
            // TRANSLATORS: Abbreviation for "First Quartile."
            results->AddContinuousColumn(_(L"Q1"));
            results->AddContinuousColumn(_(L"Median"));
            // TRANSLATORS: Abbreviation for "Third Quartile."
            results->AddContinuousColumn(_(L"Q3"));
            }
        if (m_includeMax)
            {
            results->AddContinuousColumn(_(L"Max"));
            }

        // the statistics for one set of values, in the order of the columns added above
        const auto calcStatistics = [this](const std::vector<double>& values)
        {
            constexpr auto NAN_VALUE = std::numeric_limits<double>::quiet_NaN();
            std::vector<double> validValues;
            validValues.reserve(values.size());
            std::ranges::copy_if(values, std::back_inserter(validValues),
                                 [](const double val) { return std::isfinite(val); });
            std::ranges::sort(validValues);
            const size_t validN{ validValues.size() };

            std::vector<double> stats;
            if (m_includeN)
                {
                stats.push_back(static_cast<double>(validN));
                }
            if (m_includeMissing)
                {
                stats.push_back(static_cast<double>(values.size() - validN));
                }
            if (m_includeMean)
                {
                stats.push_back((validN > 0) ? statistics::mean(validValues) : NAN_VALUE);
                }
            if (m_includeStdDev)
                {
                stats.push_back((validN > 1) ? statistics::standard_deviation(validValues, true) :
                                               NAN_VALUE);
                }
            if (m_includeMin)
                {
                stats.push_back((validN > 0) ? validValues.front() : NAN_VALUE);
                }
            if (m_includeQuartiles)
                {
                double lowerQuartile{ NAN_VALUE };
                double upperQuartile{ NAN_VALUE };
                double medianValue{ NAN_VALUE };
                if (validN > 0)
                    {
                    statistics::quartiles_presorted(validValues, lowerQuartile, upperQuartile);
                    medianValue = statistics::median_presorted(validValues);
                    }
                stats.push_back(lowerQuartile);
                stats.push_back(medianValue);
                stats.push_back(upperQuartile);
                }
            if (m_includeMax)
                {
                stats.push_back((validN > 0) ? validValues.back() : NAN_VALUE);
                }
            return stats;
        };

        for (const auto& colName : foundColumns)
            {
            const auto& column = *source.GetContinuousColumn(colName);
            if (!isGrouped)
                {
                // analyze full column
                results->AddRow(Data::RowInfo{}
                                    .Id(column.GetName())
                                    .Continuous(calcStatistics(column.GetValues())));
                continue;
                }
            // split the values by group in one pass, in the order of the group codes
            std::map<Data::GroupIdType, std::vector<double>> valuesByGroup;
            for (size_t i = 0; i < column.GetRowCount(); ++i)
                {
                valuesByGroup[groupColumn->GetValue(i)].push_back(column.GetValue(i));
                }
            for (const auto& [groupCode, groupValues] : valuesByGroup)
                {
                results->AddRow(Data::RowInfo{}
                                    .Id(column.GetName())
                                    .Categoricals({ groupCode })
                                    .Continuous(calcStatistics(groupValues)));
                }
            }

        return { AnalysisResultTable{ GetDisplayTypeName(), results } };
        }

    //-------------------------------------------
    void DescriptiveStatistics::SaveOptions(wxSimpleJSON::Ptr_t& node) const
        {
        wxArrayString columns;
        for (const auto& colName : m_columns)
            {
            columns.Add(colName);
            }
        node->Add(L"variables", columns);
        if (!m_groupColumn.empty())
            {
            node->Add(L"group", m_groupColumn);
            }

        auto statsNode = wxSimpleJSON::Create(wxSimpleJSON::JSONType::IS_OBJECT);
        statsNode->Add(L"n", m_includeN);
        statsNode->Add(L"missing", m_includeMissing);
        statsNode->Add(L"mean", m_includeMean);
        statsNode->Add(L"sd", m_includeStdDev);
        statsNode->Add(L"min", m_includeMin);
        statsNode->Add(L"quartiles", m_includeQuartiles);
        statsNode->Add(L"max", m_includeMax);
        node->Add(L"statistics", statsNode);
        }

    //-------------------------------------------
    void DescriptiveStatistics::LoadOptions(const wxSimpleJSON::Ptr_t& node)
        {
        m_columns = node->GetProperty(L"variables")->AsStrings();
        m_groupColumn = node->GetProperty(L"group")->AsString();

        const auto statsNode = node->GetProperty(L"statistics");
        if (statsNode->IsOk())
            {
            m_includeN = statsNode->GetProperty(L"n")->AsBool(true);
            m_includeMissing = statsNode->GetProperty(L"missing")->AsBool(true);
            m_includeMean = statsNode->GetProperty(L"mean")->AsBool(true);
            m_includeStdDev = statsNode->GetProperty(L"sd")->AsBool(true);
            m_includeMin = statsNode->GetProperty(L"min")->AsBool(true);
            m_includeQuartiles = statsNode->GetProperty(L"quartiles")->AsBool(true);
            m_includeMax = statsNode->GetProperty(L"max")->AsBool(true);
            }
        }
    } // namespace Wisteria::Analyses
