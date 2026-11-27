///////////////////////////////////////////////////////////////////////////////
// Name:        analysisresultswindow.h
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#ifndef WISTERIA_ANALYSIS_RESULTS_WINDOW_H
#define WISTERIA_ANALYSIS_RESULTS_WINDOW_H

#include "../analyses/analysis.h"
#include "../ui/controls/datasetgridtable.h"
#include <functional>
#include <set>
#include <vector>
#include <wx/grid.h>
#include <wx/notebook.h>
#include <wx/wx.h>

/// @brief Shows an analysis's result tables as read-only grids.
/// @details A single table is shown as a plain grid. Multiple tables are shown on tabs.
class AnalysisResultsWindow final : public wxPanel
    {
  public:
    /// @brief Sets up a grid once it is shown (e.g., its column header icons and widths).
    using GridSetupFunction = std::function<void(wxGrid*, Wisteria::UI::DatasetGridTable*)>;

    /** @brief Constructor.
        @param parent The parent window.
        @param id The window ID.
        @param guid The GUID of the analysis that this window shows.
        @param setupGrid Called for each grid after its columns are fit to their content,
            the first time that the grid is shown.*/
    AnalysisResultsWindow(wxWindow* parent, wxWindowID id, wxString guid,
                          GridSetupFunction setupGrid);

    /// @private
    ~AnalysisResultsWindow() override;
    /// @private
    AnalysisResultsWindow(const AnalysisResultsWindow&) = delete;
    /// @private
    AnalysisResultsWindow& operator=(const AnalysisResultsWindow&) = delete;

    /// @brief Rebuilds the grids from an analysis's results.
    /// @param results The result tables.
    void SetResults(const std::vector<Wisteria::Analyses::AnalysisResultTable>& results);

    /// @returns The GUID of the analysis that this window shows.
    [[nodiscard]]
    const wxString& GetGuid() const noexcept
        {
        return m_guid;
        }

    /// @returns The index of the result table being shown.
    [[nodiscard]]
    size_t GetCurrentTableIndex() const;

  private:
    [[nodiscard]]
    wxGrid* CreateGrid(wxWindow* parent, const std::shared_ptr<Wisteria::Data::Dataset>& dataset);
    void SizeColumnsWhenShown(wxGrid* grid);

    wxString m_guid;
    GridSetupFunction m_setupGrid;
    wxNotebook* m_notebook{ nullptr };
    // grids whose columns haven't been sized yet
    std::set<wxGrid*> m_unsizedGrids;
    };

#endif // WISTERIA_ANALYSIS_RESULTS_WINDOW_H
