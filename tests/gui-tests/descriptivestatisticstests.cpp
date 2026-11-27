///////////////////////////////////////////////////////////////////////////////
// Name:        descriptivestatisticstests.cpp
// Purpose:     Tests for the descriptive statistics analysis
// Author:      Blake Madden
// Copyright:   (c) 2026 Blake Madden
// License:     3-Clause BSD license
///////////////////////////////////////////////////////////////////////////////

#include "../../src/analyses/descriptivestatistics.h"
#include "../../src/reporting/reportbuilder.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>
#include <wx/file.h>
#include <wx/filename.h>

using Catch::Matchers::WithinAbs;
using namespace Wisteria;
using namespace Wisteria::Data;
using namespace Wisteria::Analyses;

namespace
    {
    constexpr double NAN_VALUE{ std::numeric_limits<double>::quiet_NaN() };

    // x: 1-9 and one missing value
    // y: 10, 20, 30, 40, and six missing values
    // z: all missing
    // w: a single value
    // Group: A for the first five rows, B for the last five
    Dataset MakeDataset()
        {
        Dataset dataset;
        dataset.AddCategoricalColumn(
            L"Group", ColumnWithStringTable::StringTableType{ { 0, L"A" }, { 1, L"B" } });
        dataset.AddContinuousColumn(L"x");
        dataset.AddContinuousColumn(L"y");
        dataset.AddContinuousColumn(L"z");
        dataset.AddContinuousColumn(L"w");
        const std::vector<double> xValues{ 1, 2, 3, 4, 5, 6, 7, 8, 9, NAN_VALUE };
        const std::vector<double> yValues{ 10,        20,        30,        40,        NAN_VALUE,
                                           NAN_VALUE, NAN_VALUE, NAN_VALUE, NAN_VALUE, NAN_VALUE };
        for (size_t i = 0; i < xValues.size(); ++i)
            {
            dataset.AddRow(
                RowInfo{}
                    .Id(wxString::Format(L"obs%zu", i))
                    .Categoricals({ static_cast<GroupIdType>(i < 5 ? 0 : 1) })
                    .Continuous({ xValues[i], yValues[i], NAN_VALUE, (i == 0) ? 5.0 : NAN_VALUE }));
            }
        return dataset;
        }

    double GetStat(const Dataset& results, const wxString& statName, const size_t row)
        {
        const auto column = results.GetContinuousColumn(statName);
        REQUIRE(column != results.GetContinuousColumns().cend());
        return column->GetValue(row);
        }
    } // namespace

