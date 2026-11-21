/** @addtogroup UI
    @brief User interface classes.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef WISTERIA_TELL_ME_CTRL_H
#define WISTERIA_TELL_ME_CTRL_H

#include "../tellmeregistry.h"
#include <functional>
#include <map>
#include <vector>
#include <wx/popupwin.h>
#include <wx/srchctrl.h>
#include <wx/wx.h>

namespace Wisteria::UI
    {
    /// @brief A search box that finds and runs program features as you type
    ///     (e.g., "What would you like to do?").
    /// @details Matches are shown in a dropdown, grouped by category with icons.
    ///     Choosing one runs it through a TellMeRegistry.\n
    ///     If the find row is enabled, choosing it (or pressing Enter with nothing
    ///     selected) sends a @c wxEVT_SEARCH event, as a plain @c wxSearchCtrl does.
    class TellMeCtrl final : public wxSearchCtrl
        {
      public:
        /// @brief Returns an icon for a feature's icon path.
        using IconResolver = std::function<wxBitmapBundle(const wxString& path)>;

        /// @brief Constructor.
        /// @param parent The parent window.
        /// @param id The control's ID.
        /// @param registry The features to search. If @c nullptr, the application's
        ///     registry is used (if the application is a BaseApp).
        /// @param size The control's size.
        TellMeCtrl(wxWindow* parent, wxWindowID id = wxID_ANY, TellMeRegistry* registry = nullptr,
                   const wxSize& size = wxDefaultSize);
        /// @private
        TellMeCtrl(const TellMeCtrl&) = delete;
        /// @private
        TellMeCtrl& operator=(const TellMeCtrl&) = delete;
        /// @private
        ~TellMeCtrl() override;

        /// @brief Sets the function that loads feature icons.
        /// @details By default, icons are loaded from the application's resource manager.
        /// @param resolver The icon loader.
        void SetIconResolver(IconResolver resolver) { m_iconResolver = std::move(resolver); }

        /// @brief Shows a "Find in document" row in the dropdown.
        /// @param enable @c true to show the row.
        void EnableFindRow(const bool enable) noexcept { m_findRowEnabled = enable; }

        /// @brief Sets the previous searches shown when the box is empty
        ///     (e.g., when loading settings).
        /// @details Searches sent through the find row are added to this list.
        /// @param searches The searches, oldest first.
        void SetRecentSearches(const wxArrayString& searches) { m_recentSearches = searches; }

        /// @returns The previous searches, oldest first.
        [[nodiscard]]
        const wxArrayString& GetRecentSearches() const noexcept
            {
            return m_recentSearches;
            }

        /// @brief Shows the dropdown for the current text.
        void ShowResults();
        /// @brief Hides the dropdown.
        void HideResults();

      private:
        enum class RowType
            {
            Header,
            Item,
            Find,
            RecentSearch
            };

        struct ResultRow
            {
            RowType m_type{ RowType::Header };
            wxString m_text;
            wxString m_description;
            wxString m_itemId;
            wxBitmapBundle m_icon;
            std::vector<std::pair<size_t, size_t>> m_highlights;
            bool m_enabled{ true };
            };

        class ResultList;

        constexpr static size_t MAX_RECENT_ITEMS{ 5 };
        constexpr static size_t MAX_RECENT_SEARCHES{ 3 };
        constexpr static size_t MAX_SEARCH_HISTORY{ 20 };

        [[nodiscard]]
        TellMeRegistry* GetRegistry() const noexcept
            {
            return m_registry;
            }

        [[nodiscard]]
        wxBitmapBundle GetIcon(const wxString& path);
        [[nodiscard]]
        bool IsSelectable(size_t row) const;
        [[nodiscard]]
        bool HasFocusWithin() const;
        void AddItemRow(const TellMeMatch& match, wxWindow* frame);
        void RebuildRows();
        void PositionPopup();
        void MoveSelection(int step);
        void RunRow(size_t row);
        void AddRecentSearch(const wxString& search);
        void SendSearchEvent();
        void OnCharHook(wxKeyEvent& event);
        void OnFocusLost(wxFocusEvent& event);
        void OnFrameChanged(wxEvent& event);

        TellMeRegistry* m_registry{ nullptr };
        IconResolver m_iconResolver;
        std::map<wxString, wxBitmapBundle> m_iconCache;
        wxArrayString m_recentSearches;
        bool m_findRowEnabled{ false };

        wxWindow* m_topLevel{ nullptr };
        wxPopupWindow* m_popup{ nullptr };
        ResultList* m_list{ nullptr };
        std::vector<ResultRow> m_rows;
        };
    } // namespace Wisteria::UI

/** @}*/

#endif // WISTERIA_TELL_ME_CTRL_H
