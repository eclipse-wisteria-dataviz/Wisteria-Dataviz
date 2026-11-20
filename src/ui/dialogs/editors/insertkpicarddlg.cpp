///////////////////////////////////////////////////////////////////////////////
// Name:        insertkpicarddlg.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "insertkpicarddlg.h"
#include "../../app/wisteriaview.h"
#include "../variableselectdlg.h"
#include <algorithm>
#include <tuple>
#include <wx/valgen.h>

namespace Wisteria::UI
    {
    //-------------------------------------------
    InsertKpiCardDlg::InsertKpiCardDlg(Canvas* canvas, const ReportBuilder* reportBuilder,
                                       wxWindow* parent, const wxString& caption,
                                       const wxWindowID id, const wxPoint& pos, const wxSize& size,
                                       const long style, EditMode editMode)
        : InsertGraphDlg(canvas, reportBuilder, parent, caption, id, pos, size, style, editMode,
                         GraphDlgIncludeNone)
        {
        CreateControls();
        FinalizeControls();

        SetMinSize(GetSize());

        Centre();
        }

    //-------------------------------------------
    std::vector<DataGuideParameter> InsertKpiCardDlg::GetParameters()
        {
        return { { .m_name = _(L"Value"),
                   .m_types = { Data::Dataset::ColumnImportType::Numeric },
                   .m_requirement = DataGuideRequirement::Required,
                   .m_description = _(L"The values that are summarized into the card's number."),
                   .m_values = {},
                   .m_multiple = false,
                   .m_condition = wxString{},
                   .m_group = wxString{} } };
        }

    //-------------------------------------------
    DataGuide InsertKpiCardDlg::GetDataGuide() const
        {
        DataGuide guide;
        guide.m_title = _(L"KPI Card");
        guide.m_iconName = L"images/kpi-card.svg";
        guide.m_summary =
            _(L"Shows a single number, such as a total or an average, above a short description.");
        guide.m_requirements = _(L"A numeric column. Its values are summarized into one number.");
        guide.m_shape = DataGuideShape::Long;
        guide.m_parameters = GetParameters();

        DataGuideExample sales;
        sales.m_title = _(L"Example: total revenue");
        sales.m_columns = { { _(L"Revenue"), _(L"Value") } };
        sales.m_rows = { { L"12500" }, { L"18200" }, { L"14500" } };
        guide.m_examples = { std::move(sales) };

        guide.m_missingData = { _(L"Missing values are ignored. If there are no values, then "
                                  "the missing-value text is shown.") };
        return guide;
        }

    //-------------------------------------------
    void InsertKpiCardDlg::CreateControls()
        {
        InsertGraphDlg::CreateControls();

        CreateKpiPage();
        CreateNumberFormatPage();
        CreatePageOptionsPage();
        }

    //-------------------------------------------
    void InsertKpiCardDlg::CreateKpiPage()
        {
        auto* kpiPage = new wxPanel(GetSideBarBook());
        auto* kpiSizer = new wxBoxSizer(wxVERTICAL);
        kpiPage->SetSizer(kpiSizer);
        GetSideBarBook()->AddPage(kpiPage, _(L"KPI Card"), ID_KPI_SECTION, true);

        // the defaults of a new card
        const Graphs::KpiCard defaultCard{ GetCanvas() };
        const auto& defaultLabel = defaultCard.GetLabel();

        // order must match TextAlignment
        const wxArrayString alignmentNames{ _(L"Left"), _(L"Right"), _(L"Center"), _(L"Justified"),
                                            _(L"Justified (at word)") };

        // data
        auto* dataBox = new wxStaticBoxSizer(wxVERTICAL, kpiPage, _(L"Data"));
        auto* dataGrid = new wxFlexGridSizer(2, wxSizerFlags::GetDefaultBorder() * 2,
                                             wxSizerFlags::GetDefaultBorder());
        dataGrid->AddGrowableCol(1, 1);

        dataGrid->Add(new wxStaticText(dataBox->GetStaticBox(), wxID_ANY, _(L"Dataset:")),
                      wxSizerFlags{}.CenterVertical());
        auto* datasetChoice = CreateDatasetChoice(dataBox->GetStaticBox());
        dataGrid->Add(datasetChoice, wxSizerFlags{}.CenterVertical());

        auto* varButton =
            new wxButton(dataBox->GetStaticBox(), ID_SELECT_VARS_BUTTON, _(L"Variables..."));
        dataGrid->Add(varButton, wxSizerFlags{}.CenterVertical());
        auto* varGrid = new wxFlexGridSizer(2, wxSize{ FromDIP(12), FromDIP(2) });
        auto* valueLabel = new wxStaticText(dataBox->GetStaticBox(), wxID_ANY, _(L"Value:"));
        valueLabel->SetFont(valueLabel->GetFont().Bold());
        varGrid->Add(valueLabel, wxSizerFlags{}.CenterVertical());
        m_valueVarLabel = new wxStaticText(dataBox->GetStaticBox(), wxID_ANY, wxString{});
        m_valueVarLabel->SetForegroundColour(Wisteria::Settings::GetHighlightedLabelColor());
        varGrid->Add(m_valueVarLabel, wxSizerFlags{}.CenterVertical());
        dataGrid->Add(varGrid, wxSizerFlags{}.CenterVertical());

        dataGrid->Add(new wxStaticText(dataBox->GetStaticBox(), wxID_ANY, _(L"Aggregate:")),
                      wxSizerFlags{}.CenterVertical());
        // the order of these entries is the numeric order of Data::ColumnAggregation
        auto* aggregateChoice =
            new wxChoice(dataBox->GetStaticBox(), wxID_ANY, wxDefaultPosition, wxDefaultSize, 0,
                         nullptr, 0, wxGenericValidator{ &m_aggregateIndex });
        for (const auto aggregation :
             { Data::ColumnAggregation::Sum, Data::ColumnAggregation::Mean,
               Data::ColumnAggregation::Min, Data::ColumnAggregation::Max,
               Data::ColumnAggregation::Count, Data::ColumnAggregation::Median })
            {
            aggregateChoice->Append(Data::GetColumnAggregationName(aggregation));
            }
        dataGrid->Add(aggregateChoice, wxSizerFlags{}.CenterVertical());

        dataGrid->Add(new wxStaticText(dataBox->GetStaticBox(), wxID_ANY, _(L"Value:")),
                      wxSizerFlags{}.CenterVertical());
        m_previewLabel = new wxStaticText(dataBox->GetStaticBox(), wxID_ANY, wxString{});
        m_previewLabel->SetFont(m_previewLabel->GetFont().Bold());
        dataGrid->Add(m_previewLabel, wxSizerFlags{}.CenterVertical());

        dataBox->Add(dataGrid, wxSizerFlags{}.Expand().Border());
        kpiSizer->Add(dataBox, wxSizerFlags{}.Expand().Border());

        // number (the label's header)
        const auto& defaultHeader = defaultLabel.GetHeaderInfo();
        auto* numberBox = new wxStaticBoxSizer(wxVERTICAL, kpiPage, _(L"Number"));
        auto* numberGrid = new wxFlexGridSizer(2, wxSizerFlags::GetDefaultBorder() * 2,
                                               wxSizerFlags::GetDefaultBorder());
        numberGrid->AddGrowableCol(1, 1);

        numberGrid->Add(new wxStaticText(numberBox->GetStaticBox(), wxID_ANY, _(L"Font:")),
                        wxSizerFlags{}.CenterVertical());
        m_numberFontPicker =
            new wxFontPickerCtrl(numberBox->GetStaticBox(), wxID_ANY, defaultHeader.GetFont());
        numberGrid->Add(m_numberFontPicker, wxSizerFlags{}.CenterVertical());

        numberGrid->Add(new wxStaticText(numberBox->GetStaticBox(), wxID_ANY, _(L"Size:")),
                        wxSizerFlags{}.CenterVertical());
        m_numberScalingSpin = new wxSpinCtrlDouble(
            numberBox->GetStaticBox(), wxID_ANY, wxString{}, wxDefaultPosition, wxDefaultSize,
            wxSP_ARROW_KEYS, 0.5, 10.0, defaultHeader.GetRelativeScaling(), 0.5);
        m_numberScalingSpin->SetDigits(1);
        numberGrid->Add(m_numberScalingSpin);

        numberGrid->Add(new wxStaticText(numberBox->GetStaticBox(), wxID_ANY, _(L"Color:")),
                        wxSizerFlags{}.CenterVertical());
        m_numberColorPicker = new wxColourPickerCtrl(numberBox->GetStaticBox(), wxID_ANY,
                                                     defaultHeader.GetFontColor());
        numberGrid->Add(m_numberColorPicker, wxSizerFlags{}.CenterVertical());

        numberGrid->Add(new wxStaticText(numberBox->GetStaticBox(), wxID_ANY, _(L"Alignment:")),
                        wxSizerFlags{}.CenterVertical());
        numberGrid->Add(new wxChoice(numberBox->GetStaticBox(), wxID_ANY, wxDefaultPosition,
                                     wxDefaultSize, alignmentNames, 0,
                                     wxGenericValidator{ &m_numberAlignment }),
                        wxSizerFlags{}.CenterVertical());

        numberBox->Add(numberGrid, wxSizerFlags{}.Expand().Border());
        kpiSizer->Add(numberBox, wxSizerFlags{}.Expand().Border());

        // body text
        auto* bodyBox = new wxStaticBoxSizer(wxVERTICAL, kpiPage, _(L"Body"));
        auto* bodyGrid = new wxFlexGridSizer(2, wxSizerFlags::GetDefaultBorder() * 2,
                                             wxSizerFlags::GetDefaultBorder());
        bodyGrid->AddGrowableCol(1, 1);

        bodyGrid->Add(new wxStaticText(bodyBox->GetStaticBox(), wxID_ANY, _(L"Text:")),
                      wxSizerFlags{}.Top());
        auto* bodyTextCtrl =
            new wxTextCtrl(bodyBox->GetStaticBox(), wxID_ANY, wxString{}, wxDefaultPosition,
                           wxSize{ FromDIP(300), FromDIP(80) }, wxTE_MULTILINE | wxTE_RICH2,
                           wxGenericValidator{ &m_bodyText });
#if wxUSE_SPELLCHECK
        bodyTextCtrl->EnableProofCheck(wxTextProofOptions::Default().GrammarCheck());
#endif
        bodyGrid->Add(bodyTextCtrl, wxSizerFlags{}.Expand());

        bodyGrid->Add(new wxStaticText(bodyBox->GetStaticBox(), wxID_ANY, _(L"Font:")),
                      wxSizerFlags{}.CenterVertical());
        m_bodyFontPicker =
            new wxFontPickerCtrl(bodyBox->GetStaticBox(), wxID_ANY, defaultLabel.GetFont());
        bodyGrid->Add(m_bodyFontPicker, wxSizerFlags{}.CenterVertical());

        bodyGrid->Add(new wxStaticText(bodyBox->GetStaticBox(), wxID_ANY, _(L"Color:")),
                      wxSizerFlags{}.CenterVertical());
        m_bodyColorPicker =
            new wxColourPickerCtrl(bodyBox->GetStaticBox(), wxID_ANY, defaultLabel.GetFontColor());
        bodyGrid->Add(m_bodyColorPicker, wxSizerFlags{}.CenterVertical());

        bodyGrid->Add(new wxStaticText(bodyBox->GetStaticBox(), wxID_ANY, _(L"Alignment:")),
                      wxSizerFlags{}.CenterVertical());
        bodyGrid->Add(new wxChoice(bodyBox->GetStaticBox(), wxID_ANY, wxDefaultPosition,
                                   wxDefaultSize, alignmentNames, 0,
                                   wxGenericValidator{ &m_bodyAlignment }),
                      wxSizerFlags{}.CenterVertical());

        bodyBox->Add(bodyGrid, wxSizerFlags{}.Expand().Border());
        kpiSizer->Add(bodyBox, wxSizerFlags{}.Expand().Border());

        // box
        auto* boxBox = new wxStaticBoxSizer(wxVERTICAL, kpiPage, _(L"Box"));
        auto* boxGrid = new wxFlexGridSizer(2, wxSizerFlags::GetDefaultBorder() * 2,
                                            wxSizerFlags::GetDefaultBorder());
        boxGrid->AddGrowableCol(1, 1);
        boxGrid->Add(new wxStaticText(boxBox->GetStaticBox(), wxID_ANY, _(L"Preset:")),
                     wxSizerFlags{}.CenterVertical());
        // order must match the presets in BuildKpiCard()
        const wxArrayString presetNames{ _(L"Standard"), _(L"Index Card") };
        boxGrid->Add(new wxChoice(boxBox->GetStaticBox(), wxID_ANY, wxDefaultPosition,
                                  wxDefaultSize, presetNames, 0,
                                  wxGenericValidator{ &m_layoutPreset }),
                     wxSizerFlags{}.CenterVertical());
        boxBox->Add(boxGrid, wxSizerFlags{}.Expand().Border());
        kpiSizer->Add(boxBox, wxSizerFlags{}.Expand().Border());

        // bind events
        datasetChoice->Bind(wxEVT_CHOICE,
                            [this]([[maybe_unused]] wxCommandEvent&) { OnDatasetChanged(); });
        varButton->Bind(wxEVT_BUTTON,
                        [this]([[maybe_unused]] wxCommandEvent&) { OnSelectVariables(); });
        aggregateChoice->Bind(wxEVT_CHOICE,
                              [this]([[maybe_unused]] wxCommandEvent&) { UpdatePreview(); });
        }

    //-------------------------------------------
    void InsertKpiCardDlg::CreateNumberFormatPage()
        {
        auto* formatPage = new wxPanel(GetSideBarBook());
        auto* formatSizer = new wxBoxSizer(wxVERTICAL);
        formatPage->SetSizer(formatSizer);
        GetSideBarBook()->AddPage(formatPage, _(L"Number Format"), ID_NUMBER_FORMAT_SECTION);

        auto* formatGrid = new wxFlexGridSizer(2, wxSizerFlags::GetDefaultBorder() * 2,
                                               wxSizerFlags::GetDefaultBorder());
        formatGrid->AddGrowableCol(1, 1);

        formatGrid->Add(new wxStaticText(formatPage, wxID_ANY, _(L"Style:")),
                        wxSizerFlags{}.CenterVertical());
        // order must match NumberDisplay
        const wxArrayString styleNames{ _(L"Number"), _(L"Percent"), _(L"Currency"),
                                        _(L"Number (no thousands separators)"), _(L"Accounting") };
        auto* styleChoice = new wxChoice(formatPage, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                         styleNames, 0, wxGenericValidator{ &m_numberStyleIndex });
        formatGrid->Add(styleChoice, wxSizerFlags{}.CenterVertical());

        formatGrid->Add(new wxStaticText(formatPage, wxID_ANY, _(L"Decimal places:")),
                        wxSizerFlags{}.CenterVertical());
        auto* precisionSpin =
            new wxSpinCtrl(formatPage, wxID_ANY, wxString{}, wxDefaultPosition, wxDefaultSize,
                           wxSP_ARROW_KEYS, 0, Graphs::KpiCard::MAX_PRECISION, m_precision);
        precisionSpin->SetValidator(wxGenericValidator{ &m_precision });
        formatGrid->Add(precisionSpin);

        formatGrid->Add(new wxStaticText(formatPage, wxID_ANY, _(L"Abbreviation:")),
                        wxSizerFlags{}.CenterVertical());
        // order must match KpiNumberAbbreviation
        const wxArrayString abbreviationNames{ _(L"None"), _(L"Thousands (K)"), _(L"Millions (M)"),
                                               _(L"Billions (B)"), _(L"Automatic") };
        auto* abbreviationChoice =
            new wxChoice(formatPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, abbreviationNames,
                         0, wxGenericValidator{ &m_abbreviationIndex });
        formatGrid->Add(abbreviationChoice, wxSizerFlags{}.CenterVertical());

        formatGrid->Add(new wxStaticText(formatPage, wxID_ANY, _(L"Suffix:")),
                        wxSizerFlags{}.CenterVertical());
        auto* suffixCtrl = new wxTextCtrl(formatPage, wxID_ANY, wxString{}, wxDefaultPosition,
                                          wxDefaultSize, 0, wxGenericValidator{ &m_suffix });
        formatGrid->Add(suffixCtrl, wxSizerFlags{}.Expand());

        formatGrid->Add(new wxStaticText(formatPage, wxID_ANY, _(L"Missing-value text:")),
                        wxSizerFlags{}.CenterVertical());
        auto* missingTextCtrl =
            new wxTextCtrl(formatPage, wxID_ANY, wxString{}, wxDefaultPosition, wxDefaultSize, 0,
                           wxGenericValidator{ &m_missingValueText });
        formatGrid->Add(missingTextCtrl, wxSizerFlags{}.Expand());

        formatSizer->Add(formatGrid, wxSizerFlags{}.Expand().Border());

        auto* plusSignCheck = new wxCheckBox(
            formatPage, wxID_ANY, _(L"Show plus sign on positive values"), wxDefaultPosition,
            wxDefaultSize, 0, wxGenericValidator{ &m_showPlusSign });
        formatSizer->Add(plusSignCheck, wxSizerFlags{}.Border());

        auto* previewSizer = new wxBoxSizer(wxHORIZONTAL);
        previewSizer->Add(new wxStaticText(formatPage, wxID_ANY, _(L"Preview:")),
                          wxSizerFlags{}.CenterVertical());
        m_formatPreviewLabel = new wxStaticText(formatPage, wxID_ANY, wxString{});
        m_formatPreviewLabel->SetFont(m_formatPreviewLabel->GetFont().Bold());
        previewSizer->Add(m_formatPreviewLabel, wxSizerFlags{}.CenterVertical().Border(wxLEFT));
        formatSizer->Add(previewSizer, wxSizerFlags{}.Border());

        // refresh the previews as the format changes
        const auto onChange = [this]([[maybe_unused]]
                                     wxCommandEvent&) { UpdatePreview(); };
        styleChoice->Bind(wxEVT_CHOICE, onChange);
        abbreviationChoice->Bind(wxEVT_CHOICE, onChange);
        precisionSpin->Bind(wxEVT_SPINCTRL, onChange);
        suffixCtrl->Bind(wxEVT_TEXT, onChange);
        missingTextCtrl->Bind(wxEVT_TEXT, onChange);
        plusSignCheck->Bind(wxEVT_CHECKBOX, onChange);
        }

    //-------------------------------------------
    void InsertKpiCardDlg::OnDatasetChanged()
        {
        m_valueVariable.clear();
        UpdateVariableLabels();
        }

    //-------------------------------------------
    void InsertKpiCardDlg::OnSelectVariables()
        {
        const auto dataset = GetSelectedDataset();
        if (dataset == nullptr)
            {
            wxMessageBox(_(L"Please select a dataset first."), _(L"No Dataset"),
                         wxOK | wxICON_INFORMATION, this);
            return;
            }

        Data::Dataset::ColumnPreviewInfo columnInfo;
        if (GetReportBuilder() != nullptr)
            {
            const auto& importOpts = GetReportBuilder()->GetDatasetImportOptions();
            const auto foundPos = importOpts.find(GetSelectedDatasetName());
            if (foundPos != importOpts.cend())
                {
                columnInfo = foundPos->second.m_columnPreviewInfo;
                }
            }
        if (columnInfo.empty())
            {
            columnInfo = BuildColumnPreviewInfo(*dataset);
            }

        const auto parameters = GetParameters();
        VariableSelectDlg dlg(
            this, columnInfo,
            { MakeVariableList(parameters[0], m_valueVariable.empty() ?
                                                  std::vector<wxString>{} :
                                                  std::vector<wxString>{ m_valueVariable }) });

        if (dlg.ShowModal() != wxID_OK)
            {
            return;
            }

        const auto valueVars = dlg.GetSelectedVariables(0);
        m_valueVariable = valueVars.empty() ? wxString{} : valueVars.front();

        UpdateVariableLabels();
        }

    //-------------------------------------------
    void InsertKpiCardDlg::UpdateVariableLabels()
        {
        m_valueVarLabel->SetLabel(m_valueVariable);
        UpdatePreview();

        GetSideBarBook()->GetCurrentPage()->Layout();
        }

    //-------------------------------------------
    void InsertKpiCardDlg::ApplyNumberFormat(Graphs::KpiCard& card) const
        {
        card.SetNumberDisplay(static_cast<NumberDisplay>(m_numberStyleIndex));
        card.SetPrecision(static_cast<uint8_t>(
            std::clamp(m_precision, 0, static_cast<int>(Graphs::KpiCard::MAX_PRECISION))));
        card.SetAbbreviation(static_cast<Graphs::KpiNumberAbbreviation>(m_abbreviationIndex));
        card.ShowPlusSign(m_showPlusSign);
        if (GetReportBuilder() != nullptr)
            {
            card.SetSuffix(GetReportBuilder()->ExpandConstants(m_suffix));
            card.SetMissingValueText(GetReportBuilder()->ExpandConstants(m_missingValueText));
            }
        else
            {
            card.SetSuffix(m_suffix);
            card.SetMissingValueText(m_missingValueText);
            }
        }

    //-------------------------------------------
    void InsertKpiCardDlg::UpdatePreview()
        {
        if (m_transferringToWindow || m_previewLabel == nullptr || m_formatPreviewLabel == nullptr)
            {
            return;
            }
        TransferDataFromWindow();

        wxString previewText;
        const auto dataset = GetSelectedDataset();
        if (dataset != nullptr && !m_valueVariable.empty())
            {
            try
                {
                Graphs::KpiCard previewCard{ GetCanvas() };
                ApplyNumberFormat(previewCard);
                previewCard.SetData(dataset, m_valueVariable,
                                    static_cast<Data::ColumnAggregation>(m_aggregateIndex));
                previewText = previewCard.GetFormattedValue();
                }
            catch (const std::exception& exc)
                {
                previewText = wxString::FromUTF8(exc.what());
                }
            }

        m_previewLabel->SetLabel(previewText);
        m_formatPreviewLabel->SetLabel(previewText);
        }

    //-------------------------------------------
    bool InsertKpiCardDlg::Validate()
        {
        if (GetSelectedDataset() == nullptr)
            {
            wxMessageBox(_(L"Please select a dataset."), _(L"No Dataset"), wxOK | wxICON_WARNING,
                         this);
            return false;
            }

        if (m_valueVariable.empty())
            {
            wxMessageBox(_(L"Please select the value variable."), _(L"Variable Not Specified"),
                         wxOK | wxICON_WARNING, this);
            OnSelectVariables();
            return false;
            }

        return true;
        }

    //-------------------------------------------
    bool InsertKpiCardDlg::TransferDataToWindow()
        {
        m_transferringToWindow = true;
        const bool result{ InsertGraphDlg::TransferDataToWindow() };
        m_transferringToWindow = false;

        UpdatePreview();
        return result;
        }

    //-------------------------------------------
    void InsertKpiCardDlg::LoadFromGraph(const Graphs::Graph2D& graph)
        {
        const auto* kpiCard = dynamic_cast<const Graphs::KpiCard*>(&graph);
        if (kpiCard == nullptr)
            {
            return;
            }

        LoadPageOptions(graph);

        SelectDataset(kpiCard->GetPropertyTemplate(L"dataset"));
        m_valueVariable = kpiCard->GetColumnName();
        m_aggregateIndex = static_cast<int>(kpiCard->GetAggregate());

        // the settings that this dialog doesn't have controls for come from here
        m_baseLabel = kpiCard->GetLabel();
        const auto& label = kpiCard->GetLabel();

        // number
        const auto& header = label.GetHeaderInfo();
        m_numberFontPicker->SetSelectedFont(header.GetFont());
        m_numberScalingSpin->SetValue(header.GetRelativeScaling());
        m_numberColorPicker->SetColour(header.GetFontColor());
        m_numberAlignment = static_cast<int>(header.GetLabelAlignment());

        // body text (prefer the template if it has {{constants}})
        const auto textTemplate = label.GetPropertyTemplate(L"text");
        m_bodyText = textTemplate.empty() ? label.GetText() : textTemplate;
        m_bodyFontPicker->SetSelectedFont(label.GetFont());
        m_bodyColorPicker->SetColour(label.GetFontColor());
        m_bodyAlignment = static_cast<int>(label.GetTextAlignment());

        m_layoutPreset = (label.GetLabelStyle() == LabelStyle::IndexCard) ? 1 : 0;

        // number format
        m_numberStyleIndex = static_cast<int>(kpiCard->GetNumberDisplay());
        m_precision = kpiCard->GetPrecision();
        m_abbreviationIndex = static_cast<int>(kpiCard->GetAbbreviation());
        m_showPlusSign = kpiCard->IsShowingPlusSign();
        const auto suffixTemplate = kpiCard->GetPropertyTemplate(L"number-format.suffix");
        m_suffix = suffixTemplate.empty() ? kpiCard->GetSuffix() : suffixTemplate;
        const auto missingTemplate =
            kpiCard->GetPropertyTemplate(L"number-format.missing-value-text");
        m_missingValueText =
            missingTemplate.empty() ? kpiCard->GetMissingValueText() : missingTemplate;

        TransferDataToWindow();
        UpdateVariableLabels();
        }

    //-------------------------------------------
    std::shared_ptr<Graphs::KpiCard> InsertKpiCardDlg::BuildKpiCard(const Graphs::Graph2D* oldGraph)
        {
        auto kpiCard = std::make_shared<Graphs::KpiCard>(GetCanvas());
        if (oldGraph != nullptr)
            {
            kpiCard->SetId(oldGraph->GetId());
            }
        ApplyPageOptions(*kpiCard);

        if (m_baseLabel.has_value())
            {
            kpiCard->GetLabel() = m_baseLabel.value();
            }
        auto& label = kpiCard->GetLabel();

        // number
        auto numberFont{ m_numberFontPicker->GetSelectedFont() };
        GraphItems::Label::FixFont(numberFont);
        label.GetHeaderInfo()
            .Enable(true)
            .Font(numberFont)
            .FontColor(m_numberColorPicker->GetColour())
            .LabelAlignment(static_cast<TextAlignment>(m_numberAlignment))
            .RelativeScaling(m_numberScalingSpin->GetValue());

        // body text
        auto bodyFont{ m_bodyFontPicker->GetSelectedFont() };
        GraphItems::Label::FixFont(bodyFont);
        label.GetFont() = bodyFont;
        label.SetFontColor(m_bodyColorPicker->GetColour());
        label.SetTextAlignment(static_cast<TextAlignment>(m_bodyAlignment));

        // layout preset (Standard or Index Card)
        const auto applyPreset = [&label](const int preset)
        {
            if (preset == 1)
                {
                // shrinks the width to the content
                label.SetLabelStyle(LabelStyle::IndexCard);
                label.SetBoundingBoxToContentAdjustment(
                    LabelBoundingBoxContentAdjustment::ContentAdjustWidth);
                }
            else
                {
                label.SetLabelStyle(LabelStyle::NoLabelStyle);
                label.SetBoundingBoxToContentAdjustment(
                    LabelBoundingBoxContentAdjustment::ContentAdjustNone);
                }
        };
        applyPreset(m_layoutPreset);

        // expand {{constants}}, caching the templates for round-tripping
        const auto expandAndCache =
            [this](GraphItems::GraphItemBase& item, const wxString& property, const wxString& raw)
        {
            const wxString expanded{ (GetReportBuilder() != nullptr) ?
                                         GetReportBuilder()->ExpandConstants(raw) :
                                         raw };
            item.SetPropertyTemplate(property, (expanded != raw) ? raw : wxString{});
            return expanded;
        };
        label.SetText(expandAndCache(label, L"text", m_bodyText));

        ApplyNumberFormat(*kpiCard);
        std::ignore = expandAndCache(*kpiCard, L"number-format.suffix", m_suffix);
        std::ignore =
            expandAndCache(*kpiCard, L"number-format.missing-value-text", m_missingValueText);

        kpiCard->SetData(GetSelectedDataset(), m_valueVariable,
                         static_cast<Data::ColumnAggregation>(m_aggregateIndex));

        if (oldGraph != nullptr)
            {
            // carry forward property templates, preserving {{placeholders}}
            const auto* oldCard = dynamic_cast<const Graphs::KpiCard*>(oldGraph);

            WisteriaView::CarryForwardProperty(*oldGraph, *kpiCard, L"dataset",
                                               GetSelectedDatasetName(),
                                               oldGraph->GetPropertyTemplate(L"dataset"));
            WisteriaView::CarryForwardProperty(
                *oldGraph, *kpiCard, L"variables.value", m_valueVariable,
                oldCard != nullptr ? oldCard->GetColumnName() : wxString{});
            }
        else
            {
            // cache dataset and variable names for round-tripping
            kpiCard->SetPropertyTemplate(L"dataset", GetSelectedDatasetName());
            kpiCard->SetPropertyTemplate(L"variables.value", m_valueVariable);
            }

        return kpiCard;
        }
    } // namespace Wisteria::UI