// Expected values are from R 4.x: mean(), sd() (sample, N - 1), and fivenum() (Tukey's hinges).
// The following R code prints the same table that the analysis produces:
//
//   desc_stats <- function(x) {
//     valid <- x[!is.na(x)]
//     fn <- fivenum(valid)
//     c(N = length(valid), Missing = sum(is.na(x)), Mean = mean(valid),
//       `Std. Dev.` = sd(valid), Min = fn[1], Q1 = fn[2], Median = fn[3],
//       Q3 = fn[4], Max = fn[5])
//   }
//
//   # the dataset from MakeDataset()
//   test <- data.frame(Group = rep(c("A", "B"), each = 5),
//                      x = c(1:9, NA),
//                      y = c(10, 20, 30, 40, rep(NA, 6)),
//                      z = rep(NA_real_, 10),
//                      w = c(5, rep(NA, 9)))
//   print(t(sapply(test[c("x", "y", "z", "w")], desc_stats)))
//   print(do.call(rbind, lapply(split(test$x, test$Group), desc_stats)))
//
// A real dataset example (mtcars.csv):
//
//   # full column
//   print(t(sapply(mtcars[c("mpg", "hp", "wt")], desc_stats)))
//   # with "cyl" as a categorical column
//   print(do.call(rbind, lapply(split(mtcars$mpg, mtcars$cyl), desc_stats)))
TEST_CASE("DescriptiveStatistics: ungrouped", "[analyses][descriptive-statistics]")
    {
    const auto dataset = MakeDataset();
    DescriptiveStatistics analysis;
    analysis.SetSourceDatasetName(L"test");
    analysis.SetColumns({ L"x", L"y", L"z", L"w" });

    std::vector<wxString> warnings;
    const auto results = analysis.Run(dataset, warnings);
    CHECK(warnings.empty());
    REQUIRE(results.size() == 1);
    REQUIRE(results.front().m_data != nullptr);
    const auto& table = *results.front().m_data;
    REQUIRE(table.GetRowCount() == 4);
    CHECK(table.GetCategoricalColumns().empty());
    CHECK(table.GetContinuousColumns().size() == 9);

    SECTION("Variable names are the row IDs")
        {
        CHECK(table.GetIdColumn().GetValue(0) == L"x");
        CHECK(table.GetIdColumn().GetValue(1) == L"y");
        CHECK(table.GetIdColumn().GetValue(2) == L"z");
        CHECK(table.GetIdColumn().GetValue(3) == L"w");
        }

    SECTION("Values with a missing value")
        {
        CHECK_THAT(GetStat(table, L"N", 0), WithinAbs(9, 1e-9));
        CHECK_THAT(GetStat(table, L"Missing", 0), WithinAbs(1, 1e-9));
        CHECK_THAT(GetStat(table, L"Mean", 0), WithinAbs(5, 1e-9));
        CHECK_THAT(GetStat(table, L"Std. Dev.", 0), WithinAbs(2.738613, 1e-6));
        CHECK_THAT(GetStat(table, L"Min", 0), WithinAbs(1, 1e-9));
        CHECK_THAT(GetStat(table, L"Q1", 0), WithinAbs(3, 1e-9));
        CHECK_THAT(GetStat(table, L"Median", 0), WithinAbs(5, 1e-9));
        CHECK_THAT(GetStat(table, L"Q3", 0), WithinAbs(7, 1e-9));
        CHECK_THAT(GetStat(table, L"Max", 0), WithinAbs(9, 1e-9));
        }

    SECTION("An even number of values")
        {
        CHECK_THAT(GetStat(table, L"N", 1), WithinAbs(4, 1e-9));
        CHECK_THAT(GetStat(table, L"Missing", 1), WithinAbs(6, 1e-9));
        CHECK_THAT(GetStat(table, L"Mean", 1), WithinAbs(25, 1e-9));
        CHECK_THAT(GetStat(table, L"Std. Dev.", 1), WithinAbs(12.909944, 1e-6));
        CHECK_THAT(GetStat(table, L"Q1", 1), WithinAbs(15, 1e-9));
        CHECK_THAT(GetStat(table, L"Median", 1), WithinAbs(25, 1e-9));
        CHECK_THAT(GetStat(table, L"Q3", 1), WithinAbs(35, 1e-9));
        }

    SECTION("All values missing")
        {
        CHECK_THAT(GetStat(table, L"N", 2), WithinAbs(0, 1e-9));
        CHECK_THAT(GetStat(table, L"Missing", 2), WithinAbs(10, 1e-9));
        CHECK(std::isnan(GetStat(table, L"Mean", 2)));
        CHECK(std::isnan(GetStat(table, L"Std. Dev.", 2)));
        CHECK(std::isnan(GetStat(table, L"Min", 2)));
        CHECK(std::isnan(GetStat(table, L"Q1", 2)));
        CHECK(std::isnan(GetStat(table, L"Median", 2)));
        CHECK(std::isnan(GetStat(table, L"Q3", 2)));
        CHECK(std::isnan(GetStat(table, L"Max", 2)));
        }

    SECTION("A single value has no standard deviation")
        {
        CHECK_THAT(GetStat(table, L"N", 3), WithinAbs(1, 1e-9));
        CHECK_THAT(GetStat(table, L"Mean", 3), WithinAbs(5, 1e-9));
        CHECK(std::isnan(GetStat(table, L"Std. Dev.", 3)));
        CHECK_THAT(GetStat(table, L"Q1", 3), WithinAbs(5, 1e-9));
        CHECK_THAT(GetStat(table, L"Median", 3), WithinAbs(5, 1e-9));
        CHECK_THAT(GetStat(table, L"Q3", 3), WithinAbs(5, 1e-9));
        }
    }

