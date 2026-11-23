///////////////////////////////////////////////////////////////////////////////
// Name:        reportbuilderdetachedgraphtests.cpp
// Purpose:     Tests for building a single graph from JSON without placing it
// Author:      Blake Madden
// Copyright:   (c) 2026 Blake Madden
// License:     3-Clause BSD license
///////////////////////////////////////////////////////////////////////////////

#include "../../src/base/label.h"
#include "../../src/graphs/filterablegraph.h"
#include "../../src/graphs/piechart.h"
#include "../../src/reporting/reportbuilder.h"
#include "graphrenderharness.h"
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <stdexcept>
#include <vector>
#include <wx/file.h>
#include <wx/filename.h>

using namespace Wisteria;
using namespace Wisteria::Data;
using namespace Wisteria::Graphs;
using namespace wisteria_render_tests;

TEST_CASE("ReportBuilder::LoadGraphDetached", "[reportbuilder][detached]")
    {
    auto dataset = std::make_shared<Dataset>();
    dataset->AddCategoricalColumn(
        L"group1", ColumnWithStringTable::StringTableType{ { 0, L"Alpha" }, { 1, L"Beta" } });
    const std::vector<GroupIdType> codes{ 0, 0, 1 };
    for (size_t idx = 0; idx < codes.size(); ++idx)
        {
        RowInfo row;
        row.Id(wxString::Format(L"obs%d", static_cast<int>(idx)));
        row.Categoricals({ codes[idx] });
        dataset->AddRow(row);
        }

    ReportBuilder builder;
    builder.GetDatasets()[L"survey"] = dataset;
    auto* canvas = MakeCanvas();

    const wxString pieJson{ L"{\"type\": \"pie-chart\", \"dataset\": \"survey\", "
                            L"\"variables\": { \"group-1\": \"group1\" } }" };

    SECTION("Builds the graph without placing it on the canvas")
        {
        const auto graph = builder.LoadGraphDetached(pieJson, canvas);
        REQUIRE(graph != nullptr);

        const auto* pie = dynamic_cast<const PieChart*>(graph.get());
        REQUIRE(pie != nullptr);
        CHECK(pie->GetGroupColumn1Name() == L"group1");
        CHECK(pie->GetPropertyTemplate(L"dataset") == L"survey");

        const auto [gridRows, gridCols] = canvas->GetFixedObjectsGridSize();
        for (size_t row = 0; row < gridRows; ++row)
            {
            for (size_t col = 0; col < gridCols; ++col)
                {
                CHECK(canvas->GetFixedObject(row, col) == nullptr);
                }
            }
        }

    SECTION("Legend settings are kept without placing a legend")
        {
        const wxString legendJson{ L"{\"type\": \"pie-chart\", \"dataset\": \"survey\", "
                                   L"\"variables\": { \"group-1\": \"group1\" }, "
                                   L"\"legend\": { \"placement\": \"left\", \"title\": \"Groups\", "
                                   L"\"include-header\": false, \"ring\": \"inner\" } }" };
        const auto graph = builder.LoadGraphDetached(legendJson, canvas);
        REQUIRE(graph != nullptr);

        const auto& legendInfo = graph->GetLegendInfo();
        REQUIRE(legendInfo.has_value());
        CHECK(legendInfo->GetPlacement() == Side::Left);
        CHECK(legendInfo->GetTitle() == L"Groups");
        CHECK_FALSE(legendInfo->IsIncludingHeader());
        CHECK(legendInfo->GetRingPerimeter() == Perimeter::Inner);

        const auto [gridRows, gridCols] = canvas->GetFixedObjectsGridSize();
        for (size_t row = 0; row < gridRows; ++row)
            {
            for (size_t col = 0; col < gridCols; ++col)
                {
                CHECK(canvas->GetFixedObject(row, col) == nullptr);
                }
            }
        }

    SECTION("A missing dataset throws, and a later load still works")
        {
        const wxString missingJson{ L"{\"type\": \"pie-chart\", \"dataset\": \"nope\", "
                                    L"\"variables\": { \"group-1\": \"group1\" } }" };
        CHECK_THROWS_AS(builder.LoadGraphDetached(missingJson, canvas), std::runtime_error);
        CHECK(builder.LoadGraphDetached(pieJson, canvas) != nullptr);
        }

    SECTION("Unsupported or malformed input returns null")
        {
        CHECK(builder.LoadGraphDetached(L"{\"type\": \"label\"}", canvas) == nullptr);
        CHECK(builder.LoadGraphDetached(L"{}", canvas) == nullptr);
        CHECK(builder.LoadGraphDetached(L"not json", canvas) == nullptr);
        CHECK(builder.LoadGraphDetached(pieJson, nullptr) == nullptr);
        }
    }

