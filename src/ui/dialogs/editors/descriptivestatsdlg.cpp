///////////////////////////////////////////////////////////////////////////////
// Name:        descriptivestatsdlg.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "descriptivestatsdlg.h"
#include "../variableselectdlg.h"
#include <utility>
#include <wx/valgen.h>

namespace Wisteria::UI
    {
    //-------------------------------------------
    DescriptiveStatsDlg::DescriptiveStatsDlg(const ReportBuilder* reportBuilder,
                                             const Analyses::DescriptiveStatistics& analysis,
                                             wxWindow* parent, const wxString& caption,
                                             const wxWindowID id, const wxPoint& pos,
                                             const wxSize& size, const long style)
        : DialogWithHelp(parent, id, caption, pos, size, style), m_reportBuilder(reportBuilder),
          m_analysis(std::make_unique<Analyses::DescriptiveStatistics>(analysis)),
          m_columns(analysis.GetColumns()), m_groupColumn(analysis.GetGroupColumn()),
          m_outputName(analysis.GetName()), m_includeN(analysis.m_includeN),
          m_includeMissing(analysis.m_includeMissing), m_includeMean(analysis.m_includeMean),
          m_includeStdDev(analysis.m_includeStdDev), m_includeMin(analysis.m_includeMin),
          m_includeQuartiles(analysis.m_includeQuartiles), m_includeMax(analysis.m_includeMax)
        {
        // a new analysis starts on the first dataset, but an existing analysis whose
        // dataset is gone starts with no dataset selected
        m_datasetIndex = analysis.GetSourceDatasetName().empty() ? 0 : wxNOT_FOUND;
        if (m_reportBuilder != nullptr)
            {
            for (const auto& [name, dataset] : m_reportBuilder->GetDatasets())
                {
                if (name.CmpNoCase(analysis.GetSourceDatasetName()) == 0)
                    {
                    m_datasetIndex = static_cast<int>(m_datasetNames.size());
                    }
                m_datasetNames.push_back(name);
                }
            }

        CreateControls();
        if (const auto dataset = GetSelectedDataset(); dataset != nullptr)
            {
            // drop the columns that are no longer continuous columns in the dataset
            std::erase_if(m_columns,
                          [&dataset](const auto& column)
                          {
                              return dataset->GetContinuousColumn(column) ==
                                     dataset->GetContinuousColumns().cend();
                          });
            if (!m_groupColumn.empty() && dataset->GetCategoricalColumn(m_groupColumn) ==
                                              dataset->GetCategoricalColumns().cend())
                {
                m_groupColumn.clear();
                }
            }
        else
            {
            m_groupColumn.clear();
            }
        UpdateColumnsLabel();

        TransferDataToWindow();

        SetMinSize(FromDIP(wxSize{ 450, 400 }));
        Fit();
        Centre();
        }

    //-------------------------------------------
    void DescriptiveStatsDlg::CreateControls()
        {
        auto* mainSizer = new wxBoxSizer(wxVERTICAL);

        // dataset selector
        auto* choicesSizer = new wxFlexGridSizer(
            2, wxSize{ wxSizerFlags::GetDefaultBorder() * 2, wxSizerFlags::GetDefaultBorder() });
        choicesSizer->AddGrowableCol(1, 1);

        choicesSizer->Add(new wxStaticText(this, wxID_ANY, _(L"Dataset:")),
                          wxSizerFlags{}.CenterVertical());
        m_datasetChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, 0, nullptr,
                                       0, wxGenericValidator(&m_datasetIndex));
        for (const auto& name : m_datasetNames)
            {
            m_datasetChoice->Append(name);
            }
        choicesSizer->Add(m_datasetChoice, wxSizerFlags{}.Expand());
        mainSizer->Add(choicesSizer, wxSizerFlags{}.Expand().Border());

        // column and group selection
        auto* colButton = new wxButton(this, wxID_ANY, _(L"Select Columns..."));
        mainSizer->Add(colButton, wxSizerFlags{}.Border(wxLEFT | wxRIGHT));
        auto* varsSizer = new wxFlexGridSizer(
            2, wxSize{ wxSizerFlags::GetDefaultBorder(), wxSizerFlags::GetDefaultBorder() });
        varsSizer->AddGrowableCol(1, 1);
        const auto addVarLabel = [this, varsSizer](const wxString& header)
        {
            auto* headerLabel = new wxStaticText(this, wxID_ANY, header);
            headerLabel->SetFont(headerLabel->GetFont().Bold());
            varsSizer->Add(headerLabel);
            auto* valueLabel = new wxStaticText(this, wxID_ANY, wxString{});
            valueLabel->SetForegroundColour(wxColour{ 0, 102, 204 });
            varsSizer->Add(valueLabel, wxSizerFlags{ 1 });
            return valueLabel;
        };
        m_columnsLabel = addVarLabel(_(L"Columns:"));
        m_groupLabel = addVarLabel(_(L"Group by:"));
        mainSizer->Add(varsSizer, wxSizerFlags{}.Expand().Border());

        // statistics to include
        auto* statsBox = new wxStaticBoxSizer(wxVERTICAL, this, _(L"Statistics"));
        auto* statsGrid = new wxGridSizer(
            2, wxSize{ wxSizerFlags::GetDefaultBorder() * 2, wxSizerFlags::GetDefaultBorder() });
        const auto addStatCheckBox = [statsBox, statsGrid](const wxString& label, bool* value)
        {
            statsGrid->Add(new wxCheckBox(statsBox->GetStaticBox(), wxID_ANY, label,
                                          wxDefaultPosition, wxDefaultSize, 0,
                                          wxGenericValidator(value)));
        };
        addStatCheckBox(_(L"N (valid observations)"), &m_includeN);
        addStatCheckBox(_(L"Missing"), &m_includeMissing);
        addStatCheckBox(_(L"Mean"), &m_includeMean);
        addStatCheckBox(_(L"Standard deviation"), &m_includeStdDev);
        addStatCheckBox(_(L"Minimum"), &m_includeMin);
        addStatCheckBox(_(L"Quartiles (Q1, median, Q3)"), &m_includeQuartiles);
        addStatCheckBox(_(L"Maximum"), &m_includeMax);
        statsBox->Add(statsGrid, wxSizerFlags{}.Expand().Border());
        mainSizer->Add(statsBox, wxSizerFlags{}.Expand().Border());

        // output name
        auto* nameSizer = new wxFlexGridSizer(
            2, wxSize{ wxSizerFlags::GetDefaultBorder() * 2, wxSizerFlags::GetDefaultBorder() });
        nameSizer->AddGrowableCol(1, 1);
        nameSizer->Add(new wxStaticText(this, wxID_ANY, _(L"Output name:")),
                       wxSizerFlags{}.CenterVertical());
        nameSizer->Add(new wxTextCtrl(this, wxID_ANY, wxString{}, wxDefaultPosition, wxDefaultSize,
                                      0, wxGenericValidator(&m_outputName)),
                       wxSizerFlags{}.Expand());
        mainSizer->Add(nameSizer, wxSizerFlags{}.Expand().Border());

        mainSizer->AddStretchSpacer();
        mainSizer->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL | wxHELP),
                       wxSizerFlags{}.Expand().Border());

        SetSizer(mainSizer);

        m_datasetChoice->Bind(wxEVT_CHOICE,
                              [this]([[maybe_unused]] wxCommandEvent&) { OnDatasetChanged(); });
        colButton->Bind(wxEVT_BUTTON,
                        [this]([[maybe_unused]] wxCommandEvent&) { OnSelectColumns(); });
        }

    //-------------------------------------------
    std::shared_ptr<const Data::Dataset> DescriptiveStatsDlg::GetSelectedDataset() const
        {
        if (m_reportBuilder == nullptr || m_datasetIndex < 0 ||
            std::cmp_greater_equal(m_datasetIndex, m_datasetNames.size()))
            {
            return nullptr;
            }
        const auto& datasets = m_reportBuilder->GetDatasets();
        const auto foundDataset = datasets.find(m_datasetNames[m_datasetIndex]);
        return (foundDataset != datasets.cend()) ? foundDataset->second : nullptr;
        }

    //-------------------------------------------
    void DescriptiveStatsDlg::OnDatasetChanged()
        {
        m_datasetChoice->GetValidator()->TransferFromWindow();
        m_columns.clear();
        m_groupColumn.clear();
        UpdateColumnsLabel();
        }

    //-------------------------------------------
    void DescriptiveStatsDlg::OnSelectColumns()
        {
        const auto dataset = GetSelectedDataset();
        if (dataset == nullptr)
            {
            wxMessageBox(_(L"Please select a dataset first."), _(L"No Dataset"),
                         wxOK | wxICON_INFORMATION, this);
            return;
            }

        Data::Dataset::ColumnPreviewInfo columnInfo;
        for (const auto& column : dataset->GetContinuousColumns())
            {
            columnInfo.emplace_back(column.GetName(), Data::Dataset::ColumnImportType::Numeric,
                                    wxString{});
            }
        for (const auto& column : dataset->GetCategoricalColumns())
            {
            columnInfo.emplace_back(column.GetName(), Data::Dataset::ColumnImportType::String,
                                    wxString{});
            }

        using VLI = VariableSelectDlg::VariableListInfo;
        VariableSelectDlg dlg(
            this, columnInfo,
            { VLI{}
                  .Label(_(L"Columns"))
                  .Required(true)
                  .AcceptedTypes({ Data::Dataset::ColumnImportType::Numeric })
                  .DefaultVariables(m_columns),
              VLI{}
                  .Label(_(L"Group by"))
                  .SingleSelection(true)
                  .Required(false)
                  .AcceptedTypes({ Data::Dataset::ColumnImportType::String })
                  .DefaultVariables(m_groupColumn.empty() ?
                                        std::vector<wxString>{} :
                                        std::vector<wxString>{ m_groupColumn }) });
        if (dlg.ShowModal() != wxID_OK)
            {
            return;
            }
        m_columns = dlg.GetSelectedVariables(0);
        const auto groupVars = dlg.GetSelectedVariables(1);
        m_groupColumn = groupVars.empty() ? wxString{} : groupVars.front();
        UpdateColumnsLabel();
        }

    //-------------------------------------------
    void DescriptiveStatsDlg::UpdateColumnsLabel()
        {
        wxString columnsStr;
        for (const auto& column : m_columns)
            {
            if (!columnsStr.empty())
                {
                columnsStr += L", ";
                }
            columnsStr += column;
            }
        m_columnsLabel->SetLabel(columnsStr);
        m_columnsLabel->Wrap(FromDIP(400));
        m_groupLabel->SetLabel(m_groupColumn.empty() ? _(L"(none)") : m_groupColumn);
        Layout();
        }

    //-------------------------------------------
    bool DescriptiveStatsDlg::TransferDataFromWindow()
        {
        if (!DialogWithHelp::TransferDataFromWindow())
            {
            return false;
            }

        const auto dataset = GetSelectedDataset();
        if (dataset == nullptr)
            {
            wxMessageBox(_(L"Please select a dataset."), _(L"No Dataset"), wxOK | wxICON_WARNING,
                         this);
            return false;
            }
        if (m_columns.empty())
            {
            wxMessageBox(_(L"Please select at least one column."), _(L"No Columns"),
                         wxOK | wxICON_WARNING, this);
            return false;
            }
        for (const auto& column : m_columns)
            {
            if (dataset->GetContinuousColumn(column) == dataset->GetContinuousColumns().cend())
                {
                wxMessageBox(
                    wxString::Format(_(L"'%s': column not found in the selected dataset."), column),
                    _(L"Column Not Found"), wxOK | wxICON_WARNING, this);
                return false;
                }
            }
        if (!(m_includeN || m_includeMissing || m_includeMean || m_includeStdDev || m_includeMin ||
              m_includeQuartiles || m_includeMax))
            {
            wxMessageBox(_(L"Please select at least one statistic."), _(L"No Statistics"),
                         wxOK | wxICON_WARNING, this);
            return false;
            }

        m_outputName.Trim(true).Trim(false);
        if (m_outputName.empty())
            {
            m_outputName =
                wxString::Format(_(L"%s (Descriptive Statistics)"), m_datasetNames[m_datasetIndex]);
            }
        return true;
        }

    //-------------------------------------------
    std::unique_ptr<Analyses::DescriptiveStatistics> DescriptiveStatsDlg::GetAnalysis() const
        {
        auto analysis = std::make_unique<Analyses::DescriptiveStatistics>(*m_analysis);
        analysis->SetName(m_outputName);
        if (m_datasetIndex >= 0 && std::cmp_less(m_datasetIndex, m_datasetNames.size()))
            {
            analysis->SetSourceDatasetName(m_datasetNames[m_datasetIndex]);
            }
        analysis->SetColumns(m_columns);
        analysis->SetGroupColumn(m_groupColumn);
        analysis->m_includeN = m_includeN;
        analysis->m_includeMissing = m_includeMissing;
        analysis->m_includeMean = m_includeMean;
        analysis->m_includeStdDev = m_includeStdDev;
        analysis->m_includeMin = m_includeMin;
        analysis->m_includeQuartiles = m_includeQuartiles;
        analysis->m_includeMax = m_includeMax;
        return analysis;
        }
    } // namespace Wisteria::UI
