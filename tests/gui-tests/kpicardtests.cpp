///////////////////////////////////////////////////////////////////////////////
// Name:        kpicardtests.cpp
// Purpose:     Tests for KpiCard (aggregation, formatting, layout, and loading)
// Author:      Blake Madden
// Copyright:   (c) 2026 Blake Madden
// License:     3-Clause BSD license
///////////////////////////////////////////////////////////////////////////////

#include "../../src/graphs/filterablegraph.h"
#include "../../src/graphs/kpicard.h"
#include "../../src/reporting/reportbuilder.h"
#include "graphrenderharness.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <memory>
#include <stdexcept>
#include <wx/numformatter.h>

using namespace Wisteria;
using namespace Wisteria::Data;
using namespace Wisteria::Graphs;
using namespace Wisteria::GraphItems;
using namespace wisteria_render_tests;
using Catch::Matchers::WithinAbs;

// 10, 20, 30, 40, and a missing value
[[nodiscard]]
static std::shared_ptr<Dataset> MakeScoreDataset()
    {
    return MakeContinuousDataset(
        L"Score", { 10.0, 20.0, 30.0, 40.0, std::numeric_limits<double>::quiet_NaN() });
    }

// the formatter's output for a plain number, so that the tests don't depend on the locale
[[nodiscard]]
static wxString FormatNumber(const double value, const int precision)
    {
    return wxNumberFormatter::ToString(value, precision,
                                       wxNumberFormatter::Style::Style_WithThousandsSep);
    }

TEST_CASE("KpiCard aggregates", "[kpicard]")
    {
    auto* canvas = MakeCanvas();
    KpiCard card{ canvas };
    const auto dataset = MakeScoreDataset();

    SECTION("Sum")
        {
        card.SetData(dataset, L"Score", ColumnAggregation::Sum);
        REQUIRE(card.GetValue().has_value());
        CHECK_THAT(card.GetValue().value(), WithinAbs(100.0, 1e-9));
        }

    SECTION("Mean")
        {
        card.SetData(dataset, L"Score", ColumnAggregation::Mean);
        REQUIRE(card.GetValue().has_value());
        CHECK_THAT(card.GetValue().value(), WithinAbs(25.0, 1e-9));
        }

    SECTION("Median")
        {
        card.SetData(dataset, L"Score", ColumnAggregation::Median);
        REQUIRE(card.GetValue().has_value());
        CHECK_THAT(card.GetValue().value(), WithinAbs(25.0, 1e-9));
        }

    SECTION("Min and max")
        {
        card.SetData(dataset, L"Score", ColumnAggregation::Min);
        REQUIRE(card.GetValue().has_value());
        CHECK_THAT(card.GetValue().value(), WithinAbs(10.0, 1e-9));

        card.SetData(dataset, L"Score", ColumnAggregation::Max);
        REQUIRE(card.GetValue().has_value());
        CHECK_THAT(card.GetValue().value(), WithinAbs(40.0, 1e-9));
        }

    SECTION("Count skips missing values")
        {
        card.SetData(dataset, L"Score", ColumnAggregation::Count);
        REQUIRE(card.GetValue().has_value());
        CHECK_THAT(card.GetValue().value(), WithinAbs(4.0, 1e-9));
        }

    SECTION("Settings are kept")
        {
        card.SetData(dataset, L"Score", ColumnAggregation::Mean);
        CHECK(card.GetColumnName() == L"Score");
        CHECK(card.GetAggregate() == ColumnAggregation::Mean);
        CHECK(card.GetDataset() == dataset);
        }
    }

