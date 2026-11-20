///////////////////////////////////////////////////////////////////////////////
// Name:        kpicard.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "kpicard.h"
#include <cmath>
#include <limits>
#include <tuple>
#include <wx/numformatter.h>

wxIMPLEMENT_DYNAMIC_CLASS(Wisteria::Graphs::KpiCard, Wisteria::Graphs::Graph2D)

    namespace Wisteria::Graphs
    {
    //----------------------------------------------------------------
    KpiCard::KpiCard(Wisteria::Canvas * canvas) : Graph2D(canvas)
        {
        auto defaultFont{ wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT) };
        GraphItems::Label::FixFont(defaultFont);
        m_label.GetFont() = defaultFont;
        m_label.SetTextAlignment(TextAlignment::Centered);
        m_label.SetPageHorizontalAlignment(PageHorizontalAlignment::Centered);
        m_label.SetPageVerticalAlignment(PageVerticalAlignment::Centered);

        auto headerFont{ defaultFont };
        headerFont.MakeBold();
        m_label.GetHeaderInfo()
            .Enable(true)
            .Font(headerFont)
            .LabelAlignment(TextAlignment::Centered)
            .RelativeScaling(3.0);
        }

    //----------------------------------------------------------------
    void KpiCard::SetData(const std::shared_ptr<const Data::Dataset>& data,
                          const wxString& continuousColumnName,
                          const Data::ColumnAggregation aggregate)
        {
        if (data == nullptr)
            {
            throw std::runtime_error(_(L"A dataset is required for a KPI card.").ToUTF8());
            }

        SetDataset(data);
        // throws if the column isn't found
        std::ignore = GetContinuousColumn(continuousColumnName);
        m_columnName = continuousColumnName;
        m_aggregate = aggregate;
        m_value = std::nullopt;

        const auto validN{ data->GetContinuousColumnValidN(continuousColumnName) };
        if (aggregate == Data::ColumnAggregation::Count)
            {
            m_value = static_cast<double>(validN);
            return;
            }
        if (validN == 0)
            {
            return;
            }

        double result{ std::numeric_limits<double>::quiet_NaN() };
        switch (aggregate)
            {
        case Data::ColumnAggregation::Sum:
            result = data->GetContinuousTotal(continuousColumnName);
            break;
        case Data::ColumnAggregation::Mean:
            result = safe_divide<double>(data->GetContinuousTotal(continuousColumnName),
                                         static_cast<double>(validN));
            break;
        case Data::ColumnAggregation::Median:
            result = data->GetContinuousMedian(continuousColumnName);
            break;
        case Data::ColumnAggregation::Min:
            result = data->GetContinuousMinMax(continuousColumnName).first;
            break;
        case Data::ColumnAggregation::Max:
            result = data->GetContinuousMinMax(continuousColumnName).second;
            break;
        default:
            break;
            }

        if (std::isfinite(result))
            {
            m_value = result;
            }
        }

    //----------------------------------------------------------------
    wxString KpiCard::GetFormattedValue() const
        {
        if (!m_value.has_value() || !std::isfinite(m_value.value()))
            {
            return m_missingValueText;
            }

        double value{ m_value.value() };
        if (m_numberDisplay == NumberDisplay::Percentage)
            {
            value *= 100;
            }

        const double absValue{ std::abs(value) };
        // the unit is picked from the rounded value, so that 999,999 becomes 1.0M, not 1,000.0K
        const auto roundToPrecision = [this](const double val)
        {
            const double scale{ std::pow(10.0, m_precision) };
            return safe_divide(std::round(val * scale), scale);
        };
        const KpiNumberAbbreviation abbreviation{
            (m_abbreviation != KpiNumberAbbreviation::Auto)   ? m_abbreviation :
            (roundToPrecision(absValue / 1'000'000) >= 1'000) ? KpiNumberAbbreviation::Billions :
            (roundToPrecision(absValue / 1'000) >= 1'000)     ? KpiNumberAbbreviation::Millions :
            (roundToPrecision(absValue) >= 1'000)             ? KpiNumberAbbreviation::Thousands :
                                                                KpiNumberAbbreviation::None
        };
        wxString unitFormat{ L"%s" };
        double divisor{ 1 };
        switch (abbreviation)
            {
        case KpiNumberAbbreviation::Thousands:
            divisor = 1'000;
            /* TRANSLATORS: %s is a formatted number divided by 1,000, followed by
               the abbreviation for thousands (e.g., "45.2K"). Add a space or move
               the abbreviation as your language requires (e.g., "%s Tsd.").*/
            unitFormat = _(L"%sK");
            break;
        case KpiNumberAbbreviation::Millions:
            divisor = 1'000'000;
            /* TRANSLATORS: %s is a formatted number divided by 1,000,000, followed by
               the abbreviation for millions (e.g., "45.2M"). Add a space or move
               the abbreviation as your language requires (e.g., "%s Mio.").*/
            unitFormat = _(L"%sM");
            break;
        case KpiNumberAbbreviation::Billions:
            divisor = 1'000'000'000;
            /* TRANSLATORS: %s is a formatted number divided by 1,000,000,000, followed by
               the abbreviation for billions (e.g., "45.2B"). Add a space or move
               the abbreviation as your language requires (e.g., "%s Mrd.").*/
            unitFormat = _(L"%sB");
            break;
        default:
            break;
            }

        const bool isCurrency{ m_numberDisplay == NumberDisplay::Currency ||
                               m_numberDisplay == NumberDisplay::Accounting };
        int formatStyle{ (m_numberDisplay == NumberDisplay::ValueSimple) ?
                             wxNumberFormatter::Style::Style_None :
                             wxNumberFormatter::Style::Style_WithThousandsSep };
        if (isCurrency)
            {
            formatStyle |= wxNumberFormatter::Style::Style_Currency |
                           wxNumberFormatter::Style::Style_CurrencySymbol;
            }

        // the sign is added separately, so that it goes in front of the currency symbol
        const wxString formattedNumber{ wxNumberFormatter::ToString(safe_divide(absValue, divisor),
                                                                    m_precision, formatStyle) };
        // a value that rounds to zero is shown without a sign
        const bool isRoundedZero{ std::none_of(formattedNumber.begin(), formattedNumber.end(),
                                               [](const wxUniChar chr)
                                               { return chr >= L'1' && chr <= L'9'; }) };
        wxString numberText{ wxString::Format(unitFormat, formattedNumber) };
        if (m_numberDisplay == NumberDisplay::Percentage)
            {
            // TRANSLATORS: %s is a formatted number; %% is a literal percent sign
            numberText = wxString::Format(_(L"%s%%"), numberText);
            }

        if (!isRoundedZero)
            {
            if (value < 0)
                {
                numberText = (m_numberDisplay == NumberDisplay::Accounting) ?
                                 wxString::Format(L"(%s)", numberText) :
                                 wxString{ L"-" } + numberText;
                }
            else if (value > 0 && m_showPlusSign)
                {
                numberText.insert(0, L"+");
                }
            }

        return numberText + m_suffix;
        }

    //----------------------------------------------------------------
    void KpiCard::SetBoundingBox(const wxRect& rect, wxDC& dc, const double parentScaling)
        {
        m_parentScaling = parentScaling;
        LayoutCard(rect, dc);
        }

    //----------------------------------------------------------------
    void KpiCard::RecalcSizes(wxDC & dc)
        {
        // the card is only a label, so the graph's axes and titles are not laid out
        LayoutCard(GetBoundingBox(dc), dc);
        }

    //----------------------------------------------------------------
    void KpiCard::LayoutCard(const wxRect& rect, wxDC& dc)
        {
        ClearObjects();

        auto card{ std::make_unique<GraphItems::Label>(m_label) };
        const wxString& bodyText{ m_label.GetText() };
        card->SetText(bodyText.empty() ? GetFormattedValue() :
                                         GetFormattedValue() + L"\n" + bodyText);
        card->GetHeaderInfo().Enable(true);
        card->SetDPIScaleFactor(GetDPIScaleFactor());
        card->SetScaling(GetScaling());
        card->SetSelectable(IsSelectable());
        // without a box, the label is measured at its natural size
        if (!rect.IsEmpty())
            {
            card->SetBoundingBox(rect, dc, m_parentScaling);
            }

        // the card only uses the space that its label needs
        Graph2D::SetBoundingBox(card->GetBoundingBox(dc), dc, m_parentScaling);

        AddObject(std::move(card));
        }
    } // namespace Wisteria::Graphs
