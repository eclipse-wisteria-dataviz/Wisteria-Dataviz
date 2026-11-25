///////////////////////////////////////////////////////////////////////////////
// Name:        matrixheatmaptests.cpp
// Purpose:     Tests for MatrixHeatMap tallying, sorting, and layout output
// Author:      Blake Madden
// Copyright:   (c) 2026 Blake Madden
// License:     3-Clause BSD license
///////////////////////////////////////////////////////////////////////////////

#include "../../src/graphs/matrixheatmap.h"
#include "graphrenderharness.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace Wisteria;
using namespace Wisteria::Data;
using namespace Wisteria::Graphs;
using namespace wisteria_render_tests;

namespace
    {
    // string tables shared by the test datasets (an empty label is missing data)
    const ColumnWithStringTable::StringTableType HIGH_SCHOOLS{
        { 0, L"West" }, { 1, L"central" }, { 2, L"St. Agnes" }, { 3, L"Central" }, { 9, L"" }
    };
    const ColumnWithStringTable::StringTableType COLLEGES{
        { 0, L"State U" }, { 1, L"Ivy College" }, { 2, L"Tech" }, { 9, L"" }
    };
    const ColumnWithStringTable::StringTableType SCHOOL_TYPES{ { 0, L"Public" },
                                                               { 1, L"Private" },
                                                               { 9, L"" } };

    struct Student
        {
        GroupIdType m_highSchool{ 0 };
        GroupIdType m_highSchoolType{ 0 };
        GroupIdType m_college{ 0 };
        GroupIdType m_collegeType{ 0 };
        double m_year{ 0 };
        };

    // columns: hs, hs_type, college, college_type (categorical) and year (continuous)
    [[nodiscard]]
    std::shared_ptr<Dataset> MakeStudentDataset(const std::vector<Student>& students)
        {
        auto dataset = std::make_shared<Dataset>();
        dataset->AddContinuousColumn(L"year");
        dataset->AddCategoricalColumn(L"hs", HIGH_SCHOOLS);
        dataset->AddCategoricalColumn(L"hs_type", SCHOOL_TYPES);
        dataset->AddCategoricalColumn(L"college", COLLEGES);
        dataset->AddCategoricalColumn(L"college_type", SCHOOL_TYPES);
        for (const auto& student : students)
            {
            dataset->AddRow(RowInfo()
                                .Continuous({ student.m_year })
                                .Categoricals({ student.m_highSchool, student.m_highSchoolType,
                                                student.m_college, student.m_collegeType }));
            }
        return dataset;
        }

    // high school codes: 0 West, 1 central, 2 St. Agnes, 3 Central
    // college codes: 0 State U, 1 Ivy College, 2 Tech
    // type codes: 0 Public, 1 Private
    [[nodiscard]]
    std::vector<Student> SampleStudents()
        {
        return {
            { 3, 0, 0, 0, 2020 }, // Central (Public) -> State U (Public)
            { 3, 0, 0, 0, 2021 }, // Central (Public) -> State U (Public)
            { 3, 0, 1, 1, 2020 }, // Central (Public) -> Ivy College (Private)
            { 2, 1, 1, 1, 2021 }, // St. Agnes (Private) -> Ivy College (Private)
            { 2, 1, 1, 1, 2022 }, // St. Agnes (Private) -> Ivy College (Private)
            { 2, 1, 1, 1, 2022 }, // St. Agnes (Private) -> Ivy College (Private)
            { 0, 0, 2, 0, 2020 }, // West (Public) -> Tech (Public)
            { 0, 0, 0, 0, 2021 }  // West (Public) -> State U (Public)
        };
        }

    [[nodiscard]]
    std::shared_ptr<MatrixHeatMap>
    BuildChart(Canvas* canvas, const std::shared_ptr<Dataset>& dataset, const wxString& rowColumn,
               const wxString& columnColumn,
               const std::optional<wxString>& rowGroupColumn = std::nullopt,
               const std::optional<wxString>& columnGroupColumn = std::nullopt)
        {
        auto chart = std::make_shared<MatrixHeatMap>(canvas);
        chart->SetData(dataset, rowColumn, columnColumn, rowGroupColumn, columnGroupColumn);
        return chart;
        }

    // the objects added beyond the matrix's own (from the base graph's layout),
    // measured from a one-cell matrix: a cell, its count, a row label, and a column label
    [[nodiscard]]
    size_t BaseObjectCount()
        {
        auto* canvas = MakeCanvas();
        auto chart =
            BuildChart(canvas, MakeStudentDataset({ { 0, 0, 0, 0, 2020 } }), L"hs", L"college");
        LayOutOffscreen(canvas, chart);
        return chart->GetObjectCount() - 4;
        }
    } // namespace

