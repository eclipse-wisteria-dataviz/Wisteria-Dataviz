/** @addtogroup Graphics
    @brief Graphing classes.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef WISTERIA_MATRIX_HEATMAP_H
#define WISTERIA_MATRIX_HEATMAP_H

#include "graph2d.h"

namespace Wisteria::Graphs
    {
    /** @brief A heat map that tallies how often each combination of two variables occurs.
        @details The unique values of one column run down the rows and the unique values of
         another column run across the columns. Each cell shows how many observations have
         that combination, and its color shows that count relative to the other cells.

         This is useful for finding the most common pairings between two variables
         (e.g., which high schools send the most students to which colleges).

        @par %Data:
         This plot accepts a Data::Dataset with two columns to cross tabulate.
         Each can be either categorical or continuous (with discrete values, such as years).
         The order of the data does not matter.

         Each axis can also be split into groups by its own optional categorical column.
         A value is tallied separately within each of its groups, so two schools with the
         same name (e.g., one public and one private) are shown as separate rows.

         | HIGH_SCHOOL  | HS_TYPE | COLLEGE        | COLLEGE_TYPE |
         | :--          | :--     | :--            | :--          |
         | Central      | Public  | State U        | Public       |
         | Central      | Public  | Ivy College    | Private      |
         | St. Agnes    | Private | Ivy College    | Private      |
         | St. Agnes    | Private | Ivy College    | Private      |
         | West         | Public  | State U        | Public       |

         ...

         With the above dataset, `HIGH_SCHOOL` will be the rows and `COLLEGE` the columns.
         `HS_TYPE` can optionally group the rows, and `COLLEGE_TYPE` the columns.

        @par Cell Values:
         By default, each cell shows its count. Call SetCellValues() to show percentages
         of each row's total, each column's total, or all observations instead.
         The colors follow the values shown.

         Row percentages are useful when the rows differ in size. For example, a large high
         school would otherwise darken its whole row, just from having more students.

         Call ShowTotals() to add a total for each row and column, along with the grand total.
         The totals are always counts and are not colored.

        @par Sorting:
         Groups are sorted alphabetically, and the values within each group are sorted
         alphabetically (or numerically, for continuous columns). Rows run top to bottom
         and columns run left to right.

        @par Missing Data:
         - Observations missing either the row or column value are not counted.
           Their other value is still shown, though. For example, a high school whose
           students have no college listed is shown as a row of empty cells.
         - Observations missing a group value are placed into an unlabeled group,
           shown after the other groups.
         - Combinations that never occur are shown as empty cells.

        @par Example:
         This example uses a home physical therapist's log of sessions around Seattle,
         with one row per session. It shows which therapies are most often given in each
         neighborhood. The neighborhoods are grouped by area of the city (e.g., North Seattle),
         and the therapies are grouped by their goal (e.g., mobility or strength).
        @code
         auto canvas = new Wisteria::Canvas{ this };
         canvas->SetFixedObjectsGridSize(1, 2);
         auto sessionData = std::make_shared<Data::Dataset>();
         try
            {
            sessionData->ImportCSV(L"/home/daphne/data/Therapy Sessions.csv",
                ImportInfo().CategoricalColumns(
                    { { L"Neighborhood", CategoricalImportMethod::ReadAsStrings },
                      { L"Area", CategoricalImportMethod::ReadAsStrings },
                      { L"Therapy", CategoricalImportMethod::ReadAsStrings },
                      { L"Goal", CategoricalImportMethod::ReadAsStrings } }));
            }
         catch (const std::exception& err)
            {
            wxMessageBox(wxString::FromUTF8(err.what()), _(L"Import Error"),
                         wxOK | wxICON_ERROR | wxCENTRE);
            return;
            }

         auto plot = std::make_shared<MatrixHeatMap>(canvas);
         plot->SetData(sessionData, L"Neighborhood", L"Therapy", L"Area", L"Goal");
         // show the share of each neighborhood's sessions for each therapy
         plot->SetCellValues(CrossTabValue::RowPercentage);
         plot->ShowTotals(true);

         canvas->SetFixedObject(0, 0, plot);
         canvas->SetFixedObject(0, 1,
            plot->CreateLegend(LegendOptions{}.IncludeHeader(true).PlacementHint(
                LegendCanvasPlacementHint::RightOfGraph)));
        @endcode*/
    class MatrixHeatMap final : public Graph2D
        {
        wxDECLARE_DYNAMIC_CLASS(MatrixHeatMap);
        MatrixHeatMap() = default;

      public:
        /** @brief Constructor.
            @param canvas The canvas that the plot is plotted on.
            @param colors The color scheme to apply to the cells.\n
                Leave as @c nullptr to use white (low counts) to black (high counts).
            @note The first colors map to the lower counts, the last colors to the higher.*/
        explicit MatrixHeatMap(
            Canvas* canvas, const std::shared_ptr<Colors::Schemes::ColorScheme>& colors = nullptr);

        /** @brief Sets the data to cross tabulate.
            @param data The data.
            @param rowColumnName The categorical or continuous column whose values
                run down the rows.
            @param columnColumnName The categorical or continuous column whose values
                run across the columns.
            @param rowGroupColumnName An optional categorical column to group the rows by.
            @param columnGroupColumnName An optional categorical column to group the columns by.
            @note Call the parent canvas's `CalcAllSizes()` when setting to a new dataset to
                re-plot the data.
            @throws std::runtime_error If any columns can't be found by name,
                throws an exception.\n
                The exception's @c what() message is UTF-8 encoded, so pass it to
                @c wxString::FromUTF8() when formatting it for an error message.*/
        void SetData(const std::shared_ptr<const Data::Dataset>& data,
                     const wxString& rowColumnName, const wxString& columnColumnName,
                     const std::optional<wxString>& rowGroupColumnName = std::nullopt,
                     const std::optional<wxString>& columnGroupColumnName = std::nullopt);

        /// @brief Sets which value each cell shows (and is colored by).
        /// @param cellValues The value to show.
        void SetCellValues(CrossTabValue cellValues);

        /// @returns Which value each cell shows.
        [[nodiscard]]
        CrossTabValue GetCellValues() const noexcept
            {
            return m_cellValueType;
            }

        /// @brief Sets whether to show a total for each row and column,
        ///     along with the grand total.
        /// @param show @c true to show the totals.
        /// @details The totals are shown in a row below the matrix and a column to its right.
        ///     They are always counts, so with percentages they show what each
        ///     percentage is out of.
        void ShowTotals(const bool show) noexcept { m_showTotals = show; }

        /// @returns Whether a total is shown for each row and column.
        [[nodiscard]]
        bool IsShowingTotals() const noexcept
            {
            return m_showTotals;
            }

        /// @returns A row's total count, or zero if @p row is out of range.
        /// @param row The row's index.
        [[nodiscard]]
        size_t GetRowTotal(const size_t row) const noexcept
            {
            return (row < m_rowTotals.size()) ? m_rowTotals[row] : 0;
            }

        /// @returns A column's total count, or zero if @p column is out of range.
        /// @param column The column's index.
        [[nodiscard]]
        size_t GetColumnTotal(const size_t column) const noexcept
            {
            return (column < m_columnTotals.size()) ? m_columnTotals[column] : 0;
            }

        /// @returns The number of observations with both a row and a column value.
        [[nodiscard]]
        size_t GetGrandTotal() const noexcept
            {
            return m_grandTotal;
            }

        /// @returns The column whose values run down the rows.
        [[nodiscard]]
        const wxString& GetRowColumnName() const noexcept
            {
            return m_rowColumnName;
            }

        /// @returns The column whose values run across the columns.
        [[nodiscard]]
        const wxString& GetColumnColumnName() const noexcept
            {
            return m_columnColumnName;
            }

        /// @returns The column grouping the rows, or empty if not grouped.
        [[nodiscard]]
        const wxString& GetRowGroupColumnName() const noexcept
            {
            return m_rowGroupColumnName;
            }

        /// @returns The column grouping the columns, or empty if not grouped.
        [[nodiscard]]
        const wxString& GetColumnGroupColumnName() const noexcept
            {
            return m_columnGroupColumnName;
            }

        /// @returns The lowest count among the combinations that occur.
        [[nodiscard]]
        size_t GetMinCount() const noexcept
            {
            return m_minCount;
            }

        /// @returns The highest count among all combinations.
        [[nodiscard]]
        size_t GetMaxCount() const noexcept
            {
            return m_maxCount;
            }

        /// @returns The number of rows.
        [[nodiscard]]
        size_t GetRowCategoryCount() const noexcept
            {
            return m_rowAxis.m_categories.size();
            }

        /// @returns The number of columns.
        [[nodiscard]]
        size_t GetColumnCategoryCount() const noexcept
            {
            return m_columnAxis.m_categories.size();
            }

        /// @returns A row's label, or empty if @p row is out of range.
        /// @param row The row's index.
        [[nodiscard]]
        wxString GetRowLabel(const size_t row) const
            {
            return (row < m_rowAxis.m_categories.size()) ? m_rowAxis.m_categories[row].m_label :
                                                           wxString{};
            }

        /// @returns A row's group label, or empty if @p row is out of range or not grouped.
        /// @param row The row's index.
        [[nodiscard]]
        wxString GetRowGroupLabel(const size_t row) const
            {
            return (row < m_rowAxis.m_categories.size()) ?
                       m_rowAxis.m_categories[row].m_groupLabel :
                       wxString{};
            }

        /// @returns A column's label, or empty if @p column is out of range.
        /// @param column The column's index.
        [[nodiscard]]
        wxString GetColumnLabel(const size_t column) const
            {
            return (column < m_columnAxis.m_categories.size()) ?
                       m_columnAxis.m_categories[column].m_label :
                       wxString{};
            }

        /// @returns A column's group label, or empty if @p column is out of range or not grouped.
        /// @param column The column's index.
        [[nodiscard]]
        wxString GetColumnGroupLabel(const size_t column) const
            {
            return (column < m_columnAxis.m_categories.size()) ?
                       m_columnAxis.m_categories[column].m_groupLabel :
                       wxString{};
            }

        /// @returns How many observations have a row's and a column's values,
        ///     or zero if either is out of range.
        /// @param row The row's index.
        /// @param column The column's index.
        [[nodiscard]]
        size_t GetCount(const size_t row, const size_t column) const noexcept
            {
            return (row < m_counts.size() && column < m_counts[row].size()) ?
                       m_counts[row][column] :
                       0;
            }

        /// @returns The value shown in a cell (a count, or a percentage from 0 to 100),
        ///     or zero if either index is out of range.
        /// @param row The row's index.
        /// @param column The column's index.
        [[nodiscard]]
        double GetCellValue(const size_t row, const size_t column) const noexcept
            {
            return (row < m_cellValues.size() && column < m_cellValues[row].size()) ?
                       m_cellValues[row][column] :
                       0;
            }

        /** @brief Builds and returns a legend.
            @param options The options for how to build the legend.
            @returns The legend for the chart.*/
        [[nodiscard]]
        std::unique_ptr<GraphItems::Label> CreateLegend(const LegendOptions& options) final;

      private:
        /// @brief A value along an axis, along with the group it is in.
        struct AxisCategory
            {
            wxString m_groupLabel;
            wxString m_label;
            double m_numericValue{ 0 };
            };

        /// @brief The unique categories along an axis, in display order.
        struct AxisCategories
            {
            std::vector<AxisCategory> m_categories;
            bool m_isNumeric{ false };
            bool m_isGrouped{ false };
            };

        /// @brief Sets the graph's descriptive string for screen readers.
        void SetAutoAccessibilityAttributes() final;

        void RecalcSizes(wxDC& dc) final;

        /// @returns The display order of the categories along an axis, with each
        ///     observation's index into those categories (or @c std::nullopt if missing).
        /// @param valueColumnName The column whose values are along the axis.
        /// @param groupColumnName The column grouping the axis, or empty if not grouped.
        /// @param[out] axis The axis's categories.
        [[nodiscard]]
        std::vector<std::optional<size_t>> BuildAxis(const wxString& valueColumnName,
                                                     const wxString& groupColumnName,
                                                     AxisCategories& axis) const;

        /// @brief Calculates the cells' values from the counts and colors the cells by them.
        void CalcCellValues();

        /// @returns A cell value, formatted for display.
        /// @param value The value to format.
        [[nodiscard]]
        wxString FormatCellValue(double value) const;

        /// @returns A category's label, followed by its group (if it has one).
        /// @param category The category to describe.
        [[nodiscard]]
        static wxString FormatCategoryLabel(const AxisCategory& category)
            {
            return category.m_groupLabel.empty() ?
                       category.m_label :
                       wxString::Format(L"%s (%s)", category.m_label, category.m_groupLabel);
            }

        /// @returns Whether the cells show percentages instead of counts.
        [[nodiscard]]
        bool IsShowingPercentages() const noexcept
            {
            return m_cellValueType != CrossTabValue::Count;
            }

        AxisCategories m_rowAxis;
        AxisCategories m_columnAxis;
        // the counts, indexed by row and then column
        std::vector<std::vector<size_t>> m_counts;
        // the values shown in the cells, indexed the same as the counts
        std::vector<std::vector<double>> m_cellValues;
        std::vector<std::vector<wxColour>> m_cellColors;
        std::vector<wxColour> m_reversedColorSpectrum; // used for the legend
        size_t m_minCount{ 0 };
        size_t m_maxCount{ 0 };
        double m_minCellValue{ 0 };
        double m_maxCellValue{ 0 };
        CrossTabValue m_cellValueType{ CrossTabValue::Count };

        std::vector<size_t> m_rowTotals;
        std::vector<size_t> m_columnTotals;
        size_t m_grandTotal{ 0 };
        bool m_showTotals{ false };

        wxString m_rowColumnName;
        wxString m_columnColumnName;
        wxString m_rowGroupColumnName;
        wxString m_columnGroupColumnName;
        };
    } // namespace Wisteria::Graphs

/** @}*/

#endif // WISTERIA_MATRIX_HEATMAP_H
