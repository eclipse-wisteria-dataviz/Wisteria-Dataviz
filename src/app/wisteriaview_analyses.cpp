///////////////////////////////////////////////////////////////////////////////
// Name:        wisteriaview_analyses.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "../analyses/descriptivestatistics.h"
#include "../ui/controls/datasetgridtable.h"
#include "../ui/dialogs/editors/descriptivestatsdlg.h"
#include "analysisresultswindow.h"
#include "wisteriaapp.h"
#include "wisteriaview.h"

//-------------------------------------------
AnalysisResultsWindow* WisteriaView::GetSelectedAnalysisWindow() const noexcept
    {
    for (auto* window : m_analysisWindows)
        {
        if (window != nullptr && window->IsShown())
            {
            return window;
            }
        }
    return nullptr;
    }

//-------------------------------------------
AnalysisResultsWindow*
WisteriaView::CreateAnalysisWindow(const Wisteria::Analyses::Analysis& analysis)
    {
    auto* window = new AnalysisResultsWindow(m_workArea, wxNewId(), analysis.GetGuid(),
                                             [](wxGrid* grid, Wisteria::UI::DatasetGridTable* table)
                                             {
                                                 ApplyColumnHeaderIcons(grid, table);
                                                 AdjustGridColumnsForIcons(grid);
                                             });
    window->Hide();
    window->SetResults(analysis.GetResults());
    m_workArea->GetSizer()->Add(window, wxSizerFlags{ 1 }.Expand());
    m_workWindows.AddWindow(window);
    return window;
    }

//-------------------------------------------
void WisteriaView::AddAnalysisToSection(const size_t index)
    {
    const auto& analyses = m_reportBuilder.GetAnalyses();
    if (index >= analyses.size() || analyses[index] == nullptr)
        {
        return;
        }
    const auto& analysis = *analyses[index];

    auto* window = CreateAnalysisWindow(analysis);
    m_analysisWindows.push_back(window);
    m_sideBar->InsertSubItemById(m_analysisFolderId, analysis.GetName(), window->GetId(),
                                 ANALYSIS_ICON_INDEX);
    m_sideBar->GetFolder(ANALYSIS_FOLDER_INDEX).Expand();
    m_sideBar->SelectSubItemById(m_analysisFolderId, window->GetId());

    GetDocument()->Modify(true);
    }

//-------------------------------------------
void WisteriaView::RemoveAnalysisFromSection(AnalysisResultsWindow* window)
    {
    const auto foundWindow = std::ranges::find(m_analysisWindows, window);
    if (foundWindow == m_analysisWindows.end())
        {
        wxFAIL_MSG(L"Window not found when removing analysis?!");
        return;
        }

    const wxWindowUpdateLocker noUpdates{ m_frame };

    m_reportBuilder.RemoveAnalysis(window->GetGuid());

    m_sideBar->SelectFolder(ANALYSIS_FOLDER_INDEX, false, false, false);
    m_sideBar->DeleteSubItemById(m_analysisFolderId, window->GetId());
    const auto removedPos =
        static_cast<size_t>(std::distance(m_analysisWindows.begin(), foundWindow));
    m_analysisWindows.erase(foundWindow);
    m_workWindows.RemoveWindowById(window->GetId());
    m_workArea->GetSizer()->Detach(window);
    window->Destroy();

    // show the analysis that took its place (or the one before it), or the data section
    if (!m_analysisWindows.empty())
        {
        const auto* nextWindow =
            m_analysisWindows[std::min(removedPos, m_analysisWindows.size() - 1)];
        m_sideBar->SelectSubItemById(m_analysisFolderId, nextWindow->GetId());
        }
    else
        {
        m_sideBar->SelectFolder(0);
        }
    m_workArea->Layout();

    GetDocument()->Modify(true);
    }

