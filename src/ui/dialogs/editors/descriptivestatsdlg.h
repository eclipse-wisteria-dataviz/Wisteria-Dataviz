/** @addtogroup UI
    @brief User interface classes.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef DESCRIPTIVE_STATS_DLG_H
#define DESCRIPTIVE_STATS_DLG_H

#include "../../analyses/descriptivestatistics.h"
#include "../../reporting/reportbuilder.h"
#include "../dialogwithhelp.h"
#include <memory>
#include <vector>
#include <wx/wx.h>

namespace Wisteria::UI
    {
    /// @brief Dialog for setting up a descriptive statistics analysis.
    class DescriptiveStatsDlg final : public DialogWithHelp
        {
      public:
        /** @brief Constructor.
            @param reportBuilder The report builder containing the project's datasets.
            @param analysis The analysis to edit (or a new one with default options).
            @param parent The parent window.
            @param caption The title of the dialog.
            @param id The window ID.
            @param pos The screen position.
            @param size The window size.
            @param style The window style.*/
        DescriptiveStatsDlg(const ReportBuilder* reportBuilder,
                            const Analyses::DescriptiveStatistics& analysis, wxWindow* parent,
                            const wxString& caption, wxWindowID id = wxID_ANY,
                            const wxPoint& pos = wxDefaultPosition,
                            const wxSize& size = wxDefaultSize,
                            long style = wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN |
                                         wxRESIZE_BORDER);

        /// @private
        DescriptiveStatsDlg(const DescriptiveStatsDlg&) = delete;
        /// @private
        DescriptiveStatsDlg& operator=(const DescriptiveStatsDlg&) = delete;

        /// @returns A copy of the analysis passed to the constructor,
        ///     with the options from the dialog.
        [[nodiscard]]
        std::unique_ptr<Analyses::DescriptiveStatistics> GetAnalysis() const;

      private:
        void CreateControls();
        bool TransferDataFromWindow() final;
        void OnDatasetChanged();
        void OnSelectColumns();
        void UpdateColumnsLabel();
        [[nodiscard]]
        std::shared_ptr<const Data::Dataset> GetSelectedDataset() const;

        const ReportBuilder* m_reportBuilder{ nullptr };
        std::unique_ptr<Analyses::DescriptiveStatistics> m_analysis;

        std::vector<wxString> m_datasetNames;
        std::vector<wxString> m_columns;
        wxString m_groupColumn;

        // DDX values
        int m_datasetIndex{ 0 };
        wxString m_outputName;
        bool m_includeN{ true };
        bool m_includeMissing{ true };
        bool m_includeMean{ true };
        bool m_includeStdDev{ true };
        bool m_includeMin{ true };
        bool m_includeQuartiles{ true };
        bool m_includeMax{ true };

        wxChoice* m_datasetChoice{ nullptr };
        wxStaticText* m_columnsLabel{ nullptr };
        wxStaticText* m_groupLabel{ nullptr };
        };
    } // namespace Wisteria::UI

/** @}*/

#endif // DESCRIPTIVE_STATS_DLG_H