TEST_CASE("KpiCard missing data", "[kpicard]")
    {
    auto* canvas = MakeCanvas();
    KpiCard card{ canvas };
    const auto allMissing =
        MakeContinuousDataset(L"Score", { std::numeric_limits<double>::quiet_NaN(),
                                          std::numeric_limits<double>::quiet_NaN() });

    SECTION("No valid values shows the missing-value text")
        {
        card.SetData(allMissing, L"Score", ColumnAggregation::Mean);
        CHECK_FALSE(card.GetValue().has_value());
        CHECK(card.GetFormattedValue() == L"—");

        card.SetMissingValueText(L"N/A");
        CHECK(card.GetFormattedValue() == L"N/A");
        }

    SECTION("Count of no valid values is zero")
        {
        card.SetData(allMissing, L"Score", ColumnAggregation::Count);
        REQUIRE(card.GetValue().has_value());
        CHECK_THAT(card.GetValue().value(), WithinAbs(0.0, 1e-9));
        }

    SECTION("An empty dataset has no value")
        {
        card.SetData(MakeContinuousDataset(L"Score", {}), L"Score", ColumnAggregation::Sum);
        CHECK_FALSE(card.GetValue().has_value());
        }

    SECTION("A missing column or dataset throws")
        {
        CHECK_THROWS_AS(card.SetData(allMissing, L"Nope", ColumnAggregation::Sum),
                        std::runtime_error);
        CHECK_THROWS_AS(card.SetData(nullptr, L"Score", ColumnAggregation::Sum),
                        std::runtime_error);
        }
    }

