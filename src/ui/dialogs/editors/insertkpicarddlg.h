/** @addtogroup UI
    @brief User interface classes.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef INSERT_KPI_CARD_DIALOG_H
#define INSERT_KPI_CARD_DIALOG_H

#include "../../graphs/kpicard.h"
#include "insertgraphdlg.h"
#include <optional>
#include <vector>
#include <wx/clrpicker.h>
#include <wx/fontpicker.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

namespace Wisteria::UI
    {
    /** @brief Dialog for inserting or editing a KPI card.
        @details Contains a "KPI Card" page (the dataset, the column and how it is
            aggregated, and the number's and body text's appearance) and a
            "Number Format" page.*/
    class InsertKpiCardDlg final : public InsertGraphDlg
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
        InsertKpiCardDlg(Canvas* canvas, const ReportBuilder* reportBuilder, wxWindow* parent,
                         const wxString& caption = _(L"Insert KPI Card"), wxWindowID id = wxID_ANY,
                         const wxPoint& pos = wxDefaultPosition, const wxSize& size = wxDefaultSize,
                         long style = wxDEFAULT_DIALOG_STYLE | wxCLIP_CHILDREN | wxRESIZE_BORDER,
                         EditMode editMode = EditMode::Insert);

        /// @private
        InsertKpiCardDlg(const InsertKpiCardDlg&) = delete;
        /// @private
        InsertKpiCardDlg& operator=(const InsertKpiCardDlg&) = delete;

        /// @brief Populates all dialog controls from an existing KPI card.
        /// @param graph The graph to read settings from.
        void LoadFromGraph(const Graphs::Graph2D& graph);

        /** @brief Constructs a KPI card from the dialog's current settings.
            @param oldGraph The previous graph being edited, or @c nullptr if inserting new.
            @returns The newly constructed KPI card.*/
        [[nodiscard]]
        std::shared_ptr<Graphs::KpiCard> BuildKpiCard(const Graphs::Graph2D* oldGraph = nullptr);

      protected:
        void CreateControls() override;
        [[nodiscard]]
        DataGuide GetDataGuide() const final;

      private:
        /// @returns The variables of the card, in the order of the variable selection dialog.
        [[nodiscard]]
        static std::vector<DataGuideParameter> GetParameters();

        void CreateKpiPage();
        void CreateNumberFormatPage();

        bool Validate() override;
        bool TransferDataToWindow() final;
        void OnSelectVariables();
        void OnDatasetChanged();
        void UpdateVariableLabels();
        void UpdatePreview();

        /// @brief Applies the "Number Format" page's settings to @p card.
        void ApplyNumberFormat(Graphs::KpiCard& card) const;

        // starts at +2 to avoid collision with InsertItemDlg::ID_PAGE_SECTION (+1)
        // and InsertGraphDlg::ID_DATASET_CHOICE (+3)
        constexpr static wxWindowID ID_KPI_SECTION{ wxID_HIGHEST + 2 };
        constexpr static wxWindowID ID_SELECT_VARS_BUTTON{ wxID_HIGHEST + 4 };
        constexpr static wxWindowID ID_NUMBER_FORMAT_SECTION{ wxID_HIGHEST + 5 };

        wxStaticText* m_valueVarLabel{ nullptr };
        wxStaticText* m_previewLabel{ nullptr };
        wxStaticText* m_formatPreviewLabel{ nullptr };

        wxFontPickerCtrl* m_numberFontPicker{ nullptr };
        wxSpinCtrlDouble* m_numberScalingSpin{ nullptr };
        wxColourPickerCtrl* m_numberColorPicker{ nullptr };
        wxFontPickerCtrl* m_bodyFontPicker{ nullptr };
        wxColourPickerCtrl* m_bodyColorPicker{ nullptr };

        // the label of the card being edited, which holds the settings
        // that this dialog doesn't have controls for
        std::optional<GraphItems::Label> m_baseLabel;

        // set while the validators fill the controls, so that the text-changed events
        // they send don't read the unfilled controls back into the DDX members
        bool m_transferringToWindow{ false };

        // DDX data members
        wxString m_valueVariable;
        int m_aggregateIndex{ 0 }; // in Data::ColumnAggregation order

        // number (the label's header)
        int m_numberAlignment{ 2 }; // in TextAlignment order (Centered)

        // body text
        wxString m_bodyText;
        int m_bodyAlignment{ 2 }; // in TextAlignment order (Centered)

        // layout preset
        int m_layoutPreset{ 0 };

        // number format
        int m_numberStyleIndex{ 0 }; // in NumberDisplay order
        int m_precision{ 0 };
        int m_abbreviationIndex{ 0 }; // in KpiNumberAbbreviation order
        bool m_showPlusSign{ false };
        wxString m_suffix;
        wxString m_missingValueText{ L"—" };
        };
    } // namespace Wisteria::UI

/// @}

#endif // INSERT_KPI_CARD_DIALOG_H