TEST_CASE("MatrixHeatMap tallies combinations", "[matrixheatmap]")
    {
    auto* canvas = MakeCanvas();
    auto chart = BuildChart(canvas, MakeStudentDataset(SampleStudents()), L"hs", L"college");

    // rows sorted alphabetically
    REQUIRE(chart->GetRowCategoryCount() == 3);
    CHECK(chart->GetRowLabel(0) == L"Central");
    CHECK(chart->GetRowLabel(1) == L"St. Agnes");
    CHECK(chart->GetRowLabel(2) == L"West");

    REQUIRE(chart->GetColumnCategoryCount() == 3);
    CHECK(chart->GetColumnLabel(0) == L"Ivy College");
    CHECK(chart->GetColumnLabel(1) == L"State U");
    CHECK(chart->GetColumnLabel(2) == L"Tech");

    // Central
    CHECK(chart->GetCount(0, 0) == 1);
    CHECK(chart->GetCount(0, 1) == 2);
    CHECK(chart->GetCount(0, 2) == 0);
    // St. Agnes
    CHECK(chart->GetCount(1, 0) == 3);
    CHECK(chart->GetCount(1, 1) == 0);
    CHECK(chart->GetCount(1, 2) == 0);
    // West
    CHECK(chart->GetCount(2, 0) == 0);
    CHECK(chart->GetCount(2, 1) == 1);
    CHECK(chart->GetCount(2, 2) == 1);

    // empty cells are not counted toward the lowest count
    CHECK(chart->GetMinCount() == 1);
    CHECK(chart->GetMaxCount() == 3);
    CHECK(chart->GetCount(99, 0) == 0);
    CHECK(chart->GetRowLabel(99).empty());
    }

TEST_CASE("MatrixHeatMap sorts ignoring case, with ties broken by case", "[matrixheatmap]")
    {
    auto* canvas = MakeCanvas();
    auto chart = BuildChart(canvas,
                            MakeStudentDataset({ { 0, 0, 0, 0, 2020 },    // West
                                                 { 1, 0, 0, 0, 2020 },    // central
                                                 { 2, 1, 0, 0, 2020 },    // St. Agnes
                                                 { 3, 0, 0, 0, 2020 },    // Central
                                                 { 1, 0, 0, 0, 2021 } }), // central
                            L"hs", L"college");

    // "central" and "Central" sort together, ahead of "St. Agnes",
    // with the uppercase spelling first
    REQUIRE(chart->GetRowCategoryCount() == 4);
    CHECK(chart->GetRowLabel(0) == L"Central");
    CHECK(chart->GetRowLabel(1) == L"central");
    CHECK(chart->GetRowLabel(2) == L"St. Agnes");
    CHECK(chart->GetRowLabel(3) == L"West");

    // the two spellings are tallied separately
    CHECK(chart->GetCount(0, 0) == 1);
    CHECK(chart->GetCount(1, 0) == 2);
    }

TEST_CASE("MatrixHeatMap keeps same-named values in different groups apart", "[matrixheatmap]")
    {
    auto students = SampleStudents();
    // a private school also named "Central"
    students.push_back({ 3, 1, 1, 1, 2022 });
    students.push_back({ 3, 1, 1, 1, 2023 });

    auto* canvas = MakeCanvas();
    auto chart = BuildChart(canvas, MakeStudentDataset(students), L"hs", L"college", L"hs_type",
                            L"college_type");

    // groups sorted alphabetically, then values within each group
    REQUIRE(chart->GetRowCategoryCount() == 4);
    CHECK(chart->GetRowGroupLabel(0) == L"Private");
    CHECK(chart->GetRowLabel(0) == L"Central");
    CHECK(chart->GetRowGroupLabel(1) == L"Private");
    CHECK(chart->GetRowLabel(1) == L"St. Agnes");
    CHECK(chart->GetRowGroupLabel(2) == L"Public");
    CHECK(chart->GetRowLabel(2) == L"Central");
    CHECK(chart->GetRowGroupLabel(3) == L"Public");
    CHECK(chart->GetRowLabel(3) == L"West");

    REQUIRE(chart->GetColumnCategoryCount() == 3);
    CHECK(chart->GetColumnGroupLabel(0) == L"Private");
    CHECK(chart->GetColumnLabel(0) == L"Ivy College");
    CHECK(chart->GetColumnGroupLabel(1) == L"Public");
    CHECK(chart->GetColumnLabel(1) == L"State U");
    CHECK(chart->GetColumnGroupLabel(2) == L"Public");
    CHECK(chart->GetColumnLabel(2) == L"Tech");

    // private Central and public Central are tallied separately
    CHECK(chart->GetCount(0, 0) == 2);
    CHECK(chart->GetCount(2, 0) == 1);
    CHECK(chart->GetCount(2, 1) == 2);
    CHECK(chart->GetCount(1, 0) == 3);
    }