TEST_CASE("DescriptiveStatistics: grouped", "[analyses][descriptive-statistics]")
    {
    const auto dataset = MakeDataset();
    DescriptiveStatistics analysis;
    analysis.SetColumns({ L"x" });
    analysis.SetGroupColumn(L"Group");

    std::vector<wxString> warnings;
    const auto results = analysis.Run(dataset, warnings);
    REQUIRE(results.size() == 1);
    const auto& table = *results.front().m_data;
    REQUIRE(table.GetRowCount() == 2);
    const auto groupColumn = table.GetCategoricalColumn(L"Group");
    REQUIRE(groupColumn != table.GetCategoricalColumns().cend());

    CHECK(table.GetIdColumn().GetValue(0) == L"x");
    CHECK(groupColumn->GetValueAsLabel(0) == L"A");
    CHECK_THAT(GetStat(table, L"N", 0), WithinAbs(5, 1e-9));
    CHECK_THAT(GetStat(table, L"Missing", 0), WithinAbs(0, 1e-9));
    CHECK_THAT(GetStat(table, L"Mean", 0), WithinAbs(3, 1e-9));
    CHECK_THAT(GetStat(table, L"Std. Dev.", 0), WithinAbs(1.581139, 1e-6));
    CHECK_THAT(GetStat(table, L"Q1", 0), WithinAbs(2, 1e-9));
    CHECK_THAT(GetStat(table, L"Q3", 0), WithinAbs(4, 1e-9));

    CHECK(groupColumn->GetValueAsLabel(1) == L"B");
    CHECK_THAT(GetStat(table, L"N", 1), WithinAbs(4, 1e-9));
    CHECK_THAT(GetStat(table, L"Missing", 1), WithinAbs(1, 1e-9));
    CHECK_THAT(GetStat(table, L"Mean", 1), WithinAbs(7.5, 1e-9));
    CHECK_THAT(GetStat(table, L"Std. Dev.", 1), WithinAbs(1.290994, 1e-6));
    CHECK_THAT(GetStat(table, L"Min", 1), WithinAbs(6, 1e-9));
    CHECK_THAT(GetStat(table, L"Q1", 1), WithinAbs(6.5, 1e-9));
    CHECK_THAT(GetStat(table, L"Median", 1), WithinAbs(7.5, 1e-9));
    CHECK_THAT(GetStat(table, L"Q3", 1), WithinAbs(8.5, 1e-9));
    CHECK_THAT(GetStat(table, L"Max", 1), WithinAbs(9, 1e-9));
    }

