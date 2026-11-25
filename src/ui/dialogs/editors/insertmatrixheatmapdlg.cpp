///////////////////////////////////////////////////////////////////////////////
// Name:        insertmatrixheatmapdlg.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "insertmatrixheatmapdlg.h"
#include "../../app/wisteriaview.h"
#include "../variableselectdlg.h"

namespace Wisteria::UI
    {
    //-------------------------------------------
    InsertMatrixHeatMapDlg::InsertMatrixHeatMapDlg(Canvas* canvas,
                                                   const ReportBuilder* reportBuilder,
                                                   wxWindow* parent, const wxString& caption,
                                                   const wxWindowID id, const wxPoint& pos,
                                                   const wxSize& size, const long style,
                                                   EditMode editMode)
        : InsertGraphDlg(canvas, reportBuilder, parent, caption, id, pos, size, style, editMode)
        {
        CreateControls();
        FinalizeControls();

        SetMinSize(GetSize());

        Centre();
        }

    //-------------------------------------------
    std::vector<DataGuideParameter> InsertMatrixHeatMapDlg::GetParameters()
        {
        const std::vector<Data::Dataset::ColumnImportType> axisTypes{
            Data::Dataset::ColumnImportType::String, Data::Dataset::ColumnImportType::Discrete,
            Data::Dataset::ColumnImportType::DichotomousString,
            Data::Dataset::ColumnImportType::DichotomousDiscrete,
            Data::Dataset::ColumnImportType::Numeric
        };
        const std::vector<Data::Dataset::ColumnImportType> groupTypes{
            Data::Dataset::ColumnImportType::String, Data::Dataset::ColumnImportType::Discrete,
            Data::Dataset::ColumnImportType::DichotomousString,
            Data::Dataset::ColumnImportType::DichotomousDiscrete
        };

        // the order here is the order of the lists in the variable selection dialog
        return { { .m_name = _(L"Row"),
                   .m_types = axisTypes,
                   .m_requirement = DataGuideRequirement::Required,
                   .m_description = _(L"The values shown down the rows, such as high schools."),
                   .m_values = {},
                   .m_multiple = false,
                   .m_condition = wxString{},
                   .m_group = wxString{} },
                 { .m_name = _(L"Column"),
                   .m_types = axisTypes,
                   .m_requirement = DataGuideRequirement::Required,
                   .m_description = _(L"The values shown across the columns, such as colleges."),
                   .m_values = {},
                   .m_multiple = false,
                   .m_condition = wxString{},
                   .m_group = wxString{} },
                 { .m_name = _(L"Row group"),
                   .m_types = groupTypes,
                   .m_requirement = DataGuideRequirement::Optional,
                   .m_description = _(L"Splits the rows into groups, such as public and private "
                                      "high schools."),
                   .m_values = {},
                   .m_multiple = false,
                   .m_condition = wxString{},
                   .m_group = wxString{} },
                 { .m_name = _(L"Column group"),
                   .m_types = groupTypes,
                   .m_requirement = DataGuideRequirement::Optional,
                   .m_description = _(L"Splits the columns into groups, such as public and "
                                      "private colleges."),
                   .m_values = {},
                   .m_multiple = false,
                   .m_condition = wxString{},
                   .m_group = wxString{} } };
        }

    //-------------------------------------------
    DataGuide InsertMatrixHeatMapDlg::GetDataGuide() const
        {
        DataGuide guide;
        guide.m_title = _(L"Matrix Heat Map");
        guide.m_iconName = L"images/matrix-heatmap.svg";
        guide.m_summary = _(L"Counts how often each pairing of two variables occurs and shows "
                            "the counts as a grid of colored cells, so that the most common "
                            "pairings stand out.");
        guide.m_requirements =
            _(L"One row per observation, holding its row and column values and, optionally, "
              "the groups that those values belong to. The order of the rows does not matter.");
        guide.m_shape = DataGuideShape::Long;
        guide.m_parameters = GetParameters();

        DataGuideExample students;
        students.m_title = _(L"Example: the colleges that students went to from high school");
        students.m_columns = { { _(L"High School"), _(L"Row") },
                               { _(L"HS Type"), _(L"Row group") },
                               { _(L"College"), _(L"Column") },
                               { _(L"College Type"), _(L"Column group") } };
        students.m_rows = {
            { L"Central", _(L"Public"), _DT(L"State U"), _(L"Public") },
            { L"Central", _(L"Public"), _DT(L"Ivy College"), _(L"Private") },
            { _DT(L"St. Agnes"), _(L"Private"), _DT(L"Ivy College"), _(L"Private") },
            { _DT(L"St. Agnes"), _(L"Private"), _DT(L"Ivy College"), _(L"Private") },
            { L"West", _(L"Public"), _DT(L"State U"), _(L"Public") }
        };
        guide.m_examples = { std::move(students) };

        guide.m_missingData = {
            _(L"An observation missing its row or column value is not counted. Its other value "
              "is still shown, as a row or column of empty cells."),
            _(L"An observation missing a group is placed into an unlabeled group, shown after "
              "the other groups.")
        };
        guide.m_notes = { _(
            L"A value is counted separately within each of its groups, so two schools with "
            "the same name (one public and one private) are shown as separate rows.") };
        return guide;
        }

    //-------------------------------------------
    void InsertMatrixHeatMapDlg::CreateControls()
        {
        InsertGraphDlg::CreateControls();

        auto* optionsPage = new wxPanel(GetSideBarBook());
        auto* optionsSizer = new wxBoxSizer(wxVERTICAL);
        optionsPage->SetSizer(optionsSizer);
        GetSideBarBook()->AddPage(optionsPage, _(L"Matrix Heat Map"), ID_OPTIONS_SECTION, true);

        // dataset selector
        auto* datasetSizer = new wxFlexGridSizer(
            2, wxSize{ wxSizerFlags::GetDefaultBorder() * 2, wxSizerFlags::GetDefaultBorder() });

        datasetSizer->Add(new wxStaticText(optionsPage, wxID_ANY, _(L"Dataset:")),
                          wxSizerFlags{}.CenterVertical());
        auto* datasetChoice = CreateDatasetChoice(optionsPage);
        datasetSizer->Add(datasetChoice);

        optionsSizer->Add(datasetSizer, wxSizerFlags{}.Border());

        // variables button
        auto* varsBox = new wxStaticBoxSizer(wxVERTICAL, optionsPage, _(L"Variables"));
        auto* varButton =
            new wxButton(varsBox->GetStaticBox(), ID_SELECT_VARS_BUTTON, _(L"Select..."));
        varsBox->Add(varButton, wxSizerFlags{}.Border(wxLEFT));

        // variable label grid
        auto* varGrid = new wxFlexGridSizer(2, wxSize{ FromDIP(12), FromDIP(2) });
        const auto addVariableLabel = [&varGrid, &varsBox](const wxString& caption)
        {
            auto* captionLabel = new wxStaticText(varsBox->GetStaticBox(), wxID_ANY, caption);
            captionLabel->SetFont(captionLabel->GetFont().Bold());
            varGrid->Add(captionLabel, wxSizerFlags{}.CenterVertical());
            auto* valueLabel = new wxStaticText(varsBox->GetStaticBox(), wxID_ANY, wxString{});
            valueLabel->SetForegroundColour(Wisteria::Settings::GetHighlightedLabelColor());
            varGrid->Add(valueLabel, wxSizerFlags{}.CenterVertical());
            return valueLabel;
        };
        m_rowVarLabel = addVariableLabel(_(L"Row:"));
        m_columnVarLabel = addVariableLabel(_(L"Column:"));
        m_rowGroupVarLabel = addVariableLabel(_(L"Row group:"));
        m_columnGroupVarLabel = addVariableLabel(_(L"Column group:"));

        varsBox->Add(varGrid, wxSizerFlags{}.Border());
        optionsSizer->Add(varsBox, wxSizerFlags{}.Border());

        // cell values (in the same order as CrossTabValue)
        auto* cellValuesSizer = new wxFlexGridSizer(
            2, wxSize{ wxSizerFlags::GetDefaultBorder() * 2, wxSizerFlags::GetDefaultBorder() });
        cellValuesSizer->Add(new wxStaticText(optionsPage, wxID_ANY, _(L"Cell values:")),
                             wxSizerFlags{}.CenterVertical());
        const wxArrayString cellValueChoices{ _(L"Counts"), _(L"Percent of each row"),
                                              _(L"Percent of each column"),
                                              _(L"Percent of total") };
        cellValuesSizer->Add(new wxChoice(optionsPage, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          cellValueChoices, 0,
                                          wxGenericValidator{ &m_cellValuesIndex }),
                             wxSizerFlags{}.CenterVertical());
        optionsSizer->Add(cellValuesSizer, wxSizerFlags{}.Border());

        optionsSizer->Add(new wxCheckBox(optionsPage, wxID_ANY, _(L"Show totals"),
                                         wxDefaultPosition, wxDefaultSize, 0,
                                         wxGenericValidator{ &m_showTotals }),
                          wxSizerFlags{}.Border());

        // bind events
        datasetChoice->Bind(wxEVT_CHOICE,
                            [this]([[maybe_unused]] wxCommandEvent&) { OnDatasetChanged(); });

        varButton->Bind(wxEVT_BUTTON,
                        [this]([[maybe_unused]] wxCommandEvent&) { OnSelectVariables(); });

        CreateLegendOptionsPage();
        CreateGraphOptionsPage();
        CreatePageOptionsPage();
        }

    //-------------------------------------------
    void InsertMatrixHeatMapDlg::OnDatasetChanged()
        {
        m_rowVariable.clear();
        m_columnVariable.clear();
        m_rowGroupVariable.clear();
        m_columnGroupVariable.clear();
        UpdateVariableLabels();
        }

    //-------------------------------------------
    void InsertMatrixHeatMapDlg::OnSelectVariables()
        {
        const auto dataset = GetSelectedDataset();
        if (dataset == nullptr)
            {
            wxMessageBox(_(L"Please select a dataset first."), _(L"No Dataset"),
                         wxOK | wxICON_INFORMATION, this);
            return;
            }

        // prefer the stored column preview info (preserves original file order)
        Data::Dataset::ColumnPreviewInfo columnInfo;
        if (GetReportBuilder() != nullptr)
            {
            const auto& importOpts = GetReportBuilder()->GetDatasetImportOptions();
            const auto it = importOpts.find(GetSelectedDatasetName());
            if (it != importOpts.cend())
                {
                columnInfo = it->second.m_columnPreviewInfo;
                }
            }
        if (columnInfo.empty())
            {
            columnInfo = BuildColumnPreviewInfo(*dataset);
            }

        // expand any constant placeholders so the variable selection list
        // can match the real dataset columns
        const auto asDefaults = [this](const wxString& variable)
        {
            const auto expanded = ExpandVariable(variable);
            return expanded.empty() ? std::vector<wxString>{} : std::vector<wxString>{ expanded };
        };

        const auto parameters = GetParameters();
        VariableSelectDlg dlg(
            this, columnInfo,
            { MakeVariableList(parameters[0], asDefaults(m_rowVariable)),
              MakeVariableList(parameters[1], asDefaults(m_columnVariable)),
              MakeVariableList(parameters[2], asDefaults(m_rowGroupVariable)),
              MakeVariableList(parameters[3], asDefaults(m_columnGroupVariable)) });

        if (dlg.ShowModal() != wxID_OK)
            {
            return;
            }

        const auto firstSelected = [&dlg](const size_t list)
        {
            const auto vars = dlg.GetSelectedVariables(list);
            return vars.empty() ? wxString{} : vars.front();
        };
        m_rowVariable = firstSelected(0);
        m_columnVariable = firstSelected(1);
        m_rowGroupVariable = firstSelected(2);
        m_columnGroupVariable = firstSelected(3);

        UpdateVariableLabels();
        }

    //-------------------------------------------
    void InsertMatrixHeatMapDlg::UpdateVariableLabels()
        {
        m_rowVarLabel->SetLabel(m_rowVariable);
        m_columnVarLabel->SetLabel(m_columnVariable);
        m_rowGroupVarLabel->SetLabel(m_rowGroupVariable);
        m_columnGroupVarLabel->SetLabel(m_columnGroupVariable);

        GetSideBarBook()->GetCurrentPage()->Layout();
        }

    //-------------------------------------------
    bool InsertMatrixHeatMapDlg::Validate()
        {
        if (GetSelectedDataset() == nullptr)
            {
            wxMessageBox(_(L"Please select a dataset."), _(L"No Dataset"), wxOK | wxICON_WARNING,
                         this);
            return false;
            }

        if (m_rowVariable.empty() || m_columnVariable.empty())
            {
            wxMessageBox(_(L"Please select a row and a column variable."),
                         _(L"Variable Not Specified"), wxOK | wxICON_WARNING, this);
            OnSelectVariables();
            return false;
            }

        if (!ValidateColorScheme())
            {
            return false;
            }

        return true;
        }

    //-------------------------------------------
    void InsertMatrixHeatMapDlg::LoadFromGraph(const Graphs::Graph2D& graph)
        {
        const auto* heatmap = dynamic_cast<const Graphs::MatrixHeatMap*>(&graph);
        if (heatmap == nullptr)
            {
            return;
            }

        // load graph and page options from the base classes
        LoadGraphOptions(graph);

        // select the dataset by name from the property template
        SelectDataset(heatmap->GetPropertyTemplate(L"dataset"));

        // load column names from property templates
        m_rowVariable = heatmap->GetPropertyTemplate(L"variables.row");
        m_columnVariable = heatmap->GetPropertyTemplate(L"variables.column");
        m_rowGroupVariable = heatmap->GetPropertyTemplate(L"variables.row-group");
        m_columnGroupVariable = heatmap->GetPropertyTemplate(L"variables.column-group");
        UpdateVariableLabels();

        m_cellValuesIndex = static_cast<int>(heatmap->GetCellValues());
        m_showTotals = heatmap->IsShowingTotals();

        TransferDataToWindow();
        }

    //-------------------------------------------
    std::shared_ptr<Graphs::MatrixHeatMap>
    InsertMatrixHeatMapDlg::BuildMatrixHeatMap(const Graphs::Graph2D* oldGraph)
        {
        auto plot = std::make_shared<Graphs::MatrixHeatMap>(GetCanvas());
        if (oldGraph != nullptr)
            {
            plot->SetId(oldGraph->GetId());
            }
        ApplyGraphOptions(*plot);
        ApplyPageOptions(*plot);

        const auto asOptional = [](const wxString& variable)
        { return variable.empty() ? std::nullopt : std::optional<wxString>{ variable }; };
        plot->SetData(GetSelectedDataset(), GetRowVariable(), GetColumnVariable(),
                      asOptional(GetRowGroupVariable()), asOptional(GetColumnGroupVariable()));
        plot->SetCellValues(GetCellValues());
        plot->ShowTotals(IsShowingTotals());
        ApplyAxisOverrides(*plot);

        if (oldGraph != nullptr)
            {
            const auto* oldHeatMap = dynamic_cast<const Graphs::MatrixHeatMap*>(oldGraph);
            WisteriaView::CarryForwardProperty(*oldGraph, *plot, L"dataset",
                                               GetSelectedDatasetName(),
                                               oldGraph->GetPropertyTemplate(L"dataset"));
            WisteriaView::CarryForwardProperty(
                *oldGraph, *plot, L"variables.row", GetRowVariable(),
                oldHeatMap != nullptr ? oldHeatMap->GetRowColumnName() : wxString{});
            WisteriaView::CarryForwardProperty(
                *oldGraph, *plot, L"variables.column", GetColumnVariable(),
                oldHeatMap != nullptr ? oldHeatMap->GetColumnColumnName() : wxString{});
            WisteriaView::CarryForwardProperty(
                *oldGraph, *plot, L"variables.row-group", GetRowGroupVariable(),
                oldHeatMap != nullptr ? oldHeatMap->GetRowGroupColumnName() : wxString{});
            WisteriaView::CarryForwardProperty(
                *oldGraph, *plot, L"variables.column-group", GetColumnGroupVariable(),
                oldHeatMap != nullptr ? oldHeatMap->GetColumnGroupColumnName() : wxString{});
            }
        else
            {
            plot->SetPropertyTemplate(L"dataset", GetSelectedDatasetName());
            plot->SetPropertyTemplate(L"variables.row", GetRowVariable());
            plot->SetPropertyTemplate(L"variables.column", GetColumnVariable());
            if (!GetRowGroupVariable().empty())
                {
                plot->SetPropertyTemplate(L"variables.row-group", GetRowGroupVariable());
                }
            if (!GetColumnGroupVariable().empty())
                {
                plot->SetPropertyTemplate(L"variables.column-group", GetColumnGroupVariable());
                }
            }

        return plot;
        }
    } // namespace Wisteria::UI