TEST_CASE("MatrixHeatMap sorts continuous values numerically", "[matrixheatmap]")
    {
    auto* canvas = MakeCanvas();
    auto chart = BuildChart(
        canvas,
        MakeStudentDataset(
            { { 0, 0, 0, 0, 2010 }, { 0, 0, 0, 0, 9 }, { 0, 0, 0, 0, 100 }, { 0, 0, 0, 0, 100 } }),
        L"year", L"college");

    // sorted as text, these would be 100, 2010, 9
    REQUIRE(chart->GetRowCategoryCount() == 3);
    CHECK(chart->GetRowLabel(0) == L"9");
    CHECK(chart->GetRowLabel(1) == L"100");
    CHECK(chart->GetRowLabel(2) == L"2010");
    CHECK(chart->GetCount(1, 0) == 2);

    // values that only differ beyond the label's precision are the same category
    auto closeChart =
        BuildChart(canvas, MakeStudentDataset({ { 0, 0, 0, 0, 1.5 }, { 0, 0, 0, 0, 1.5 + 1e-9 } }),
                   L"year", L"college");
    REQUIRE(closeChart->GetRowCategoryCount() == 1);
    CHECK(closeChart->GetRowLabel(0) == L"1.5");
    CHECK(closeChart->GetCount(0, 0) == 2);
    }

TEST_CASE("MatrixHeatMap handles missing data", "[matrixheatmap]")
    {
    auto* canvas = MakeCanvas();
    auto chart = BuildChart(canvas,
                            MakeStudentDataset({
                                { 0, 0, 0, 0, 2020 },
                                // missing high school, not counted
                                { 9, 0, 0, 0, 2020 },
                                // missing college, not counted
                                { 0, 0, 9, 0, 2020 },
                                // missing year, not counted when year is an axis
                                { 2, 1, 1, 1, std::numeric_limits<double>::quiet_NaN() },
                                // missing group, placed in an unlabeled group at the end
                                { 1, 9, 2, 0, 2021 },
                            }),
                            L"hs", L"college", L"hs_type");

    REQUIRE(chart->GetRowCategoryCount() == 3);
    CHECK(chart->GetRowGroupLabel(0) == L"Private");
    CHECK(chart->GetRowLabel(0) == L"St. Agnes");
    CHECK(chart->GetRowGroupLabel(1) == L"Public");
    CHECK(chart->GetRowLabel(1) == L"West");
    CHECK(chart->GetRowGroupLabel(2).empty());
    CHECK(chart->GetRowLabel(2) == L"central");

    REQUIRE(chart->GetColumnCategoryCount() == 3);
    // West -> State U is the only complete record for West
    CHECK(chart->GetCount(1, 1) == 1);
    // the missing college isn't a column, so West has nothing else
    CHECK(chart->GetCount(1, 0) == 0);
    CHECK(chart->GetCount(1, 2) == 0);

    // with year as the rows, the NaN year is dropped
    auto yearChart = BuildChart(canvas,
                                MakeStudentDataset({
                                    { 0, 0, 0, 0, 2020 },
                                    { 2, 1, 1, 1, std::numeric_limits<double>::quiet_NaN() },
                                }),
                                L"year", L"college");
    REQUIRE(yearChart->GetRowCategoryCount() == 1);
    CHECK(yearChart->GetRowLabel(0) == L"2020");
    }

TEST_CASE("MatrixHeatMap shows values with no complete records as empty rows", "[matrixheatmap]")
    {
    auto* canvas = MakeCanvas();
    auto chart = BuildChart(canvas,
                            MakeStudentDataset({
                                { 0, 0, 0, 0, 2020 },
                                // St. Agnes sends no students to college
                                { 2, 1, 9, 0, 2020 },
                                { 2, 1, 9, 0, 2021 },
                            }),
                            L"hs", L"college");

    REQUIRE(chart->GetRowCategoryCount() == 2);
    CHECK(chart->GetRowLabel(0) == L"St. Agnes");
    CHECK(chart->GetRowLabel(1) == L"West");
    REQUIRE(chart->GetColumnCategoryCount() == 1);
    CHECK(chart->GetCount(0, 0) == 0);
    CHECK(chart->GetCount(1, 0) == 1);
    }