TEST_CASE("DescriptiveStatistics: options", "[analyses][descriptive-statistics]")
    {
    const auto dataset = MakeDataset();
    std::vector<wxString> warnings;

    SECTION("Only the included statistics are columns")
        {
        DescriptiveStatistics analysis;
        analysis.SetColumns({ L"x" });
        analysis.m_includeN = false;
        analysis.m_includeMissing = false;
        analysis.m_includeStdDev = false;
        analysis.m_includeMin = false;
        analysis.m_includeQuartiles = false;
        const auto results = analysis.Run(dataset, warnings);
        const auto& table = *results.front().m_data;
        CHECK(table.GetContinuousColumns().size() == 2);
        CHECK(table.GetContinuousColumn(wxString{ L"N" }) == table.GetContinuousColumns().cend());
        CHECK(table.GetContinuousColumn(wxString{ L"Median" }) ==
              table.GetContinuousColumns().cend());
        CHECK_THAT(GetStat(table, L"Mean", 0), WithinAbs(5, 1e-9));
        CHECK_THAT(GetStat(table, L"Max", 0), WithinAbs(9, 1e-9));
        }

    SECTION("A missing column is skipped with a warning")
        {
        DescriptiveStatistics analysis;
        analysis.SetColumns({ L"x", L"nope" });
        const auto results = analysis.Run(dataset, warnings);
        CHECK(warnings.size() == 1);
        REQUIRE(results.size() == 1);
        const auto& table = *results.front().m_data;
        REQUIRE(table.GetRowCount() == 1);
        CHECK(table.GetIdColumn().GetValue(0) == L"x");
        // the column stays in the options
        CHECK(analysis.GetColumns() == std::vector<wxString>{ L"x", L"nope" });
        }

    SECTION("Calculate keeps the warnings")
        {
        DescriptiveStatistics analysis;
        analysis.SetColumns({ L"x", L"nope" });
        analysis.Calculate(dataset);
        CHECK(analysis.GetResults().size() == 1);
        CHECK(analysis.GetWarnings().size() == 1);
        analysis.SetColumns({ L"x" });
        analysis.Calculate(dataset);
        CHECK(analysis.GetWarnings().empty());
        }

    SECTION("No columns found throws")
        {
        DescriptiveStatistics analysis;
        analysis.SetColumns({ L"nope", L"nada" });
        CHECK_THROWS_AS(analysis.Run(dataset, warnings), std::runtime_error);
        }

    SECTION("A missing group column throws")
        {
        DescriptiveStatistics analysis;
        analysis.SetColumns({ L"x" });
        analysis.SetGroupColumn(L"nope");
        CHECK_THROWS_AS(analysis.Run(dataset, warnings), std::runtime_error);
        }

    SECTION("A failed calculation clears the previous results")
        {
        DescriptiveStatistics analysis;
        analysis.SetColumns({ L"x" });
        analysis.Calculate(dataset);
        CHECK(analysis.GetResults().size() == 1);
        analysis.SetColumns({ L"nope" });
        CHECK_THROWS_AS(analysis.Calculate(dataset), std::runtime_error);
        CHECK(analysis.GetResults().empty());
        }
    }

TEST_CASE("Analysis: factory and options round trip", "[analyses]")
    {
    SECTION("Factory")
        {
        const auto analysis = Analysis::Create(L"descriptive-statistics");
        REQUIRE(analysis != nullptr);
        CHECK(analysis->GetTypeName() == L"descriptive-statistics");
        CHECK(dynamic_cast<const DescriptiveStatistics*>(analysis.get()) != nullptr);
        CHECK(Analysis::Create(L"not-an-analysis") == nullptr);
        }

    SECTION("Options are saved and loaded")
        {
        DescriptiveStatistics analysis;
        analysis.SetColumns({ L"x", L"y" });
        analysis.SetGroupColumn(L"Group");
        analysis.m_includeMissing = false;
        analysis.m_includeQuartiles = false;

        auto node = wxSimpleJSON::Create(wxSimpleJSON::JSONType::IS_OBJECT, true);
        analysis.SaveOptions(node);

        DescriptiveStatistics loaded;
        loaded.LoadOptions(node);
        CHECK(loaded.GetColumns() == std::vector<wxString>{ L"x", L"y" });
        CHECK(loaded.GetGroupColumn() == L"Group");
        CHECK(loaded.m_includeN);
        CHECK_FALSE(loaded.m_includeMissing);
        CHECK(loaded.m_includeMean);
        CHECK(loaded.m_includeStdDev);
        CHECK(loaded.m_includeMin);
        CHECK_FALSE(loaded.m_includeQuartiles);
        CHECK(loaded.m_includeMax);
        }

    SECTION("A clone keeps the GUID and options")
        {
        DescriptiveStatistics analysis;
        analysis.SetName(L"Summary");
        analysis.SetColumns({ L"x" });
        const auto clone = analysis.Clone();
        CHECK(clone->GetGuid() == analysis.GetGuid());
        CHECK(clone->GetName() == L"Summary");
        const auto* clonedStats = dynamic_cast<const DescriptiveStatistics*>(clone.get());
        REQUIRE(clonedStats != nullptr);
        CHECK(clonedStats->GetColumns() == std::vector<wxString>{ L"x" });
        }
    }