TEST_CASE("KpiCard number formatting", "[kpicard]")
    {
    auto* canvas = MakeCanvas();
    KpiCard card{ canvas };
    const auto setValue = [&card](const double value)
    { card.SetData(MakeContinuousDataset(L"Value", { value }), L"Value", ColumnAggregation::Sum); };

    SECTION("Number with precision")
        {
        setValue(12345.678);
        CHECK(card.GetFormattedValue() == FormatNumber(12345.678, 0));
        card.SetPrecision(1);
        CHECK(card.GetFormattedValue() == FormatNumber(12345.678, 1));
        }

    SECTION("Values that round to zero have no sign")
        {
        setValue(-0.001);
        CHECK(card.GetFormattedValue() == FormatNumber(0, 0));
        card.SetNumberDisplay(NumberDisplay::Accounting);
        CHECK(card.GetFormattedValue().find(L'(') == wxString::npos);

        setValue(0.001);
        card.SetNumberDisplay(NumberDisplay::Value);
        card.ShowPlusSign(true);
        CHECK(card.GetFormattedValue() == FormatNumber(0, 0));
        // with enough decimal places, the sign is shown
        card.SetPrecision(3);
        CHECK(card.GetFormattedValue() == L"+" + FormatNumber(0.001, 3));
        }

    SECTION("Precision is limited")
        {
        card.SetPrecision(static_cast<uint8_t>(KpiCard::MAX_PRECISION + 5));
        CHECK(card.GetPrecision() == KpiCard::MAX_PRECISION);
        }

    SECTION("Number without thousands separators")
        {
        setValue(12345);
        card.SetNumberDisplay(NumberDisplay::ValueSimple);
        CHECK(card.GetFormattedValue() ==
              wxNumberFormatter::ToString(12345.0, 0, wxNumberFormatter::Style::Style_None));
        }

    SECTION("Percent is multiplied by 100")
        {
        setValue(0.256);
        card.SetNumberDisplay(NumberDisplay::Percentage);
        card.SetPrecision(1);
        CHECK(card.GetFormattedValue() == FormatNumber(25.6, 1) + L"%");
        }

    SECTION("Abbreviations")
        {
        setValue(45200);
        card.SetPrecision(1);
        card.SetAbbreviation(KpiNumberAbbreviation::Thousands);
        CHECK(card.GetFormattedValue() == FormatNumber(45.2, 1) + L"K");

        card.SetAbbreviation(KpiNumberAbbreviation::Millions);
        CHECK(card.GetFormattedValue() == FormatNumber(0.0, 1) + L"M");

        card.SetAbbreviation(KpiNumberAbbreviation::Auto);
        CHECK(card.GetFormattedValue() == FormatNumber(45.2, 1) + L"K");

        setValue(2'500'000);
        CHECK(card.GetFormattedValue() == FormatNumber(2.5, 1) + L"M");

        setValue(3'100'000'000);
        CHECK(card.GetFormattedValue() == FormatNumber(3.1, 1) + L"B");

        // too small for a unit
        setValue(950);
        CHECK(card.GetFormattedValue() == FormatNumber(950, 1));

        // values that round up to the next unit use it
        setValue(999'999);
        CHECK(card.GetFormattedValue() == FormatNumber(1.0, 1) + L"M");
        setValue(999.96);
        CHECK(card.GetFormattedValue() == FormatNumber(1.0, 1) + L"K");
        setValue(999'960'000);
        CHECK(card.GetFormattedValue() == FormatNumber(1.0, 1) + L"B");
        // but not values that round down
        setValue(999'940);
        CHECK(card.GetFormattedValue() == FormatNumber(999.9, 1) + L"K");
        }

    SECTION("Signs")
        {
        setValue(-5);
        CHECK(card.GetFormattedValue() == L"-" + FormatNumber(5, 0));

        setValue(5);
        CHECK(card.GetFormattedValue() == FormatNumber(5, 0));
        card.ShowPlusSign(true);
        CHECK(card.GetFormattedValue() == L"+" + FormatNumber(5, 0));

        // zero is neither positive nor negative
        setValue(0);
        CHECK(card.GetFormattedValue() == FormatNumber(0, 0));
        }

    SECTION("Accounting shows negatives in parentheses")
        {
        const wxString currency{ wxNumberFormatter::ToString(
            5000.0, 0,
            wxNumberFormatter::Style::Style_WithThousandsSep |
                wxNumberFormatter::Style::Style_Currency |
                wxNumberFormatter::Style::Style_CurrencySymbol) };

        setValue(-5000);
        card.SetNumberDisplay(NumberDisplay::Accounting);
        CHECK(card.GetFormattedValue() == L"(" + currency + L")");

        card.SetNumberDisplay(NumberDisplay::Currency);
        CHECK(card.GetFormattedValue() == L"-" + currency);

        setValue(5000);
        card.SetNumberDisplay(NumberDisplay::Accounting);
        CHECK(card.GetFormattedValue() == currency);
        }

    SECTION("Suffix")
        {
        setValue(12);
        card.SetSuffix(L" pts");
        CHECK(card.GetFormattedValue() == FormatNumber(12, 0) + L" pts");
        }
    }

TEST_CASE("KpiCard layout", "[kpicard]")
    {
    auto* canvas = MakeCanvas();
    auto card = std::make_shared<KpiCard>(canvas);
    card->SetData(MakeScoreDataset(), L"Score", ColumnAggregation::Sum);

    SECTION("The number is prepended to the body as the header")
        {
        card->GetLabel().SetText(L"Total score");
        LayOutOffscreen(canvas, card);

        REQUIRE(card->GetObjectCount() == 1);
        const auto* label = dynamic_cast<const Label*>(card->GetObjects().front().get());
        REQUIRE(label != nullptr);
        CHECK(label->GetText() == card->GetFormattedValue() + L"\nTotal score");
        CHECK(label->GetHeaderInfo().IsEnabled());
        // the template's text is still just the body
        CHECK(card->GetLabel().GetText() == L"Total score");
        }

    SECTION("Without body text, only the number is shown")
        {
        LayOutOffscreen(canvas, card);

        REQUIRE(card->GetObjectCount() == 1);
        const auto* label = dynamic_cast<const Label*>(card->GetObjects().front().get());
        REQUIRE(label != nullptr);
        CHECK(label->GetText() == card->GetFormattedValue());
        }

    SECTION("An empty box is measured at the label's natural size")
        {
        wxBitmap bmp{ CANVAS_WIDTH, CANVAS_HEIGHT, 32 };
        wxMemoryDC memDc{ bmp };
        wxGCDC gcdc{ memDc };

        auto& item = static_cast<GraphItemBase&>(*card);
        item.SetBoundingBox(wxRect{}, gcdc, 1.0);
        const wxRect naturalBox{ item.GetBoundingBox(gcdc) };
        CHECK_FALSE(naturalBox.IsEmpty());

        // a larger box doesn't change the natural size
        item.SetBoundingBox(wxRect{ wxSize{ CANVAS_WIDTH, CANVAS_HEIGHT } }, gcdc, 1.0);
        item.SetBoundingBox(wxRect{}, gcdc, 1.0);
        CHECK(item.GetBoundingBox(gcdc).GetSize() == naturalBox.GetSize());
        }

    SECTION("Scale locking follows the label")
        {
        const auto& item = static_cast<const GraphItemBase&>(*card);
        CHECK_FALSE(item.IsBoundingBoxScalingLocked());
        card->GetLabel().LockBoundingBoxScaling();
        CHECK(item.IsBoundingBoxScalingLocked());
        }
    }

TEST_CASE("KpiCard in a FilterableGraph", "[kpicard][filterablegraph]")
    {
    auto* canvas = MakeCanvas();
    // East: 10, 20; West: 30, 40, 50
    const auto dataset =
        MakeGroupedContinuousDataset(L"Sales", { 10.0, 20.0, 30.0, 40.0, 50.0 }, L"Region",
                                     { 0, 0, 1, 1, 1 }, { { 0, L"East" }, { 1, L"West" } });

    FilterableGraph filterableGraph{ dataset, L"Region", L"West",
                                     [canvas](const std::shared_ptr<const Dataset>& subset)
                                     {
                                         auto card = std::make_shared<KpiCard>(canvas);
                                         card->SetData(subset, L"Sales", ColumnAggregation::Sum);
                                         return card;
                                     } };

    const auto valueOf = [&filterableGraph](const size_t index)
    {
        const auto* card =
            dynamic_cast<const KpiCard*>(filterableGraph.GetVariants().at(index).m_graph.get());
        REQUIRE(card != nullptr);
        REQUIRE(card->GetValue().has_value());
        return card->GetValue().value();
    };

    REQUIRE(filterableGraph.GetVariants().size() == 3);
    CHECK_THAT(valueOf(0), WithinAbs(150.0, 1e-9)); // (All)
    CHECK_THAT(valueOf(1), WithinAbs(30.0, 1e-9));  // East
    CHECK_THAT(valueOf(2), WithinAbs(120.0, 1e-9)); // West
    CHECK(filterableGraph.GetActiveIndex() == 2);

    SECTION("Scale locking follows the active card")
        {
        const auto& item = static_cast<const GraphItemBase&>(filterableGraph);
        CHECK_FALSE(item.IsBoundingBoxScalingLocked());
        auto* activeCard = dynamic_cast<KpiCard*>(filterableGraph.GetActiveGraph().get());
        REQUIRE(activeCard != nullptr);
        activeCard->GetLabel().LockBoundingBoxScaling();
        CHECK(item.IsBoundingBoxScalingLocked());
        }

    SECTION("A KPI card has no legend")
        {
        CHECK(filterableGraph.CreateLegend(LegendOptions{}) == nullptr);
        }
    }

TEST_CASE("KpiCard loaded from JSON", "[kpicard][reportbuilder]")
    {
    ReportBuilder builder;
    builder.GetDatasets()[L"scores"] = MakeScoreDataset();
    auto* canvas = MakeCanvas();

    SECTION("All properties are loaded")
        {
        const wxString json{ L"{\"type\": \"kpi-card\", \"dataset\": \"scores\", "
                             L"\"variables\": { \"value\": \"Score\" }, "
                             L"\"aggregate\": \"median\", "
                             L"\"number-format\": { \"style\": \"percentage\", "
                             L"\"precision\": 2, \"abbreviation\": \"auto\", "
                             L"\"show-plus-sign\": true, \"suffix\": \" pts\", "
                             L"\"missing-value-text\": \"N/A\" }, "
                             L"\"label\": { \"text\": \"Median score\", "
                             L"\"header\": { \"relative-scaling\": 4 } } }" };
        const auto graph = builder.LoadGraphDetached(json, canvas);
        const auto* card = dynamic_cast<const KpiCard*>(graph.get());
        REQUIRE(card != nullptr);

        CHECK(card->GetColumnName() == L"Score");
        CHECK(card->GetAggregate() == ColumnAggregation::Median);
        REQUIRE(card->GetValue().has_value());
        CHECK_THAT(card->GetValue().value(), WithinAbs(25.0, 1e-9));

        CHECK(card->GetNumberDisplay() == NumberDisplay::Percentage);
        CHECK(card->GetPrecision() == 2);
        CHECK(card->GetAbbreviation() == KpiNumberAbbreviation::Auto);
        CHECK(card->IsShowingPlusSign());
        CHECK(card->GetSuffix() == L" pts");
        CHECK(card->GetMissingValueText() == L"N/A");

        CHECK(card->GetLabel().GetText() == L"Median score");
        CHECK_THAT(card->GetLabel().GetHeaderInfo().GetRelativeScaling(), WithinAbs(4.0, 1e-9));

        CHECK(card->GetPropertyTemplate(L"dataset") == L"scores");
        CHECK(card->GetPropertyTemplate(L"variables.value") == L"Score");
        }

    SECTION("The aggregate defaults to sum")
        {
        const wxString json{ L"{\"type\": \"kpi-card\", \"dataset\": \"scores\", "
                             L"\"variables\": { \"value\": \"Score\" } }" };
        const auto graph = builder.LoadGraphDetached(json, canvas);
        const auto* card = dynamic_cast<const KpiCard*>(graph.get());
        REQUIRE(card != nullptr);
        CHECK(card->GetAggregate() == ColumnAggregation::Sum);
        CHECK(card->GetNumberDisplay() == NumberDisplay::Value);
        REQUIRE(card->GetValue().has_value());
        CHECK_THAT(card->GetValue().value(), WithinAbs(100.0, 1e-9));
        }

    SECTION("Missing variables or datasets throw")
        {
        CHECK_THROWS_AS(
            builder.LoadGraphDetached(L"{\"type\": \"kpi-card\", \"dataset\": \"scores\"}", canvas),
            std::runtime_error);
        CHECK_THROWS_AS(
            builder.LoadGraphDetached(L"{\"type\": \"kpi-card\", \"dataset\": \"nope\", "
                                      L"\"variables\": { \"value\": \"Score\" } }",
                                      canvas),
            std::runtime_error);
        CHECK_THROWS_AS(
            builder.LoadGraphDetached(L"{\"type\": \"kpi-card\", \"dataset\": \"scores\", "
                                      L"\"variables\": { \"value\": \"Nope\" } }",
                                      canvas),
            std::runtime_error);
        }

    SECTION("A filter builds a card for each value")
        {
        builder.GetDatasets()[L"sales"] =
            MakeGroupedContinuousDataset(L"Sales", { 10.0, 20.0, 30.0 }, L"Region", { 0, 0, 1 },
                                         { { 0, L"East" }, { 1, L"West" } });
        const wxString json{ L"{\"type\": \"kpi-card\", \"dataset\": \"sales\", "
                             L"\"variables\": { \"value\": \"Sales\" }, "
                             L"\"filter\": { \"column\": \"Region\", \"default\": \"East\" } }" };
        const auto filterableGraph = builder.LoadFilterableGraphDetached(json, canvas);
        REQUIRE(filterableGraph != nullptr);
        REQUIRE(filterableGraph->GetVariants().size() == 3);

        const auto* activeCard =
            dynamic_cast<const KpiCard*>(filterableGraph->GetActiveGraph().get());
        REQUIRE(activeCard != nullptr);
        REQUIRE(activeCard->GetValue().has_value());
        CHECK_THAT(activeCard->GetValue().value(), WithinAbs(30.0, 1e-9));
        }
    }