TEST_CASE("MatrixHeatMap shows counts or percentages", "[matrixheatmap]")
    {
    using Catch::Matchers::WithinAbs;

    // rows: Central, St. Agnes, West (totals 3, 3, 2)
    // columns: Ivy College, State U, Tech (totals 4, 3, 1)
    auto* canvas = MakeCanvas();
    auto chart = BuildChart(canvas, MakeStudentDataset(SampleStudents()), L"hs", L"college");

    SECTION("Counts by default")
        {
        CHECK(chart->GetCellValues() == CrossTabValue::Count);
        CHECK_THAT(chart->GetCellValue(1, 0), WithinAbs(3, 1e-9));
        CHECK_THAT(chart->GetCellValue(0, 1), WithinAbs(2, 1e-9));
        }

    SECTION("Row percentages")
        {
        chart->SetCellValues(CrossTabValue::RowPercentage);
        CHECK_THAT(chart->GetCellValue(0, 0), WithinAbs(100.0 / 3, 1e-6));
        CHECK_THAT(chart->GetCellValue(0, 1), WithinAbs(200.0 / 3, 1e-6));
        CHECK_THAT(chart->GetCellValue(1, 0), WithinAbs(100, 1e-9));
        CHECK_THAT(chart->GetCellValue(2, 1), WithinAbs(50, 1e-9));
        CHECK_THAT(chart->GetCellValue(2, 2), WithinAbs(50, 1e-9));
        CHECK_THAT(chart->GetCellValue(1, 1), WithinAbs(0, 1e-9));
        }

    SECTION("Column percentages")
        {
        chart->SetCellValues(CrossTabValue::ColumnPercentage);
        CHECK_THAT(chart->GetCellValue(0, 0), WithinAbs(25, 1e-9));
        CHECK_THAT(chart->GetCellValue(1, 0), WithinAbs(75, 1e-9));
        CHECK_THAT(chart->GetCellValue(0, 1), WithinAbs(200.0 / 3, 1e-6));
        CHECK_THAT(chart->GetCellValue(2, 2), WithinAbs(100, 1e-9));
        }

    SECTION("Total percentages")
        {
        chart->SetCellValues(CrossTabValue::TotalPercentage);
        CHECK_THAT(chart->GetCellValue(1, 0), WithinAbs(37.5, 1e-9));
        CHECK_THAT(chart->GetCellValue(2, 2), WithinAbs(12.5, 1e-9));
        }

    SECTION("Counts are unchanged by showing percentages")
        {
        chart->SetCellValues(CrossTabValue::RowPercentage);
        CHECK(chart->GetCount(1, 0) == 3);
        CHECK(chart->GetMaxCount() == 3);
        chart->SetCellValues(CrossTabValue::Count);
        CHECK_THAT(chart->GetCellValue(1, 0), WithinAbs(3, 1e-9));
        }

    SECTION("A row with no complete records has no percentages")
        {
        // a high school whose only student has no college listed
        auto students = SampleStudents();
        students.push_back({ 1, 0, 9, 0, 2020 });
        auto emptyRowChart = BuildChart(canvas, MakeStudentDataset(students), L"hs", L"college");
        emptyRowChart->SetCellValues(CrossTabValue::RowPercentage);
        // "central" sorts right after "Central"
        REQUIRE(emptyRowChart->GetRowLabel(1) == L"central");
        CHECK_THAT(emptyRowChart->GetCellValue(1, 0), WithinAbs(0, 1e-9));
        CHECK_THAT(emptyRowChart->GetCellValue(1, 1), WithinAbs(0, 1e-9));
        }
    }

TEST_CASE("MatrixHeatMap throws on unknown columns", "[matrixheatmap]")
    {
    auto* canvas = MakeCanvas();
    const auto dataset = MakeStudentDataset(SampleStudents());
    auto chart = std::make_shared<MatrixHeatMap>(canvas);
    CHECK_THROWS_AS(chart->SetData(dataset, L"nope", L"college"), std::runtime_error);
    CHECK_THROWS_AS(chart->SetData(dataset, L"hs", L"nope"), std::runtime_error);
    CHECK_THROWS_AS(chart->SetData(dataset, L"hs", L"college", L"nope"), std::runtime_error);
    // group columns must be categorical
    CHECK_THROWS_AS(chart->SetData(dataset, L"hs", L"college", std::nullopt, L"year"),
                    std::runtime_error);
    }