//-------------------------------------------
wxString WisteriaView::GetUniqueAnalysisName(const wxString& name, const wxString& ignoreGuid) const
    {
    const auto isNameTaken = [this, &ignoreGuid](const wxString& label)
    {
        return std::ranges::any_of(m_reportBuilder.GetAnalyses(),
                                   [&ignoreGuid, &label](const auto& analysis)
                                   {
                                       return analysis != nullptr &&
                                              analysis->GetGuid() != ignoreGuid &&
                                              analysis->GetName().CmpNoCase(label) == 0;
                                   });
    };

    if (!isNameTaken(name))
        {
        return name;
        }
    for (size_t suffix = 2;; ++suffix)
        {
        const wxString candidate = wxString::Format(L"%s (%zu)", name, suffix);
        if (!isNameTaken(candidate))
            {
            return candidate;
            }
        }
    }

//-------------------------------------------
void WisteriaView::RunAnalysisAndReport(const size_t index)
    {
    try
        {
        m_reportBuilder.RunAnalysis(index);
        const auto& warnings = m_reportBuilder.GetAnalyses()[index]->GetWarnings();
        if (!warnings.empty())
            {
            wxString message;
            for (const auto& warning : warnings)
                {
                if (!message.empty())
                    {
                    message += L"\n\n";
                    }
                message += warning;
                }
            wxMessageBox(message, _(L"Analysis Warning"), wxOK | wxICON_WARNING, m_frame);
            }
        }
    catch (const std::exception& exc)
        {
        wxMessageBox(wxString::FromUTF8(exc.what()), _(L"Analysis Error"), wxOK | wxICON_WARNING,
                     m_frame);
        }
    }

