/** @addtogroup Graphics
    @brief Graphing classes.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef WISTERIA_KPI_CARD_H
#define WISTERIA_KPI_CARD_H

#include "graph2d.h"
#include <algorithm>
#include <optional>

namespace Wisteria::Graphs
    {
    /** @brief How a KPI card's number is shortened.
        @note These units count in thousands (K, M, and B). Some languages count large
            numbers in ten-thousands instead (e.g., 万 in Chinese and Japanese),
            so the abbreviations may not suit them.*/
    enum class KpiNumberAbbreviation
        {
        None,      /*!< Show the full number.*/
        Thousands, /*!< Divide by 1,000 and append "K" (e.g., @c 45.2K).*/
        Millions,  /*!< Divide by 1,000,000 and append "M".*/
        Billions,  /*!< Divide by 1,000,000,000 and append "B".*/
        Auto       /*!< Use the largest unit that leaves a value of at least 1.*/
        };

    // clang-format off
    /** @brief A card showing a single summary number (e.g., a total or mean) calculated
            from a column, above an optional body of text.
        @details The card is drawn as a GraphItems::Label. The number is the label's
            header line, and the label's text is the card's body.
            Call GetLabel() to edit the card's appearance, including the number's font
            and color through its header (`GetLabel().GetHeaderInfo()`).

        @par %Data:
         This card accepts a Data::Dataset with a continuous column to aggregate.
         To show the number for each value of a categorical column,
         build the card through a FilterableGraph.

        @par Missing Data:
         Missing values are ignored. If there are no valid values,
         then the missing-value text is shown (see SetMissingValueText()).

        @par Example:
        @code
         auto canvas = new Wisteria::Canvas(this);
         canvas->SetFixedObjectsGridSize(1, 1);

         auto visitData = std::make_shared<Data::Dataset>();
         visitData->ImportCSV(L"/home/daphne/patients/visits.csv",
             Data::ImportInfo().ContinuousColumns({ L"Visit Minutes" }));

         auto card = std::make_shared<KpiCard>(canvas);
         card->SetData(visitData, L"Visit Minutes", Data::ColumnAggregation::Mean);
         card->SetPrecision(1);
         card->SetSuffix(_(L" min"));
         card->GetLabel().SetText(_(L"Average visit length"));
         card->GetLabel().GetHeaderInfo().FontColor(
             Colors::ColorBrewer::GetColor(Colors::Color::ForestGreen));

         canvas->SetFixedObject(0, 0, card);
        @endcode*/
    // clang-format on

    class KpiCard final : public Graph2D
        {
        wxDECLARE_DYNAMIC_CLASS(KpiCard);
        KpiCard() = default;

      public:
        /** @brief Constructor.
            @param canvas The parent canvas to render on.*/
        explicit KpiCard(Canvas* canvas);

        /** @brief Sets the data and calculates the card's number.
            @param data The data to use.
            @param continuousColumnName The continuous column to aggregate.
            @param aggregate How to calculate the number.
            @throws std::runtime_error If the column can't be found, throws an exception.\n
                The exception's @c what() message is UTF-8 encoded, so pass it to
                @c wxString::FromUTF8() when formatting it for an error message.*/
        void SetData(const std::shared_ptr<const Data::Dataset>& data,
                     const wxString& continuousColumnName, Data::ColumnAggregation aggregate);

        /// @returns The column being aggregated.
        [[nodiscard]]
        const wxString& GetColumnName() const noexcept
            {
            return m_columnName;
            }

        /// @returns How the number is calculated.
        [[nodiscard]]
        Data::ColumnAggregation GetAggregate() const noexcept
            {
            return m_aggregate;
            }

        /// @returns The calculated number, or @c std::nullopt if there is no valid data.
        [[nodiscard]]
        std::optional<double> GetValue() const noexcept
            {
            return m_value;
            }

        /// @returns The calculated number, formatted for display.
        [[nodiscard]]
        wxString GetFormattedValue() const;

        /// @brief Gets/sets the label that the card is drawn as.
        /// @details Its text is the card's body, and its header is the number.
        /// @returns The label.
        [[nodiscard]]
        GraphItems::Label& GetLabel() noexcept
            {
            return m_label;
            }

        /// @private
        [[nodiscard]]
        const GraphItems::Label& GetLabel() const noexcept
            {
            return m_label;
            }

        /// @name Number Format Functions
        /// @brief Functions for how the number is displayed.
        /// @{

        /// @returns How the number is displayed.
        [[nodiscard]]
        NumberDisplay GetNumberDisplay() const noexcept
            {
            return m_numberDisplay;
            }

        /// @brief Sets how the number is displayed.
        /// @param display The display style.
        /// @note NumberDisplay::Percentage multiplies the value by 100.
        void SetNumberDisplay(const NumberDisplay display) noexcept { m_numberDisplay = display; }

        /// @returns The number of decimal places shown.
        [[nodiscard]]
        uint8_t GetPrecision() const noexcept
            {
            return m_precision;
            }

        /// @brief Sets the number of decimal places shown.
        /// @param precision The number of decimal places, up to MAX_PRECISION.
        void SetPrecision(const uint8_t precision) noexcept
            {
            m_precision = std::min(precision, MAX_PRECISION);
            }

        /// @brief The most decimal places that can be shown.
        constexpr static uint8_t MAX_PRECISION{ 10 };

        /// @returns How the number is shortened.
        [[nodiscard]]
        KpiNumberAbbreviation GetAbbreviation() const noexcept
            {
            return m_abbreviation;
            }

        /// @brief Sets how the number is shortened.
        /// @param abbreviation The abbreviation method.
        void SetAbbreviation(const KpiNumberAbbreviation abbreviation) noexcept
            {
            m_abbreviation = abbreviation;
            }

        /// @returns @c true if positive numbers are shown with a plus sign.
        [[nodiscard]]
        bool IsShowingPlusSign() const noexcept
            {
            return m_showPlusSign;
            }

        /// @brief Sets whether positive numbers are shown with a plus sign.
        /// @param show @c true to show a plus sign.
        void ShowPlusSign(const bool show) noexcept { m_showPlusSign = show; }

        /// @returns The text shown after the number.
        [[nodiscard]]
        const wxString& GetSuffix() const noexcept
            {
            return m_suffix;
            }

        /// @brief Sets the text shown after the number (e.g., " pts").
        /// @param suffix The suffix.
        void SetSuffix(const wxString& suffix) { m_suffix = suffix; }

        /// @returns The text shown if there is no valid data.
        [[nodiscard]]
        const wxString& GetMissingValueText() const noexcept
            {
            return m_missingValueText;
            }

        /// @brief Sets the text shown if there is no valid data.
        /// @param text The text to show.
        void SetMissingValueText(const wxString& text) { m_missingValueText = text; }

        /// @}

        /// @returns @c nullptr, as a KPI card has no legend.
        /// @param options This parameter is ignored.
        [[deprecated("A KPI card has no legend.")]]
        std::unique_ptr<GraphItems::Label>
        CreateLegend([[maybe_unused]] const LegendOptions& options) override
            {
            return nullptr;
            }

        /// @returns @c true if the label's bounding box scaling is locked.
        /// @sa GraphItems::Label::LockBoundingBoxScaling().
        [[nodiscard]]
        bool IsBoundingBoxScalingLocked() const noexcept final
            {
            return m_label.IsBoundingBoxScalingLocked();
            }

      private:
        void RecalcSizes(wxDC& dc) final;
        void SetBoundingBox(const wxRect& rect, wxDC& dc, double parentScaling) final;
        /// @brief Builds the card's label and fits it (and the card) to @p rect.
        /// @details If @p rect is empty, then the card is sized to its label's natural size.
        void LayoutCard(const wxRect& rect, wxDC& dc);

        wxString m_columnName;
        Data::ColumnAggregation m_aggregate{ Data::ColumnAggregation::Sum };
        std::optional<double> m_value;
        GraphItems::Label m_label;

        NumberDisplay m_numberDisplay{ NumberDisplay::Value };
        uint8_t m_precision{ 0 };
        KpiNumberAbbreviation m_abbreviation{ KpiNumberAbbreviation::None };
        bool m_showPlusSign{ false };
        wxString m_suffix;
        wxString m_missingValueText{ L"—" };

        // the canvas's scaling from the last call to SetBoundingBox(),
        // passed to the label when its bounding box scaling is locked
        double m_parentScaling{ 1.0 };
        };
    } // namespace Wisteria::Graphs

/** @}*/

#endif // WISTERIA_KPI_CARD_H