TEST_CASE("ReportBuilder loads analyses", "[reportbuilder][analyses]")
    {
    wxLogNull noLog;
    const wxString tempDir{ wxFileName::GetTempDir() + wxFileName::GetPathSeparator() };
    const wxString csvPath{ tempDir + L"wisteria-analyses-test.csv" };
    const wxString configPath{ tempDir + L"wisteria-analyses-test.json" };

        {
        wxFile csvFile{ csvPath, wxFile::write };
        REQUIRE(csvFile.IsOpened());
        csvFile.Write(L"Region,Sales\nEast,1\nEast,2\nEast,3\nWest,4\nWest,5\n");
        }
        {
        wxFile configFile{ configPath, wxFile::write };
        REQUIRE(configFile.IsOpened());
        configFile.Write(LR"({
  "name": "Analyses test",
  "datasets": [
    {
      "name": "Sales",
      "path": "wisteria-analyses-test.csv",
      "categorical-columns": [ { "name": "Region" } ],
      "continuous-columns": [ "Sales" ]
    }
  ],
  "analyses": [
    {
      "type": "descriptive-statistics",
      "name": "Sales summary",
      "guid": "abc-123",
      "dataset": "Sales",
      "variables": [ "Sales" ],
      "statistics": { "missing": false }
    },
    {
      "type": "descriptive-statistics",
      "name": "Missing dataset",
      "dataset": "Nope",
      "variables": [ "Sales" ]
    },
    {
      "type": "not-an-analysis",
      "name": "Unknown"
    }
  ]
})");
        }

    ReportBuilder builder;
    [[maybe_unused]]
    const auto pages = builder.LoadConfigurationFile(configPath, wxTheApp->GetTopWindow());
    wxRemoveFile(csvPath);
    wxRemoveFile(configPath);

    const auto& analyses = builder.GetAnalyses();
    // the unknown type cannot be loaded
    REQUIRE(analyses.size() == 2);

    SECTION("A valid analysis is loaded and run")
        {
        const auto& analysis = *analyses[0];
        CHECK(analysis.GetName() == L"Sales summary");
        CHECK(analysis.GetGuid() == L"abc-123");
        CHECK(analysis.GetSourceDatasetName() == L"Sales");
        REQUIRE(analysis.GetResults().size() == 1);
        const auto& table = *analysis.GetResults().front().m_data;
        REQUIRE(table.GetRowCount() == 1);
        CHECK(table.GetContinuousColumn(wxString{ L"Missing" }) ==
              table.GetContinuousColumns().cend());
        CHECK_THAT(GetStat(table, L"N", 0), WithinAbs(5, 1e-9));
        CHECK_THAT(GetStat(table, L"Mean", 0), WithinAbs(3, 1e-9));
        CHECK(builder.FindAnalysis(L"abc-123") == std::optional<size_t>{ 0 });
        }

    SECTION("An analysis with a missing dataset is kept without results")
        {
        const auto& analysis = *analyses[1];
        CHECK(analysis.GetName() == L"Missing dataset");
        CHECK(analysis.GetResults().empty());
        CHECK_FALSE(analysis.GetGuid().empty());
        }

    SECTION("Errors are reported") { CHECK(builder.UnloadPendingErrorMessages().size() == 2); }

    SECTION("Removing an analysis")
        {
        builder.RemoveAnalysis(L"abc-123");
        CHECK(builder.GetAnalyses().size() == 1);
        CHECK_FALSE(builder.FindAnalysis(L"abc-123").has_value());
        }
    }