//-------------------------------------------
void WisteriaView::ExportDatasetToFile(const Wisteria::Data::Dataset& dataset,
                                       const wxString& defaultName, const wxString& caption)
    {
    wxFileDialog fileDlg(m_frame, caption, wxString{}, defaultName,
                         _(L"Comma-separated values (*.csv)|*.csv|"
                           "Tab-delimited text (*.txt)|*.txt"),
                         wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (fileDlg.ShowModal() != wxID_OK)
        {
        return;
        }
    try
        {
        if (fileDlg.GetFilterIndex() == 0)
            {
            dataset.ExportCSV(fileDlg.GetPath());
            }
        else
            {
            dataset.ExportTSV(fileDlg.GetPath());
            }
        }
    catch (const std::exception& exc)
        {
        wxMessageBox(wxString::FromUTF8(exc.what()), caption, wxOK | wxICON_ERROR, m_frame);
        }
    }

//-------------------------------------------
void WisteriaView::UpdateAnalysisButtonStates() const
    {
    const bool enabled = (GetSelectedAnalysisWindow() != nullptr);

    if (m_analysisButtonBar != nullptr)
        {
        m_analysisButtonBar->EnableButton(ID_EDIT_ANALYSIS, enabled);
        m_analysisButtonBar->EnableButton(ID_DELETE_ANALYSIS, enabled);
        }

    if (wxMenuBar* const menuBar = (m_frame != nullptr) ? m_frame->GetMenuBar() : nullptr;
        menuBar != nullptr)
        {
        menuBar->Enable(ID_EDIT_ANALYSIS, enabled);
        menuBar->Enable(ID_DELETE_ANALYSIS, enabled);
        }
    }

//-------------------------------------------
void WisteriaView::OnDescriptiveStatistics([[maybe_unused]] wxCommandEvent& event)
    {
    if (m_reportBuilder.GetDatasets().empty())
        {
        wxMessageBox(_(L"Please import a dataset first."), _(L"No Datasets"),
                     wxOK | wxICON_INFORMATION, m_frame);
        return;
        }

    Wisteria::Analyses::DescriptiveStatistics analysis;
    if (const auto datasetName = GetSelectedDatasetName(); datasetName.has_value())
        {
        analysis.SetSourceDatasetName(datasetName.value());
        analysis.SetName(wxString::Format(_(L"%s (Descriptive Statistics)"), datasetName.value()));
        }

    Wisteria::UI::DescriptiveStatsDlg dlg(&m_reportBuilder, analysis, m_frame,
                                          _(L"Descriptive Statistics"));
    SetDialogHelpTopic(dlg, L"online/dialogs/analyses.html");
    if (dlg.ShowModal() != wxID_OK)
        {
        return;
        }

    auto newAnalysis = dlg.GetAnalysis();
    newAnalysis->SetName(GetUniqueAnalysisName(newAnalysis->GetName()));
    const size_t index = m_reportBuilder.AddAnalysis(std::move(newAnalysis));
    RunAnalysisAndReport(index);
    AddAnalysisToSection(index);
    }

//-------------------------------------------
void WisteriaView::OnEditAnalysis([[maybe_unused]] wxCommandEvent& event)
    {
    auto* window = GetSelectedAnalysisWindow();
    if (window == nullptr)
        {
        return;
        }
    const auto index = m_reportBuilder.FindAnalysis(window->GetGuid());
    if (!index.has_value())
        {
        return;
        }
    auto& analysis = m_reportBuilder.GetAnalyses()[index.value()];

    // each type of analysis has its own dialog
    std::unique_ptr<Wisteria::Analyses::Analysis> editedAnalysis;
    if (const auto* descriptiveStats =
            dynamic_cast<const Wisteria::Analyses::DescriptiveStatistics*>(analysis.get());
        descriptiveStats != nullptr)
        {
        Wisteria::UI::DescriptiveStatsDlg dlg(&m_reportBuilder, *descriptiveStats, m_frame,
                                              _(L"Edit Descriptive Statistics"));
        SetDialogHelpTopic(dlg, L"online/dialogs/analyses.html");
        if (dlg.ShowModal() != wxID_OK)
            {
            return;
            }
        editedAnalysis = dlg.GetAnalysis();
        }
    else
        {
        return;
        }

    editedAnalysis->SetName(
        GetUniqueAnalysisName(editedAnalysis->GetName(), editedAnalysis->GetGuid()));
    analysis = std::move(editedAnalysis);
    RunAnalysisAndReport(index.value());

    window->SetResults(analysis->GetResults());
    m_sideBar->SetSubItemLabel(m_analysisFolderId, window->GetId(), analysis->GetName());
    m_workArea->Layout();

    GetDocument()->Modify(true);
    }

//-------------------------------------------
void WisteriaView::OnDeleteAnalysis([[maybe_unused]] wxCommandEvent& event)
    {
    auto* window = GetSelectedAnalysisWindow();
    if (window == nullptr)
        {
        return;
        }
    if (wxMessageBox(_(L"Are you sure you want to delete this analysis?"), _(L"Delete Analysis"),
                     wxYES_NO | wxICON_QUESTION, m_frame) == wxYES)
        {
        RemoveAnalysisFromSection(window);
        }
    }

//-------------------------------------------
void WisteriaView::OnExportAnalysis([[maybe_unused]] wxCommandEvent& event)
    {
    const auto* window = GetSelectedAnalysisWindow();
    if (window == nullptr)
        {
        return;
        }
    const auto index = m_reportBuilder.FindAnalysis(window->GetGuid());
    if (!index.has_value())
        {
        return;
        }
    const auto& analysis = *m_reportBuilder.GetAnalyses()[index.value()];
    const auto& results = analysis.GetResults();
    const size_t tableIndex = window->GetCurrentTableIndex();
    if (tableIndex >= results.size() || results[tableIndex].m_data == nullptr)
        {
        wxMessageBox(_(L"No results are available for this analysis."), _(L"Export Results"),
                     wxOK | wxICON_INFORMATION, m_frame);
        return;
        }

    const auto& table = results[tableIndex];
    const wxString defaultName =
        (results.size() > 1) ? wxString::Format(L"%s - %s", analysis.GetName(), table.m_name) :
                               analysis.GetName();
    ExportDatasetToFile(*table.m_data, defaultName, _(L"Export Results"));
    }
