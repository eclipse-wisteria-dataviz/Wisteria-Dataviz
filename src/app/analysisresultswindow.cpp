///////////////////////////////////////////////////////////////////////////////
// Name:        analysisresultswindow.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "analysisresultswindow.h"
#include <wx/wupdlock.h>

//-------------------------------------------
AnalysisResultsWindow::AnalysisResultsWindow(wxWindow* parent, const wxWindowID id, wxString guid,
                                             GridSetupFunction setupGrid)
    : wxPanel(parent, id), m_guid(std::move(guid)), m_setupGrid(std::move(setupGrid))
    {
    SetSizer(new wxBoxSizer{ wxVERTICAL });
    }

//-------------------------------------------
wxGrid* AnalysisResultsWindow::CreateGrid(wxWindow* parent,
                                          const std::shared_ptr<Wisteria::Data::Dataset>& dataset)
    {
    auto* table = new Wisteria::UI::DatasetGridTable(dataset);
    auto* grid = new wxGrid(parent, wxID_ANY);
    grid->SetDoubleBuffered(true);
    grid->GetGridWindow()->SetDoubleBuffered(true);
    grid->SetTable(table, true);
    grid->SetDefaultCellFitMode(wxGridFitMode::Ellipsize());
    grid->EnableEditing(false);

    // the columns are sized to fit the grid's width, which is only known once it is shown
    m_unsizedGrids.insert(grid);
    grid->Bind(wxEVT_SIZE,
               [this, grid](wxSizeEvent& event)
               {
                   event.Skip();
                   SizeColumnsWhenShown(grid);
               });
    grid->Bind(wxEVT_SHOW,
               [this, grid](wxShowEvent& event)
               {
                   event.Skip();
                   if (event.IsShown())
                       {
                       SizeColumnsWhenShown(grid);
                       }
               });
    return grid;
    }

//-------------------------------------------
AnalysisResultsWindow::~AnalysisResultsWindow()
    {
    // the grids' event handlers use this window's members,
    // so destroy the grids before the members are
    m_unsizedGrids.clear();
    DestroyChildren();
    }

//-------------------------------------------
void AnalysisResultsWindow::SizeColumnsWhenShown(wxGrid* grid)
    {
    if (IsBeingDeleted() || !m_unsizedGrids.contains(grid) || !grid->IsShownOnScreen())
        {
        return;
        }
    // wait until the layout that showed the grid is finished
    CallAfter(
        [this, grid]()
        {
            if (!m_unsizedGrids.contains(grid) || !grid->IsShownOnScreen())
                {
                return;
                }
            m_unsizedGrids.erase(grid);
            grid->AutoSizeColumns(false);
            auto* table = dynamic_cast<Wisteria::UI::DatasetGridTable*>(grid->GetTable());
            if (m_setupGrid && table != nullptr)
                {
                m_setupGrid(grid, table);
                }
            grid->ForceRefresh();
        });
    }

//-------------------------------------------
void AnalysisResultsWindow::SetResults(
    const std::vector<Wisteria::Analyses::AnalysisResultTable>& results)
    {
    const wxWindowUpdateLocker noUpdates{ this };

    // the grids are about to be destroyed
    m_unsizedGrids.clear();
    GetSizer()->Clear(true);
    m_notebook = nullptr;

    if (results.empty())
        {
        GetSizer()->Add(
            new wxStaticText(this, wxID_ANY, _(L"No results are available for this analysis.")),
            wxSizerFlags{}.Border());
        }
    else if (results.size() == 1)
        {
        GetSizer()->Add(CreateGrid(this, results.front().m_data), wxSizerFlags{ 1 }.Expand());
        }
    else
        {
        m_notebook = new wxNotebook(this, wxID_ANY);
        for (const auto& result : results)
            {
            m_notebook->AddPage(CreateGrid(m_notebook, result.m_data), result.m_name);
            }
        GetSizer()->Add(m_notebook, wxSizerFlags{ 1 }.Expand());
        }

    Layout();
    }

//-------------------------------------------
size_t AnalysisResultsWindow::GetCurrentTableIndex() const
    {
    if (m_notebook != nullptr && m_notebook->GetSelection() != wxNOT_FOUND)
        {
        return static_cast<size_t>(m_notebook->GetSelection());
        }
    return 0;
    }