TEST_CASE("ReportBuilder loads standalone graphs", "[reportbuilder][graphs]")
    {
    wxLogNull noLog;
    const wxString tempDir{ wxFileName::GetTempDir() + wxFileName::GetPathSeparator() };
    const wxString csvPath{ tempDir + L"wisteria-standalone-graphs-test.csv" };
    const wxString configPath{ tempDir + L"wisteria-standalone-graphs-test.json" };

        {
        wxFile csvFile{ csvPath, wxFile::write };
        REQUIRE(csvFile.IsOpened());
        csvFile.Write(L"Region,Product\nEast,Apples\nEast,Bananas\nNorth,Bananas\n"
                      "North,Cherries\nWest,Apples\nWest,Cherries\n");
        }
        {
        wxFile configFile{ configPath, wxFile::write };
        REQUIRE(configFile.IsOpened());
        configFile.Write(LR"({
  "name": "Standalone graphs test",
  "datasets": [
    {
      "name": "Sales",
      "path": "wisteria-standalone-graphs-test.csv",
      "categorical-columns": [ { "name": "Region" }, { "name": "Product" } ]
    }
  ],
  "graphs": [
    {
      "name": "Products",
      "graph": {
        "type": "pie-chart",
        "dataset": "Sales",
        "variables": { "group-1": "Product" }
      }
    },
    {
      "name": "Products by region",
      "graph": {
        "type": "pie-chart",
        "dataset": "Sales",
        "variables": { "group-1": "Product" },
        "legend": { "placement": "left" },
        "filter": { "column": "Region", "default": "North" }
      }
    },
    {
      "name": "Unknown",
      "graph": { "type": "not-a-graph" }
    }
  ],
  "pages": [
    {
      "name": "Page",
      "rows": [
        {
          "items": [
            {
              "type": "pie-chart",
              "dataset": "Sales",
              "variables": { "group-1": "Region" }
            }
          ]
        }
      ]
    }
  ]
})");
        }

    ReportBuilder builder;
    const auto pages = builder.LoadConfigurationFile(configPath, wxTheApp->GetTopWindow());
    const auto graphs = builder.ReleaseGraphCanvases();
    wxRemoveFile(csvPath);
    wxRemoveFile(configPath);

    // destroys the loaded canvases, even if a REQUIRE below fails
    struct CanvasCleanup
        {
        std::vector<Canvas*> m_canvases;

        CanvasCleanup() = default;
        CanvasCleanup(const CanvasCleanup&) = delete;
        CanvasCleanup& operator=(const CanvasCleanup&) = delete;

        ~CanvasCleanup()
            {
            for (auto* canvas : m_canvases)
                {
                if (canvas != nullptr)
                    {
                    canvas->Destroy();
                    }
                }
            }
        };

    CanvasCleanup cleanup;
    cleanup.m_canvases.insert(cleanup.m_canvases.end(), pages.cbegin(), pages.cend());
    cleanup.m_canvases.insert(cleanup.m_canvases.end(), graphs.cbegin(), graphs.cend());

    // graphs are kept separate from the pages
    REQUIRE(pages.size() == 1);
    CHECK(pages.front()->GetLabel() == L"Page");

    // a graph that can't be built isn't loaded
    REQUIRE(graphs.size() == 2);
    CHECK(builder.ReleaseGraphCanvases().empty());

    for (const auto* graphCanvas : graphs)
        {
        CHECK_FALSE(graphCanvas->IsMaintainingAspectRatio());
        }

    CHECK(graphs[0]->GetLabel() == L"Products");
    CHECK(graphs[0]->GetFixedObjectsGridSize() == std::pair<size_t, size_t>{ 1, 1 });
    CHECK(dynamic_cast<const PieChart*>(graphs[0]->GetFixedObject(0, 0).get()) != nullptr);

    // the legend is placed to the left of the filtered graph
    CHECK(graphs[1]->GetLabel() == L"Products by region");
    CHECK(graphs[1]->GetFixedObjectsGridSize() == std::pair<size_t, size_t>{ 1, 2 });
    const auto* legend =
        dynamic_cast<const GraphItems::Label*>(graphs[1]->GetFixedObject(0, 0).get());
    REQUIRE(legend != nullptr);
    CHECK(legend->IsLegend());
    const auto* filterableGraph =
        dynamic_cast<const FilterableGraph*>(graphs[1]->GetFixedObject(0, 1).get());
    REQUIRE(filterableGraph != nullptr);
    CHECK(filterableGraph->GetFilterColumn() == L"Region");
    CHECK(filterableGraph->GetVariants()[filterableGraph->GetActiveIndex()].m_label == L"North");
    }
