/** @addtogroup UI
    @brief User interface classes.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef INSERT_MATRIX_HEATMAP_DIALOG_H
#define INSERT_MATRIX_HEATMAP_DIALOG_H

#include "../../graphs/matrixheatmap.h"
#include "insertgraphdlg.h"
#include <vector>
#include <wx/wx.h>

namespace Wisteria::UI
    {
    /** @brief Dialog for inserting a matrix heat map into a canvas cell.
        @details Extends InsertGraphDlg with an "Options" page containing:
            - A dataset selector (from the project's datasets).
            - A "Variables..." button that opens a VariableSelectDlg
              for selecting the row and column variables and their optional groups.
            - Labels showing the current variable selections.
            - A choice of whether the cells show counts or percentages.
            - An option to show a total for each row and column.*/
    class InsertMatrixHeatMapDlg final : public InsertGraphDlg
        {
      public:
        /** @brief Constructor.
            @param canvas The canvas whose grid layout is displayed.
            @param reportBuilder The report builder containing the project's datasets.
            @param parent The parent window.
            @param caption The dialog title.
            @param id The window ID.
            @param pos The screen position.
            @param size The window size.
            @param style The window style.
            @param editMode Whether the item is being inserted or edited.*/
        InsertMatrixHeatMapDlg(Canvas* canvas, const ReportBuilder* reportBuilder, wxWindow* parent,
                               const wxString& caption = _(L"Insert Matrix Heat Map"),
                               wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
                               const wxSize& size = wxDefaultSize,
                               long style = wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN |
                                            wxRESIZE_BORDER,
                               EditMode editMode = EditMode::Insert);

        /// @private
        InsertMatrixHeatMapDlg(const InsertMatrixHeatMapDlg&) = delete;
        /// @private
        InsertMatrixHeatMapDlg& operator=(const InsertMatrixHeatMapDlg&) = delete;

        /// @returns The row variable name (with any constant placeholders expanded).
        [[nodiscard]]
        wxString GetRowVariable() const
            {
            return ExpandVariable(m_rowVariable);
            }

        /// @returns The column variable name (with any constant placeholders expanded).
        [[nodiscard]]
        wxString GetColumnVariable() const
            {
            return ExpandVariable(m_columnVariable);
            }

        /// @returns The row grouping variable name (with any constant placeholders
        ///     expanded), or empty if none.
        [[nodiscard]]
        wxString GetRowGroupVariable() const
            {
            return ExpandVariable(m_rowGroupVariable);
            }

        /// @returns The column grouping variable name (with any constant placeholders
        ///     expanded), or empty if none.
        [[nodiscard]]
        wxString GetColumnGroupVariable() const
            {
            return ExpandVariable(m_columnGroupVariable);
            }

        /// @returns Which value each cell shows.
        [[nodiscard]]
        CrossTabValue GetCellValues() const noexcept
            {
            return static_cast<CrossTabValue>(m_cellValuesIndex);
            }

        /// @returns Whether to show a total for each row and column.
        [[nodiscard]]
        bool IsShowingTotals() const noexcept
            {
            return m_showTotals;
            }

        /// @brief Populates all dialog controls from an existing matrix heat map.
        /// @param graph The graph to read settings from.
        void LoadFromGraph(const Graphs::Graph2D& graph);

        /// @returns A new matrix heat map built from the dialog's current settings.
        /// @param oldGraph When editing, the graph being replaced (so its ID and
        ///     any unchanged property templates are carried forward); @c nullptr
        ///     when inserting a new matrix heat map.
        /// @note Call after @c ShowModal() returns @c wxID_OK.
        [[nodiscard]]
        std::shared_ptr<Graphs::MatrixHeatMap>
        BuildMatrixHeatMap(const Graphs::Graph2D* oldGraph = nullptr);

      protected:
        void CreateControls() override;
        [[nodiscard]]
        DataGuide GetDataGuide() const final;

      private:
        /// @returns The variables of the plot, in the order of the variable selection dialog.
        [[nodiscard]]
        static std::vector<DataGuideParameter> GetParameters();

        bool Validate() override;
        void OnSelectVariables();
        void OnDatasetChanged();
        void UpdateVariableLabels();

        // starts at +2 to avoid collision with InsertItemDlg::ID_PAGE_SECTION (+1)
        constexpr static wxWindowID ID_OPTIONS_SECTION{ wxID_HIGHEST + 2 };
        constexpr static wxWindowID ID_SELECT_VARS_BUTTON{ wxID_HIGHEST + 4 };

        wxStaticText* m_rowVarLabel{ nullptr };
        wxStaticText* m_columnVarLabel{ nullptr };
        wxStaticText* m_rowGroupVarLabel{ nullptr };
        wxStaticText* m_columnGroupVarLabel{ nullptr };

        wxString m_rowVariable;
        wxString m_columnVariable;
        wxString m_rowGroupVariable;
        wxString m_columnGroupVariable;

        // the index of the CrossTabValue to show in the cells
        int m_cellValuesIndex{ 0 };
        bool m_showTotals{ false };
        };
    } // namespace Wisteria::UI

/// @}

#endif // INSERT_MATRIX_HEATMAP_DIALOG_H
