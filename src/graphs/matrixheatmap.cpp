///////////////////////////////////////////////////////////////////////////////
// Name:        matrixheatmap.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "matrixheatmap.h"

wxIMPLEMENT_DYNAMIC_CLASS(Wisteria::Graphs::MatrixHeatMap, Wisteria::Graphs::Graph2D)

    namespace Wisteria::Graphs
    {
    //----------------------------------------------------------------
    MatrixHeatMap::MatrixHeatMap(
        Canvas * canvas, const std::shared_ptr<Colors::Schemes::ColorScheme>& colors /*= nullptr*/)
        : Graph2D(canvas)
        {
        SetColorScheme(
            colors != nullptr ?
                colors :
                std::make_unique<Colors::Schemes::ColorScheme>(Colors::Schemes::ColorScheme{
                    Colors::ColorBrewer::GetColor(Colors::Color::White),
                    Colors::ColorBrewer::GetColor(Colors::Color::Black) }));

        GetBottomXAxis().SetRange(0, 10, 0, 1, 1);
        GetLeftYAxis().SetRange(0, 10, 0, 1, 1);
        GetBottomXAxis().Show(false);
        GetLeftYAxis().Show(false);
        GetTopXAxis().Show(false);
        GetRightYAxis().Show(false);
        GetPen().SetColour(L"#BEBBBB");
        }

    //----------------------------------------------------------------
    std::vector<std::optional<size_t>> MatrixHeatMap::BuildAxis(const wxString& valueColumnName,
                                                                const wxString& groupColumnName,
                                                                AxisCategories& axis) const
        {
        axis = AxisCategories{};

        // the values are read as categorical first and continuous second
        const auto categoricalColumn = GetDataset()->GetCategoricalColumn(valueColumnName);
        const bool isCategorical{ categoricalColumn !=
                                  GetDataset()->GetCategoricalColumns().cend() };
        const auto continuousColumn = GetDataset()->GetContinuousColumn(valueColumnName);
        if (!isCategorical && continuousColumn == GetDataset()->GetContinuousColumns().cend())
            {
            throw std::runtime_error(
                wxString::Format(_(L"'%s': categorical or continuous column not found for "
                                   "matrix heatmap."),
                                 valueColumnName)
                    .ToUTF8());
            }
        axis.m_isNumeric = !isCategorical;

        axis.m_isGrouped = !groupColumnName.empty();
        const auto groupColumn = axis.m_isGrouped ? GetCategoricalColumn(groupColumnName) :
                                                    GetDataset()->GetCategoricalColumns().cend();

        // each observation's category, or nothing if its value is missing
        std::vector<std::optional<AxisCategory>> observations(GetDataset()->GetRowCount());
        for (size_t i = 0; i < observations.size(); ++i)
            {
            AxisCategory category;
            if (isCategorical)
                {
                if (categoricalColumn->IsMissingData(i))
                    {
                    continue;
                    }
                category.m_label = categoricalColumn->GetValueAsLabel(i);
                }
            else
                {
                const double value{ continuousColumn->GetValue(i) };
                if (!std::isfinite(value))
                    {
                    continue;
                    }
                // round to the label's precision, so that values shown with the same
                // label are the same category
                constexpr int LABEL_PRECISION{ 6 };
                const double precisionFactor{ std::pow(10.0, LABEL_PRECISION) };
                const double roundedValue{ std::round(value * precisionFactor) / precisionFactor };
                category.m_numericValue = std::isfinite(roundedValue) ? roundedValue : value;
                category.m_label =
                    wxNumberFormatter::ToString(category.m_numericValue, LABEL_PRECISION,
                                                wxNumberFormatter::Style::Style_NoTrailingZeroes);
                }
            if (axis.m_isGrouped)
                {
                category.m_groupLabel = groupColumn->GetValueAsLabel(i);
                }
            observations[i] = std::move(category);
            }

        const bool isNumeric{ axis.m_isNumeric };
        const auto compareStrings = [](const wxString& lhs, const wxString& rhs)
        {
            const int result{ lhs.CmpNoCase(rhs) };
            return (result != 0) ? (result < 0) : (lhs.Cmp(rhs) < 0);
        };
        const auto lessThan =
            [isNumeric, &compareStrings](const AxisCategory& lhs, const AxisCategory& rhs)
        {
            if (lhs.m_groupLabel != rhs.m_groupLabel)
                {
                // the unlabeled group goes last
                if (lhs.m_groupLabel.empty())
                    {
                    return false;
                    }
                if (rhs.m_groupLabel.empty())
                    {
                    return true;
                    }
                return compareStrings(lhs.m_groupLabel, rhs.m_groupLabel);
                }
            return isNumeric ? (lhs.m_numericValue < rhs.m_numericValue) :
                               compareStrings(lhs.m_label, rhs.m_label);
        };
        const auto isEquivalent = [&lessThan](const AxisCategory& lhs, const AxisCategory& rhs)
        { return !lessThan(lhs, rhs) && !lessThan(rhs, lhs); };

        // the unique categories, sorted
        for (const auto& observation : observations)
            {
            if (observation)
                {
                axis.m_categories.push_back(observation.value());
                }
            }
        std::ranges::sort(axis.m_categories, lessThan);
        const auto duplicates = std::ranges::unique(axis.m_categories, isEquivalent);
        axis.m_categories.erase(duplicates.begin(), duplicates.end());

        // map each observation to its category's position
        std::vector<std::optional<size_t>> positions(observations.size());
        for (size_t i = 0; i < observations.size(); ++i)
            {
            if (observations[i])
                {
                const auto foundPos =
                    std::ranges::lower_bound(axis.m_categories, observations[i].value(), lessThan);
                positions[i] = static_cast<size_t>(
                    std::ranges::distance(axis.m_categories.cbegin(), foundPos));
                }
            }
        return positions;
        }

    //----------------------------------------------------------------
    void MatrixHeatMap::SetData(
        const std::shared_ptr<const Data::Dataset>& data, const wxString& rowColumnName,
        const wxString& columnColumnName,
        const std::optional<wxString>& rowGroupColumnName /*= std::nullopt*/,
        const std::optional<wxString>& columnGroupColumnName
        /*= std::nullopt*/)
        {
        SetDataset(data);
        GetSelectedIds().clear();
        m_rowColumnName = rowColumnName;
        m_columnColumnName = columnColumnName;
        m_rowGroupColumnName = rowGroupColumnName.value_or(wxString{});
        m_columnGroupColumnName = columnGroupColumnName.value_or(wxString{});
        m_rowAxis = AxisCategories{};
        m_columnAxis = AxisCategories{};
        m_counts.clear();
        m_cellValues.clear();
        m_rowTotals.clear();
        m_columnTotals.clear();
        m_grandTotal = 0;
        m_cellColors.clear();
        m_minCount = m_maxCount = 0;
        m_minCellValue = m_maxCellValue = 0;

        if (GetDataset() == nullptr)
            {
            return;
            }

        const auto rowPositions = BuildAxis(m_rowColumnName, m_rowGroupColumnName, m_rowAxis);
        const auto columnPositions =
            BuildAxis(m_columnColumnName, m_columnGroupColumnName, m_columnAxis);

        // tally each combination
        m_counts.assign(m_rowAxis.m_categories.size(),
                        std::vector<size_t>(m_columnAxis.m_categories.size(), 0));
        for (size_t i = 0; i < rowPositions.size(); ++i)
            {
            if (rowPositions[i] && columnPositions[i])
                {
                ++m_counts[rowPositions[i].value()][columnPositions[i].value()];
                }
            }

        CalcCellValues();
        }

    //----------------------------------------------------------------
    void MatrixHeatMap::SetCellValues(const CrossTabValue cellValues)
        {
        m_cellValueType = cellValues;
        CalcCellValues();
        }

    //----------------------------------------------------------------
    void MatrixHeatMap::CalcCellValues()
        {
        const size_t rowCount{ m_counts.size() };
        const size_t columnCount{ m_columnAxis.m_categories.size() };

        // the totals, which the percentages are also of
        m_rowTotals.assign(rowCount, 0);
        m_columnTotals.assign(columnCount, 0);
        m_grandTotal = 0;
        for (size_t row = 0; row < rowCount; ++row)
            {
            for (size_t col = 0; col < columnCount; ++col)
                {
                m_rowTotals[row] += m_counts[row][col];
                m_columnTotals[col] += m_counts[row][col];
                m_grandTotal += m_counts[row][col];
                }
            }

        // converts a count to the value shown, given the totals of its row and column
        const auto calcValue =
            [this](const size_t count, const size_t rowTotal, const size_t columnTotal)
        {
            const auto countValue{ static_cast<double>(count) };
            switch (m_cellValueType)
                {
            case CrossTabValue::RowPercentage:
                return safe_divide<double>(countValue, rowTotal) * 100;
            case CrossTabValue::ColumnPercentage:
                return safe_divide<double>(countValue, columnTotal) * 100;
            case CrossTabValue::TotalPercentage:
                return safe_divide<double>(countValue, m_grandTotal) * 100;
            case CrossTabValue::Count:
                [[fallthrough]];
            default:
                return countValue;
                }
        };

        m_cellValues.assign(rowCount, std::vector<double>(columnCount, 0));
        for (size_t row = 0; row < rowCount; ++row)
            {
            for (size_t col = 0; col < columnCount; ++col)
                {
                m_cellValues[row][col] =
                    calcValue(m_counts[row][col], m_rowTotals[row], m_columnTotals[col]);
                }
            }

        // prepare the colors
        m_reversedColorSpectrum = GetColorScheme()->GetColors();
        std::ranges::reverse(m_reversedColorSpectrum);

        // combinations that never occur are left uncolored
        std::vector<double> occurringCounts;
        std::vector<double> occurringValues;
        for (size_t row = 0; row < rowCount; ++row)
            {
            for (size_t col = 0; col < columnCount; ++col)
                {
                if (m_counts[row][col] > 0)
                    {
                    occurringCounts.push_back(static_cast<double>(m_counts[row][col]));
                    occurringValues.push_back(m_cellValues[row][col]);
                    }
                }
            }
        m_cellColors.assign(rowCount, std::vector<wxColour>(columnCount));
        if (occurringValues.empty())
            {
            m_minCount = m_maxCount = 0;
            m_minCellValue = m_maxCellValue = 0;
            return;
            }
        m_minCount = static_cast<size_t>(*std::ranges::min_element(occurringCounts));
        m_maxCount = static_cast<size_t>(*std::ranges::max_element(occurringCounts));
        m_minCellValue = *std::ranges::min_element(occurringValues);
        m_maxCellValue = *std::ranges::max_element(occurringValues);

        Colors::ColorBrewer colorBrewer;
        colorBrewer.SetColorScale(GetColorScheme()->GetColors().cbegin(),
                                  GetColorScheme()->GetColors().cend());
        const auto brewedColors = colorBrewer.BrewColors(occurringValues);
        size_t colorIndex{ 0 };
        for (size_t row = 0; row < rowCount; ++row)
            {
            for (size_t col = 0; col < columnCount; ++col)
                {
                if (m_counts[row][col] > 0)
                    {
                    m_cellColors[row][col] = brewedColors[colorIndex++];
                    }
                }
            }
        }

    //----------------------------------------------------------------
    wxString MatrixHeatMap::FormatCellValue(const double value) const
        {
        if (!IsShowingPercentages())
            {
            return wxNumberFormatter::ToString(value, 0, Settings::GetDefaultNumberFormat());
            }
        // A combination that occurs never shows as 0%. Likewise, only a cell that is
        // the entire row (or column, or table) shows as 100%.
        if (value > 0 && value < 0.5)
            {
            return _(L"<1%");
            }
        if (value >= 99.5 && value < 100)
            {
            return _(L">99%");
            }
        return wxNumberFormatter::ToString(value, 0, wxNumberFormatter::Style::Style_None) + L"%";
        }

    //----------------------------------------------------------------
    void MatrixHeatMap::RecalcSizes(wxDC & dc)
        {
        // if no data, then bail
        if (GetDataset() == nullptr || m_counts.empty() || m_columnAxis.m_categories.empty())
            {
            return;
            }

        Graph2D::RecalcSizes(dc);

        const size_t rowCount{ m_rowAxis.m_categories.size() };
        const size_t columnCount{ m_columnAxis.m_categories.size() };
        // the totals take an extra row and column
        const size_t displayRowCount{ rowCount + (m_showTotals ? 1 : 0) };
        const size_t displayColumnCount{ columnCount + (m_showTotals ? 1 : 0) };
        const wxString totalLabel{ _(L"Total") };

        constexpr wxCoord LABEL_PADDING{ 4 };
        const wxCoord padding{ static_cast<wxCoord>(ScaleToScreenAndCanvas(LABEL_PADDING)) };
        const wxRect drawArea{ GetPlotAreaBoundingBox() };
        const wxFont axisFont{ GetBottomXAxis().GetFont() };
        // labels drawn with a fitted font keep their scaling at 1,
        // so the axis font's size is capped at its scaled size
        const double maxFontSize{ axisFont.GetFractionalPointSize() * GetScaling() };

        GraphItems::Label measuringLabel(GraphItems::GraphItemInfo{}
                                             .Scaling(GetScaling())
                                             .Pen(wxNullPen)
                                             .DPIScaling(GetDPIScaleFactor())
                                             .Font(axisFont));
        // measures the same way as the drawn labels, which keep their scaling at 1
        GraphItems::Label fittedMeasuringLabel(
            GraphItems::GraphItemInfo{}.Pen(wxNullPen).DPIScaling(GetDPIScaleFactor()));
        // Returns the widest string and its measured width, the height of one line, and
        // whether the widest string is bold. If requested, the last string is measured bold
        // (that is the "Total" label, which is drawn bold).
        const auto measureWidest = [&dc](GraphItems::Label& label,
                                         const std::vector<wxString>& strings, const bool boldLast)
        {
            wxString widestStr;
            wxCoord widestWidth{ 0 };
            wxCoord lineHeight{ 0 };
            bool widestIsBold{ false };
            const wxFont regularFont{ label.GetFont() };
            for (size_t i = 0; i < strings.size(); ++i)
                {
                const bool isBold{ boldLast && i + 1 == strings.size() };
                label.GetFont() = isBold ? regularFont.Bold() : regularFont;
                label.SetText(strings[i].empty() ? wxString{ L" " } : strings[i]);
                const wxRect measured{ label.GetBoundingBox(dc) };
                if (measured.GetWidth() > widestWidth)
                    {
                    widestWidth = measured.GetWidth();
                    widestStr = strings[i];
                    widestIsBold = isBold;
                    }
                lineHeight = std::max(lineHeight, measured.GetHeight());
                }
            label.GetFont() = regularFont;
            return std::tuple{ widestStr, widestWidth, lineHeight, widestIsBold };
        };

        // the labels along each axis, and the runs of categories in each group
        struct GroupRun
            {
            wxString m_label;
            size_t m_start{ 0 };
            size_t m_length{ 0 };
            };

        const auto collectLabels = [this, &totalLabel](const AxisCategories& axis)
        {
            std::vector<wxString> labels;
            labels.reserve(axis.m_categories.size() + 1);
            for (const auto& category : axis.m_categories)
                {
                labels.push_back(category.m_label);
                }
            if (m_showTotals)
                {
                labels.push_back(totalLabel);
                }
            return labels;
        };
        const auto collectGroupRuns = [](const AxisCategories& axis)
        {
            std::vector<GroupRun> runs;
            if (!axis.m_isGrouped)
                {
                return runs;
                }
            for (size_t i = 0; i < axis.m_categories.size(); ++i)
                {
                if (runs.empty() || runs.back().m_label != axis.m_categories[i].m_groupLabel)
                    {
                    runs.push_back(GroupRun{ axis.m_categories[i].m_groupLabel, i, 0 });
                    }
                ++runs.back().m_length;
                }
            return runs;
        };
        const auto collectGroupLabels = [](const std::vector<GroupRun>& runs)
        {
            std::vector<wxString> labels;
            labels.reserve(runs.size());
            for (const auto& run : runs)
                {
                labels.push_back(run.m_label);
                }
            return labels;
        };

        const std::vector<GroupRun> rowGroupRuns{ collectGroupRuns(m_rowAxis) };
        const std::vector<GroupRun> columnGroupRuns{ collectGroupRuns(m_columnAxis) };

        const std::vector<wxString> rowLabels{ collectLabels(m_rowAxis) };
        const std::vector<wxString> columnLabels{ collectLabels(m_columnAxis) };
        const std::vector<wxString> rowGroupLabels{ collectGroupLabels(rowGroupRuns) };
        const std::vector<wxString> columnGroupLabels{ collectGroupLabels(columnGroupRuns) };

        [[maybe_unused]] const auto [widestRowStr, widestRowWidth, rowLineHeight, widestRowIsBold] =
            measureWidest(measuringLabel, rowLabels, m_showTotals);
        [[maybe_unused]] const auto [widestColumnStr, widestColumnWidth, columnLineHeight,
                                     widestColumnIsBold] =
            measureWidest(measuringLabel, columnLabels, m_showTotals);
        // group labels are bold so that they stand out from the axis labels
        const wxFont groupFont{ axisFont.Bold() };
        GraphItems::Label groupMeasuringLabel(GraphItems::GraphItemInfo{}
                                                  .Scaling(GetScaling())
                                                  .Pen(wxNullPen)
                                                  .DPIScaling(GetDPIScaleFactor())
                                                  .Font(groupFont));
        [[maybe_unused]] const auto [widestRowGroupStr, widestRowGroupWidth, rowGroupLineHeight,
                                     rowGroupIsBold] =
            measureWidest(groupMeasuringLabel, rowGroupLabels, false);
        [[maybe_unused]] const auto [widestColumnGroupStr, widestColumnGroupWidth,
                                     columnGroupLineHeight, columnGroupIsBold] =
            measureWidest(groupMeasuringLabel, columnGroupLabels, false);

        // the space between a group's label and the axis labels, where its bracket is drawn
        const wxCoord bracketGutter{ padding * 2 };

        // the largest cell that fits in the area, after the labels' space is taken out
        const auto calcBoxSize = [&drawArea, displayRowCount, displayColumnCount](
                                     const wxCoord labelWidth, const wxCoord labelHeight)
        {
            return std::max(
                1.0,
                std::min(safe_divide<double>(drawArea.GetWidth() - labelWidth, displayColumnCount),
                         safe_divide<double>(drawArea.GetHeight() - labelHeight, displayRowCount)));
        };

        // Space is first reserved for the labels at the axis font, so that labels never take
        // more than a third of the area in either direction.
        const wxCoord maxLabelWidth{ drawArea.GetWidth() / 3 };
        const wxCoord maxLabelHeight{ drawArea.GetHeight() / 3 };
        const wxCoord maxRowGroupSpace{ rowGroupRuns.empty() ?
                                            0 :
                                            std::min(widestRowGroupWidth + padding + bracketGutter,
                                                     maxLabelWidth) };
        const wxCoord maxRowLabelSpace{ std::min(widestRowWidth + padding,
                                                 maxLabelWidth - maxRowGroupSpace) };
        const wxCoord maxColumnGroupSpace{
            columnGroupRuns.empty() ?
                0 :
                std::min(columnGroupLineHeight + padding + bracketGutter, maxLabelHeight)
        };
        const wxCoord maxColumnLabelSpace{ std::min(widestColumnWidth + padding,
                                                    maxLabelHeight - maxColumnGroupSpace) };
        const double maxSpaceBoxSize{ calcBoxSize(maxRowGroupSpace + maxRowLabelSpace,
                                                  maxColumnGroupSpace + maxColumnLabelSpace) };

        // Fits a font so that a string fits within a width and height. If the string will be
        // drawn bold, then it is measured bold, but the font returned is still the base font.
        const auto fitFont = [&dc, maxFontSize](const wxFont& baseFont, const wxString& str,
                                                const double width, const double height,
                                                const bool measureBold = false)
        {
            wxFont fittedFont{ baseFont };
            fittedFont.SetFractionalPointSize(
                std::min(maxFontSize,
                         GraphItems::Label::CalcFontSizeToFitBoundingBox(
                             dc, measureBold ? baseFont.Bold() : baseFont,
                             wxRect2DDouble{ 0, 0, std::max(1.0, width), std::max(1.0, height) },
                             str.empty() ? wxString{ L" " } : str)));
            return fittedFont;
        };

        const wxFont rowLabelFont{ fitFont(axisFont, widestRowStr, maxRowLabelSpace - padding,
                                           maxSpaceBoxSize, widestRowIsBold) };
        // column labels are drawn vertically, so the string runs along the height
        const wxFont columnLabelFont{ fitFont(axisFont, widestColumnStr,
                                              maxColumnLabelSpace - padding, maxSpaceBoxSize,
                                              widestColumnIsBold) };
        const wxFont rowGroupFont{ fitFont(groupFont, widestRowGroupStr,
                                           maxRowGroupSpace - padding - bracketGutter,
                                           drawArea.GetHeight()) };
        const wxFont columnGroupFont{ fitFont(groupFont, widestColumnGroupStr, drawArea.GetWidth(),
                                              maxColumnGroupSpace - padding - bracketGutter) };

        // the fitted fonts are usually smaller than the axis font,
        // so only the space that the labels need with them is used
        const auto measureFitted =
            [&fittedMeasuringLabel, &measureWidest](
                const wxFont& font, const std::vector<wxString>& strings, const bool boldLast)
        {
            fittedMeasuringLabel.GetFont() = font;
            [[maybe_unused]] const auto [widestStr, widestWidth, lineHeight, widestIsBold] =
                measureWidest(fittedMeasuringLabel, strings, boldLast);
            return std::pair{ widestWidth, lineHeight };
        };
        const wxCoord rowGroupSpace{
            rowGroupRuns.empty() ?
                0 :
                std::min(measureFitted(rowGroupFont, rowGroupLabels, false).first + padding +
                             bracketGutter,
                         maxRowGroupSpace)
        };
        const wxCoord rowLabelSpace{ std::min(
            measureFitted(rowLabelFont, rowLabels, m_showTotals).first + padding,
            maxRowLabelSpace) };
        const wxCoord columnGroupSpace{
            columnGroupRuns.empty() ?
                0 :
                std::min(measureFitted(columnGroupFont, columnGroupLabels, false).second + padding +
                             bracketGutter,
                         maxColumnGroupSpace)
        };
        const wxCoord columnLabelSpace{ std::min(
            measureFitted(columnLabelFont, columnLabels, m_showTotals).first + padding,
            maxColumnLabelSpace) };
        const double boxSize{ calcBoxSize(rowGroupSpace + rowLabelSpace,
                                          columnGroupSpace + columnLabelSpace) };

        // center the labels and grid within the area
        const wxCoord contentWidth{ rowGroupSpace + rowLabelSpace +
                                    static_cast<wxCoord>(boxSize * displayColumnCount) };
        const wxCoord contentHeight{ columnGroupSpace + columnLabelSpace +
                                     static_cast<wxCoord>(boxSize * displayRowCount) };
        const wxCoord contentLeft{ drawArea.GetLeft() +
                                   std::max(0, (drawArea.GetWidth() - contentWidth) / 2) };
        const wxCoord contentTop{ drawArea.GetTop() +
                                  std::max(0, (drawArea.GetHeight() - contentHeight) / 2) };

        const wxCoord gridLeft{ contentLeft + rowGroupSpace + rowLabelSpace };
        const wxCoord gridTop{ contentTop + columnGroupSpace + columnLabelSpace };
        const wxCoord gridRight{ gridLeft + static_cast<wxCoord>(boxSize * columnCount) };
        const wxCoord gridBottom{ gridTop + static_cast<wxCoord>(boxSize * rowCount) };
        // the outer edges, including any totals
        const wxCoord outerRight{ gridLeft + static_cast<wxCoord>(boxSize * displayColumnCount) };
        const wxCoord outerBottom{ gridTop + static_cast<wxCoord>(boxSize * displayRowCount) };

        const auto formatCount = [](const size_t count)
        {
            return wxNumberFormatter::ToString(static_cast<double>(count), 0,
                                               Settings::GetDefaultNumberFormat());
        };
        // The font fits the highest cell value and the grand total (the widest of the totals,
        // which are drawn bold).
        const wxFont valueFont{ fitFont(axisFont, FormatCellValue(m_maxCellValue), boxSize,
                                        boxSize) };
        const wxFont grandTotalFont{ m_showTotals ? fitFont(axisFont, formatCount(m_grandTotal),
                                                            boxSize, boxSize, true) :
                                                    valueFont };
        const wxFont cellLabelFont{ (grandTotalFont.GetFractionalPointSize() <
                                     valueFont.GetFractionalPointSize()) ?
                                        grandTotalFont :
                                        valueFont };

        // the labels keep their scaling at 1, so their minimum sizes
        // are only converted from the screen's DPI
        const auto toLabelDIPs = [this](const double value)
        { return static_cast<wxCoord>(safe_divide(value, GetDPIScaleFactor())); };

        // the cells
        for (size_t row = 0; row < rowCount; ++row)
            {
            for (size_t col = 0; col < columnCount; ++col)
                {
                const size_t count{ m_counts[row][col] };
                const wxColour cellColor{ (count > 0 && m_cellColors[row][col].IsOk()) ?
                                              m_cellColors[row][col] :
                                              wxTransparentColor };
                const wxCoord left{ gridLeft + static_cast<wxCoord>(boxSize * col) };
                const wxCoord top{ gridTop + static_cast<wxCoord>(boxSize * row) };
                const wxCoord right{ gridLeft + static_cast<wxCoord>(boxSize * (col + 1)) };
                const wxCoord bottom{ gridTop + static_cast<wxCoord>(boxSize * (row + 1)) };
                const std::array<wxPoint, 4> pts{ wxPoint{ left, top }, wxPoint{ left, bottom },
                                                  wxPoint{ right, bottom }, wxPoint{ right, top } };

                const wxString countStr{ wxNumberFormatter::ToString(
                    static_cast<double>(count), 0, Settings::GetDefaultNumberFormat()) };
                const wxString cellValueStr{ FormatCellValue(m_cellValues[row][col]) };
                // Grouped values include their group, so same-named values in different
                // groups are told apart. Percentages also show the count that they are from.
                const wxString rowStr{ FormatCategoryLabel(m_rowAxis.m_categories[row]) };
                const wxString columnStr{ FormatCategoryLabel(m_columnAxis.m_categories[col]) };
                const wxString selectionLabel{
                    IsShowingPercentages() ?
                        wxString::Format(L"%s × %s: %s (%s)", rowStr, columnStr, cellValueStr,
                                         countStr) :
                        wxString::Format(L"%s × %s: %s", rowStr, columnStr, countStr)
                };
                // Keep scaling at 1 since this is set to a specific size on the plot.
                // A cell also keeps its fill color in an HTML dashboard's dark mode.
                AddObject(std::make_unique<GraphItems::Polygon>(
                    GraphItems::GraphItemInfo{ selectionLabel }
                        .Pen(GetPen())
                        .Brush(cellColor)
                        .Accessibility(wxSVGAttributes{}.Class(_DT(L"ink-keep heatmap-cell"))),
                    pts));

                if (count > 0)
                    {
                    AddObject(std::make_unique<GraphItems::Label>(
                        GraphItems::GraphItemInfo{ cellValueStr }
                            .Font(cellLabelFont)
                            .Pen(wxNullPen)
                            .Selectable(false)
                            .FontColor(Colors::ColorContrast::BlackOrWhiteContrast(cellColor))
                            .Anchoring(Anchoring::Center)
                            .AnchorPoint(wxPoint{ left + ((right - left) / 2),
                                                  top + ((bottom - top) / 2) })));
                    }
                }
            }

        // the totals, which are bold counts drawn without cells
        if (m_showTotals)
            {
            const wxFont totalFont{ cellLabelFont.Bold() };
            const auto boxStart = [boxSize](const wxCoord gridStart, const size_t index)
            { return gridStart + static_cast<wxCoord>(boxSize * index); };
            const auto addTotal =
                [this, &totalFont, &formatCount](const wxPoint& center, const size_t count)
            {
                AddObject(std::make_unique<GraphItems::Label>(
                    GraphItems::GraphItemInfo{ formatCount(count) }
                        .Font(totalFont)
                        .Pen(wxNullPen)
                        .Selectable(false)
                        .FontColor(GetLeftYAxis().GetFontColor())
                        .Anchoring(Anchoring::Center)
                        .AnchorPoint(center)));
            };

            const wxCoord halfBox{ static_cast<wxCoord>(boxSize / 2) };
            const wxCoord totalsColumnCenter{ gridRight + halfBox };
            const wxCoord totalsRowCenter{ gridBottom + halfBox };
            // each row's total, in the column to the right
            for (size_t row = 0; row < rowCount; ++row)
                {
                addTotal(wxPoint{ totalsColumnCenter, boxStart(gridTop, row) + halfBox },
                         m_rowTotals[row]);
                }
            // each column's total, in the row below
            for (size_t col = 0; col < columnCount; ++col)
                {
                addTotal(wxPoint{ boxStart(gridLeft, col) + halfBox, totalsRowCenter },
                         m_columnTotals[col]);
                }
            addTotal(wxPoint{ totalsColumnCenter, totalsRowCenter }, m_grandTotal);

            // short lines between each total and the next one
            const wxCoord quarterBox{ static_cast<wxCoord>(boxSize / 4) };
            auto separators = std::make_unique<GraphItems::Lines>(GetPen(), GetScaling());
            for (size_t row = 1; row <= rowCount; ++row)
                {
                const wxCoord lineY{ boxStart(gridTop, row) };
                separators->AddLine(wxPoint{ totalsColumnCenter - quarterBox, lineY },
                                    wxPoint{ totalsColumnCenter + quarterBox, lineY });
                }
            for (size_t col = 1; col <= columnCount; ++col)
                {
                const wxCoord lineX{ boxStart(gridLeft, col) };
                separators->AddLine(wxPoint{ lineX, totalsRowCenter - quarterBox },
                                    wxPoint{ lineX, totalsRowCenter + quarterBox });
                }
            AddObject(std::move(separators));
            }

        // the row labels, right aligned against the grid
        for (size_t row = 0; row < displayRowCount; ++row)
            {
            auto rowLabel = std::make_unique<GraphItems::Label>(
                GraphItems::GraphItemInfo{ rowLabels[row] }
                    // Font is already scaled, so leave its scaling at 1.
                    // The totals' label is bold, like the totals.
                    .Font((row < rowCount) ? rowLabelFont : rowLabelFont.Bold())
                    .FontColor(GetLeftYAxis().GetFontColor())
                    .Pen(wxNullPen)
                    .Anchoring(Anchoring::TopLeftCorner)
                    .AnchorPoint(wxPoint{ gridLeft - rowLabelSpace,
                                          gridTop + static_cast<wxCoord>(boxSize * row) })
                    .Padding(0, LABEL_PADDING, 0, 0)
                    .LabelPageHorizontalAlignment(PageHorizontalAlignment::RightAligned)
                    .LabelPageVerticalAlignment(PageVerticalAlignment::Centered));
            rowLabel->SetMinimumUserSizeDIPs(toLabelDIPs(rowLabelSpace), toLabelDIPs(boxSize));
            AddObject(std::move(rowLabel));
            }

        // the column labels, drawn vertically and starting against the grid
        for (size_t col = 0; col < displayColumnCount; ++col)
            {
            auto columnLabel = std::make_unique<GraphItems::Label>(
                GraphItems::GraphItemInfo{ columnLabels[col] }
                    .Font((col < columnCount) ? columnLabelFont : columnLabelFont.Bold())
                    .FontColor(GetBottomXAxis().GetFontColor())
                    .Pen(wxNullPen)
                    // measured below before being added, so it needs its DPI scaling now
                    .DPIScaling(GetDPIScaleFactor())
                    .Orient(Orientation::Vertical)
                    .Anchoring(Anchoring::TopLeftCorner)
                    .AnchorPoint(wxPoint{ gridLeft + static_cast<wxCoord>(boxSize * col),
                                          gridTop - padding }));
            // center the label horizontally over its column
            const wxCoord labelWidth{ columnLabel->GetBoundingBox(dc).GetWidth() };
            columnLabel->Offset((static_cast<wxCoord>(boxSize) - labelWidth) / 2, 0);
            AddObject(std::move(columnLabel));
            }

        // the group labels, the brackets spanning each group,
        // and the lines separating the groups
        // lines match their group labels' colors
        const wxPen rowSeparatorPen{ GetLeftYAxis().GetFontColor(), 1 };
        const wxPen rowBracketPen{ GetLeftYAxis().GetFontColor(), 2 };
        const wxPen columnSeparatorPen{ GetBottomXAxis().GetFontColor(), 1 };
        const wxPen columnBracketPen{ GetBottomXAxis().GetFontColor(), 2 };
        for (const auto& run : rowGroupRuns)
            {
            const wxCoord runTop{ gridTop + static_cast<wxCoord>(boxSize * run.m_start) };
            const wxCoord runBottom{ gridTop +
                                     static_cast<wxCoord>(boxSize * (run.m_start + run.m_length)) };
            auto groupLabel = std::make_unique<GraphItems::Label>(
                GraphItems::GraphItemInfo{ run.m_label }
                    .Font(rowGroupFont)
                    .FontColor(GetLeftYAxis().GetFontColor())
                    .Pen(wxNullPen)
                    .Anchoring(Anchoring::TopLeftCorner)
                    .AnchorPoint(wxPoint{ contentLeft, runTop })
                    .Padding(0, LABEL_PADDING, 0, 0)
                    .LabelPageHorizontalAlignment(PageHorizontalAlignment::RightAligned)
                    .LabelPageVerticalAlignment(PageVerticalAlignment::Centered));
            groupLabel->SetMinimumUserSizeDIPs(toLabelDIPs(rowGroupSpace - bracketGutter),
                                               toLabelDIPs(boxSize * run.m_length));
            AddObject(std::move(groupLabel));

            const wxCoord bracketX{ contentLeft + rowGroupSpace - (bracketGutter / 2) };
            auto bracket = std::make_unique<GraphItems::Lines>(rowBracketPen, GetScaling());
            bracket->AddLine(wxPoint{ bracketX, runTop + padding },
                             wxPoint{ bracketX, runBottom - padding });
            AddObject(std::move(bracket));

            if (run.m_start > 0)
                {
                auto separator = std::make_unique<GraphItems::Lines>(rowSeparatorPen, GetScaling());
                separator->AddLine(wxPoint{ contentLeft, runTop }, wxPoint{ outerRight, runTop });
                AddObject(std::move(separator));
                }
            }
        for (const auto& run : columnGroupRuns)
            {
            const wxCoord runLeft{ gridLeft + static_cast<wxCoord>(boxSize * run.m_start) };
            const wxCoord runRight{ gridLeft +
                                    static_cast<wxCoord>(boxSize * (run.m_start + run.m_length)) };
            auto groupLabel = std::make_unique<GraphItems::Label>(
                GraphItems::GraphItemInfo{ run.m_label }
                    .Font(columnGroupFont)
                    .FontColor(GetBottomXAxis().GetFontColor())
                    .Pen(wxNullPen)
                    .Anchoring(Anchoring::TopLeftCorner)
                    .AnchorPoint(wxPoint{ runLeft, contentTop })
                    .Padding(0, 0, LABEL_PADDING, 0)
                    .LabelPageHorizontalAlignment(PageHorizontalAlignment::Centered)
                    .LabelPageVerticalAlignment(PageVerticalAlignment::BottomAligned));
            groupLabel->SetMinimumUserSizeDIPs(toLabelDIPs(boxSize * run.m_length),
                                               toLabelDIPs(columnGroupSpace - bracketGutter));
            AddObject(std::move(groupLabel));

            const wxCoord bracketY{ contentTop + columnGroupSpace - (bracketGutter / 2) };
            auto bracket = std::make_unique<GraphItems::Lines>(columnBracketPen, GetScaling());
            bracket->AddLine(wxPoint{ runLeft + padding, bracketY },
                             wxPoint{ runRight - padding, bracketY });
            AddObject(std::move(bracket));

            if (run.m_start > 0)
                {
                auto separator =
                    std::make_unique<GraphItems::Lines>(columnSeparatorPen, GetScaling());
                separator->AddLine(wxPoint{ runLeft, contentTop }, wxPoint{ runLeft, outerBottom });
                AddObject(std::move(separator));
                }
            }
        }

    //----------------------------------------------------------------
    std::unique_ptr<GraphItems::Label> MatrixHeatMap::CreateLegend(const LegendOptions& options)
        {
        SetLegendInfo(options);
        if (GetDataset() == nullptr || m_maxCount == 0)
            {
            return nullptr;
            }

        auto legend = std::make_unique<GraphItems::Label>(
            GraphItems::GraphItemInfo{
                // add spaces on the empty lines to work around SVG exporting
                // stripping out the blank lines
                wxString::Format(L"%s\n \n \n%s", FormatCellValue(m_maxCellValue),
                                 FormatCellValue(m_minCellValue)) }
                .Padding(0, 0, 0, GraphItems::Label::GetMinLegendWidthDIPs() * 1.5)
                .DPIScaling(GetDPIScaleFactor())
                .Anchoring(Anchoring::TopLeftCorner)
                .LabelAlignment(TextAlignment::FlushLeft)
                .FontColor(GetLeftYAxis().GetFontColor()));
        if (options.IsIncludingHeader())
            {
            const auto defaultHeader = [this]()
            {
                switch (m_cellValueType)
                    {
                case CrossTabValue::RowPercentage:
                    return wxString{ _(L"% of Row") };
                case CrossTabValue::ColumnPercentage:
                    return wxString{ _(L"% of Column") };
                case CrossTabValue::TotalPercentage:
                    return wxString{ _(L"% of Total") };
                case CrossTabValue::Count:
                    [[fallthrough]];
                default:
                    return wxString{ _(L"Count") };
                    }
            };
            const wxString headerText{ options.GetTitle().empty() ? defaultHeader() :
                                                                    options.GetTitle() };
            legend->SetText(wxString::Format(L"%s\n", headerText) + legend->GetText());
            legend->GetHeaderInfo()
                .Enable(true)
                .LabelAlignment(TextAlignment::FlushLeft)
                .FontColor(GetLeftYAxis().GetFontColor());
            }
        legend->GetLegendIcons().emplace_back(m_reversedColorSpectrum);

        AddReferenceLinesAndAreasToLegend(*legend);
        AdjustLegendSettings(*legend, options.GetPlacementHint());
        return legend;
        }

    //----------------------------------------------------------------
    void MatrixHeatMap::SetAutoAccessibilityAttributes()
        {
        if (GetDataset() == nullptr || m_counts.empty() || m_maxCount == 0)
            {
            return;
            }

        wxString label{ _(L"A matrix heatmap") };

        AddAccessibilityAttribute(label, GetTitle().GetText(), L": ");
        AddAccessibilityAttribute(label, GetSubtitle().GetText(), L", ");

        label += L". ";
        label += IsShowingPercentages() ?
                     wxString::Format(
                         /* TRANSLATORS: matrix heatmap accessibility. 1st %s is the variable
                            down the rows, 2nd %s is the variable across the columns. */
                         _(L"Percentages of %s by %s"), m_rowColumnName, m_columnColumnName) :
                     wxString::Format(
                         /* TRANSLATORS: matrix heatmap accessibility. 1st %s is the variable
                            down the rows, 2nd %s is the variable across the columns. */
                         _(L"Counts of %s by %s"), m_rowColumnName, m_columnColumnName);
        label += L". ";
        // counts are written without thousands separators
        const auto formatAccessibleValue = [this](const double value)
        {
            return IsShowingPercentages() ? FormatCellValue(value) :
                                            wxString{ std::to_wstring(static_cast<size_t>(value)) };
        };
        const wxString minStr{ formatAccessibleValue(m_minCellValue) };
        const wxString maxStr{ formatAccessibleValue(m_maxCellValue) };
        switch (m_cellValueType)
            {
        case CrossTabValue::RowPercentage:
            label += wxString::Format(
                /* TRANSLATORS: matrix heatmap accessibility: range of the percentages of each
                   row's total. 1st %s is the minimum, 2nd %s is the maximum. */
                _(L"Percentages of each row's total range from %s to %s"), minStr, maxStr);
            break;
        case CrossTabValue::ColumnPercentage:
            label += wxString::Format(
                /* TRANSLATORS: matrix heatmap accessibility: range of the percentages of each
                   column's total. 1st %s is the minimum, 2nd %s is the maximum. */
                _(L"Percentages of each column's total range from %s to %s"), minStr, maxStr);
            break;
        case CrossTabValue::TotalPercentage:
            label += wxString::Format(
                /* TRANSLATORS: matrix heatmap accessibility: range of the percentages of all
                   observations. 1st %s is the minimum, 2nd %s is the maximum. */
                _(L"Percentages of the total range from %s to %s"), minStr, maxStr);
            break;
        case CrossTabValue::Count:
            [[fallthrough]];
        default:
            label += wxString::Format(
                /* TRANSLATORS: matrix heatmap accessibility: count range.
                   1st %s is the minimum, 2nd %s is the maximum. */
                _(L"Counts range from %s to %s"), minStr, maxStr);
            break;
            }
        label += L": ";

        // only the combinations that occur are listed
        for (size_t row = 0; row < m_counts.size(); ++row)
            {
            for (size_t col = 0; col < m_counts[row].size(); ++col)
                {
                if (m_counts[row][col] == 0)
                    {
                    continue;
                    }
                const wxString rowStr{ FormatCategoryLabel(m_rowAxis.m_categories[row]) };
                const wxString columnStr{ FormatCategoryLabel(m_columnAxis.m_categories[col]) };
                label += wxString::Format(L"%s × %s: %s, ", rowStr, columnStr,
                                          formatAccessibleValue(m_cellValues[row][col]));
                }
            }
        if (label.ends_with(L", "))
            {
            label.RemoveLast(2);
            }

        if (m_showTotals)
            {
            label += L". ";
            label += wxString::Format(
                /* TRANSLATORS: matrix heatmap accessibility: number of observations counted. */
                _(L"Total: %s"), std::to_wstring(m_grandTotal));
            }

        AddAccessibilityAttribute(label, GetCaption().GetText(), L". ");

        if (!label.ends_with(L"."))
            {
            label += L".";
            }

        GetAutoAccessibilityAttributes() = wxSVGAttributes{}.Role(_DT(L"img")).AriaLabel(label);
        }
    } // namespace Wisteria::Graphs