TEST_CASE("MatrixHeatMap layout is deterministic and complete", "[matrixheatmap][render]")
    {
    const size_t baseObjects{ BaseObjectCount() };

    SECTION("ungrouped")
        {
        auto* canvas = MakeCanvas();
        auto chart = BuildChart(canvas, MakeStudentDataset(SampleStudents()), L"hs", L"college");
        LayOutOffscreen(canvas, chart);
        const size_t firstPass{ chart->GetObjectCount() };
        LayOutOffscreen(canvas, chart);
        CHECK(chart->GetObjectCount() == firstPass);

        // 9 cells, 5 non-zero counts, 3 row labels, 3 column labels
        CHECK(firstPass == baseObjects + 9 + 5 + 3 + 3);
        }

    SECTION("grouped")
        {
        auto* canvas = MakeCanvas();
        auto chart = BuildChart(canvas, MakeStudentDataset(SampleStudents()), L"hs", L"college",
                                L"hs_type", L"college_type");
        LayOutOffscreen(canvas, chart);
        const size_t firstPass{ chart->GetObjectCount() };
        LayOutOffscreen(canvas, chart);
        CHECK(chart->GetObjectCount() == firstPass);

        // 9 cells, 5 non-zero counts, 3 row labels, 3 column labels,
        // 2 row groups (2 brackets, 1 separator), and 2 column groups (2 brackets, 1 separator)
        CHECK(firstPass == baseObjects + 9 + 5 + 3 + 3 + (2 + 2 + 1) + (2 + 2 + 1));
        }

    SECTION("with totals")
        {
        auto* canvas = MakeCanvas();
        auto chart = BuildChart(canvas, MakeStudentDataset(SampleStudents()), L"hs", L"college");
        chart->ShowTotals(true);
        LayOutOffscreen(canvas, chart);
        const size_t firstPass{ chart->GetObjectCount() };
        LayOutOffscreen(canvas, chart);
        CHECK(chart->GetObjectCount() == firstPass);

        // 9 cells, 5 non-zero counts, 4 row labels, 4 column labels (including "Total"),
        // 7 totals (3 rows, 3 columns, and the grand total), and the lines between the totals
        CHECK(firstPass == baseObjects + 9 + 5 + 4 + 4 + 7 + 1);
        }
    }

TEST_CASE("MatrixHeatMap totals", "[matrixheatmap]")
    {
    // rows: Central, St. Agnes, West (totals 3, 3, 2)
    // columns: Ivy College, State U, Tech (totals 4, 3, 1)
    auto* canvas = MakeCanvas();
    auto chart = BuildChart(canvas, MakeStudentDataset(SampleStudents()), L"hs", L"college");
    CHECK_FALSE(chart->IsShowingTotals());

    const auto checkTotals = [&chart]()
    {
        CHECK(chart->GetRowTotal(0) == 3);
        CHECK(chart->GetRowTotal(1) == 3);
        CHECK(chart->GetRowTotal(2) == 2);
        CHECK(chart->GetColumnTotal(0) == 4);
        CHECK(chart->GetColumnTotal(1) == 3);
        CHECK(chart->GetColumnTotal(2) == 1);
        CHECK(chart->GetGrandTotal() == 8);
        CHECK(chart->GetRowTotal(99) == 0);
    };

    SECTION("Counts") { checkTotals(); }

    SECTION("Totals stay counts when the cells show percentages")
        {
        chart->SetCellValues(CrossTabValue::RowPercentage);
        checkTotals();
        chart->SetCellValues(CrossTabValue::ColumnPercentage);
        checkTotals();
        chart->SetCellValues(CrossTabValue::TotalPercentage);
        checkTotals();
        }

    SECTION("A row with no complete records totals zero")
        {
        auto students = SampleStudents();
        students.push_back({ 1, 0, 9, 0, 2020 });
        auto emptyRowChart = BuildChart(canvas, MakeStudentDataset(students), L"hs", L"college");
        REQUIRE(emptyRowChart->GetRowLabel(1) == L"central");
        CHECK(emptyRowChart->GetRowTotal(1) == 0);
        // the record without a college isn't in the grand total
        CHECK(emptyRowChart->GetGrandTotal() == 8);
        }
    }
