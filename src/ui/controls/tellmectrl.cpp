///////////////////////////////////////////////////////////////////////////////
// Name:        tellmectrl.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "tellmectrl.h"
#include "../app.h"
#include <algorithm>
#include <wx/artprov.h>
#include <wx/dcclient.h>
#include <wx/settings.h>
#include <wx/vlbox.h>

namespace Wisteria::UI
    {
    /// @brief Owner-drawn list of Tell Me results.
    /// @private
    class TellMeCtrl::ResultList final : public wxVListBox
        {
      public:
        ResultList(wxWindow* parent, TellMeCtrl* owner)
            : wxVListBox(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE),
              m_owner(owner)
            {
            }

        ResultList(const ResultList&) = delete;
        ResultList& operator=(const ResultList&) = delete;

        [[nodiscard]]
        wxCoord GetRowHeight(const size_t row) const
            {
            return OnMeasureItem(row);
            }

      private:
        constexpr static int HEADER_HEIGHT{ 24 };
        constexpr static int ROW_HEIGHT{ 30 };
        constexpr static int DESCRIPTION_ROW_HEIGHT{ 48 };
        constexpr static int ICON_SIZE{ 16 };
        constexpr static int LARGE_ICON_SIZE{ 32 };

        void OnDrawItem(wxDC& dc, const wxRect& rect, size_t n) const final;
        wxCoord OnMeasureItem(size_t n) const final;

        TellMeCtrl* m_owner{ nullptr };
        };

    //---------------------------------------------------
    wxCoord TellMeCtrl::ResultList::OnMeasureItem(const size_t n) const
        {
        if (n >= m_owner->m_rows.size())
            {
            return 0;
            }
        const auto& row = m_owner->m_rows[n];
        return FromDIP(row.m_type == RowType::Header ? HEADER_HEIGHT :
                       !row.m_description.empty()    ? DESCRIPTION_ROW_HEIGHT :
                                                       ROW_HEIGHT);
        }

    //---------------------------------------------------
    void TellMeCtrl::ResultList::OnDrawItem(wxDC& dc, const wxRect& rect, const size_t n) const
        {
        if (n >= m_owner->m_rows.size())
            {
            return;
            }
        const auto& row = m_owner->m_rows[n];
        const wxDCClipper clipper{ dc, rect };
        const wxCoord padding{ FromDIP(8) };
        const wxFont normalFont{ GetFont() };
        const wxFont boldFont{ normalFont.Bold() };

        if (row.m_type == RowType::Header)
            {
            dc.SetFont(boldFont);
            dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
            dc.DrawText(row.m_text, rect.GetX() + padding,
                        rect.GetY() + (rect.GetHeight() - dc.GetCharHeight()) / 2);
            return;
            }

        // rows with a description get a larger icon and a second line of text
        const bool hasDescription{ !row.m_description.empty() };
        const int iconSize{ hasDescription ? LARGE_ICON_SIZE : ICON_SIZE };

        wxCoord xPos{ rect.GetX() + padding };
        if (row.m_icon.IsOk())
            {
            wxBitmap icon{ row.m_icon.GetBitmap(
                wxSize{ wxRound(iconSize * GetDPIScaleFactor()),
                        wxRound(iconSize * GetDPIScaleFactor()) }) };
            icon.SetScaleFactor(GetContentScaleFactor());
            dc.DrawBitmap(icon, xPos,
                          rect.GetY() + (rect.GetHeight() - icon.GetLogicalHeight()) / 2, true);
            }
        xPos += FromDIP(iconSize) + padding;

        const wxColour grayTextColor{ wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT) };
        const wxColour textColor{ !row.m_enabled ?
                                      grayTextColor :
                                  IsSelected(n) ?
                                      wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT) :
                                      wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT) };
        dc.SetTextForeground(textColor);
        dc.SetFont(normalFont);
        const wxCoord nameHeight{ dc.GetCharHeight() };
        wxCoord textY{ rect.GetY() + (rect.GetHeight() - nameHeight) / 2 };

        if (hasDescription)
            {
            dc.SetFont(normalFont.Smaller());
            const wxCoord descriptionHeight{ dc.GetCharHeight() };
            const wxCoord lineGap{ FromDIP(2) };
            textY =
                rect.GetY() + (rect.GetHeight() - (nameHeight + lineGap + descriptionHeight)) / 2;

            // long descriptions are cut off with an ellipsis
            const wxString description{ wxControl::Ellipsize(
                row.m_description, dc, wxELLIPSIZE_END,
                std::max(rect.GetRight() - xPos - padding, 0), wxELLIPSIZE_FLAGS_NONE) };
            dc.SetTextForeground(IsSelected(n) && row.m_enabled ? textColor : grayTextColor);
            dc.DrawText(description, xPos, textY + nameHeight + lineGap);
            dc.SetTextForeground(textColor);
            dc.SetFont(normalFont);
            }

        // draw the text in segments, bolding the parts that matched the query
        const auto drawSegment = [&dc, &xPos, textY](const wxString& segment, const wxFont& font)
        {
            if (segment.empty())
                {
                return;
                }
            dc.SetFont(font);
            dc.DrawText(segment, xPos, textY);
            xPos += dc.GetTextExtent(segment).GetWidth();
        };

        const wxString& text{ row.m_text };
        size_t textPos{ 0 };
        for (const auto& [start, length] : row.m_highlights)
            {
            if (start < textPos || start >= text.length())
                {
                continue;
                }
            drawSegment(text.substr(textPos, start - textPos), normalFont);
            drawSegment(text.substr(start, length), boldFont);
            textPos = std::min(start + length, text.length());
            }
        if (textPos < text.length())
            {
            drawSegment(text.substr(textPos), normalFont);
            }
        }

    //---------------------------------------------------
    TellMeCtrl::TellMeCtrl(wxWindow* parent, const wxWindowID id, TellMeRegistry* registry,
                           const wxSize& size)
        : wxSearchCtrl(parent, id, wxString{}, wxDefaultPosition, size, 0), m_registry(registry),
          m_topLevel(wxGetTopLevelParent(parent))
        {
        auto* app = dynamic_cast<BaseApp*>(wxTheApp);
        if (app != nullptr)
            {
            if (m_registry == nullptr)
                {
                m_registry = &app->GetTellMeRegistry();
                }
            m_iconResolver = [app](const wxString& path)
            { return app->GetResourceManager().GetSVG(path); };
            }

        SetDescriptiveText(_(L"What would you like to do?"));
        ShowCancelButton(true);

        m_popup = new wxPopupWindow(this, wxBORDER_SIMPLE);
        m_list = new ResultList(m_popup, this);
        auto* popupSizer = new wxBoxSizer(wxVERTICAL);
        popupSizer->Add(m_list, wxSizerFlags{ 1 }.Expand());
        m_popup->SetSizer(popupSizer);

        m_list->Bind(wxEVT_LEFT_DOWN,
                     [this](const wxMouseEvent& event)
                     {
                         const int hitRow{ m_list->VirtualHitTest(event.GetPosition().y) };
                         if (hitRow != wxNOT_FOUND)
                             {
                             RunRow(static_cast<size_t>(hitRow));
                             }
                     });
        m_list->Bind(wxEVT_MOTION,
                     [this](wxMouseEvent& event)
                     {
                         const int hitRow{ m_list->VirtualHitTest(event.GetPosition().y) };
                         if (hitRow != wxNOT_FOUND && hitRow != m_list->GetSelection() &&
                             IsSelectable(static_cast<size_t>(hitRow)))
                             {
                             m_list->SetSelection(hitRow);
                             }
                         event.Skip();
                     });

        Bind(wxEVT_TEXT,
             [this](wxCommandEvent& event)
             {
                 if (HasFocusWithin())
                     {
                     ShowResults();
                     }
                 event.Skip();
             });
        // remember searches that the find row (or Enter) sent
        Bind(wxEVT_SEARCH,
             [this](wxCommandEvent& event)
             {
                 if (m_findRowEnabled)
                     {
                     AddRecentSearch(GetValue().Strip(wxString::both));
                     }
                 event.Skip();
             });
        Bind(wxEVT_SEARCH_CANCEL,
             [this](wxCommandEvent& event)
             {
                 HideResults();
                 event.Skip();
             });
        Bind(wxEVT_CHAR_HOOK, &TellMeCtrl::OnCharHook, this);

        // the generic search control is a composite, so watch its children too
        std::vector<wxWindow*> focusWindows{ this };
        for (auto* child : GetChildren())
            {
            if (child != m_popup)
                {
                focusWindows.push_back(child);
                }
            }
        for (auto* window : focusWindows)
            {
            window->Bind(wxEVT_KILL_FOCUS, &TellMeCtrl::OnFocusLost, this);
            window->Bind(wxEVT_LEFT_DOWN,
                         [this](wxMouseEvent& event)
                         {
                             CallAfter([this]() { ShowResults(); });
                             event.Skip();
                         });
            }

        if (m_topLevel != nullptr)
            {
            m_topLevel->Bind(wxEVT_MOVE, &TellMeCtrl::OnFrameChanged, this);
            m_topLevel->Bind(wxEVT_SIZE, &TellMeCtrl::OnFrameChanged, this);
            m_topLevel->Bind(wxEVT_ICONIZE, &TellMeCtrl::OnFrameChanged, this);
            }
        }

    //---------------------------------------------------
    TellMeCtrl::~TellMeCtrl()
        {
        if (m_topLevel != nullptr)
            {
            m_topLevel->Unbind(wxEVT_MOVE, &TellMeCtrl::OnFrameChanged, this);
            m_topLevel->Unbind(wxEVT_SIZE, &TellMeCtrl::OnFrameChanged, this);
            m_topLevel->Unbind(wxEVT_ICONIZE, &TellMeCtrl::OnFrameChanged, this);
            }
        }

    //---------------------------------------------------
    void TellMeCtrl::ShowResults()
        {
        if (GetRegistry() == nullptr)
            {
            return;
            }
        RebuildRows();
        if (m_rows.empty())
            {
            HideResults();
            return;
            }

        m_list->SetItemCount(m_rows.size());
        m_list->SetSelection(wxNOT_FOUND);
        // without a find row, Enter runs the best match
        if (!m_findRowEnabled && !GetValue().Strip(wxString::both).empty())
            {
            MoveSelection(1);
            }
        m_list->RefreshAll();

        PositionPopup();
        if (!m_popup->IsShown())
            {
            m_popup->Show();
            }
        }

    //---------------------------------------------------
    void TellMeCtrl::HideResults()
        {
        if (m_popup == nullptr || !m_popup->IsShown())
            {
            return;
            }
        const bool focusInList{ wxWindow::FindFocus() == m_list };
        m_popup->Hide();
        if (focusInList)
            {
            SetFocus();
            }
        }

    //---------------------------------------------------
    void TellMeCtrl::PositionPopup()
        {
        wxCoord listHeight{ 0 };
        for (size_t i = 0; i < m_rows.size(); ++i)
            {
            listHeight += m_list->GetRowHeight(i);
            }
        listHeight = std::min(listHeight, FromDIP(480));
        const int popupWidth{ std::max(GetSize().GetWidth(), FromDIP(380)) };

        m_popup->SetClientSize(popupWidth, listHeight);
        m_popup->Layout();
        m_popup->Position(ClientToScreen(wxPoint{ 0, 0 }), wxSize{ 0, GetSize().GetHeight() });
        }

    //---------------------------------------------------
    bool TellMeCtrl::HasFocusWithin() const
        {
        for (const wxWindow* window = wxWindow::FindFocus(); window != nullptr;
             window = window->GetParent())
            {
            if (window == this)
                {
                return true;
                }
            }
        return false;
        }

    //---------------------------------------------------
    bool TellMeCtrl::IsSelectable(const size_t row) const
        {
        return row < m_rows.size() && m_rows[row].m_type != RowType::Header &&
               m_rows[row].m_enabled;
        }

    //---------------------------------------------------
    wxBitmapBundle TellMeCtrl::GetIcon(const wxString& path)
        {
        if (path.empty() || !m_iconResolver)
            {
            return {};
            }
        const auto iconPos = m_iconCache.find(path);
        if (iconPos != m_iconCache.cend())
            {
            return iconPos->second;
            }
        const auto [node, inserted] =
            m_iconCache.insert(std::make_pair(path, m_iconResolver(path)));
        return node->second;
        }

    //---------------------------------------------------
    void TellMeCtrl::AddItemRow(const TellMeMatch& match, wxWindow* frame)
        {
        const auto& item = GetRegistry()->GetItem(match.m_categoryIndex, match.m_itemIndex);
        ResultRow row;
        row.m_type = RowType::Item;
        row.m_text = item.m_name;
        row.m_description = item.m_description;
        row.m_itemId = item.m_id;
        row.m_icon = GetIcon(item.m_iconPath);
        row.m_highlights = match.m_highlights;
        row.m_enabled = GetRegistry()->IsEnabled(item.m_id, frame);
        m_rows.push_back(std::move(row));
        }

    //---------------------------------------------------
    void TellMeCtrl::RebuildRows()
        {
        m_rows.clear();
        const auto* registry = GetRegistry();
        if (registry == nullptr)
            {
            return;
            }
        auto* frame = wxGetTopLevelParent(this);

        const auto addRow = [this](const RowType type, const wxString& text,
                                   const wxBitmapBundle& icon = wxBitmapBundle{})
        {
            ResultRow row;
            row.m_type = type;
            row.m_text = text;
            row.m_icon = icon;
            m_rows.push_back(std::move(row));
        };

        const wxString query{ GetValue().Strip(wxString::both) };
        if (query.empty())
            {
            size_t itemCount{ 0 };
            for (const auto& id : registry->GetRecentIds())
                {
                if (itemCount >= MAX_RECENT_ITEMS)
                    {
                    break;
                    }
                const auto itemPos = registry->FindItem(id);
                if (!itemPos.has_value() || !registry->IsRunnable(id))
                    {
                    continue;
                    }
                if (itemCount == 0)
                    {
                    addRow(RowType::Header, _(L"Recently Used"));
                    }
                TellMeMatch match;
                match.m_categoryIndex = itemPos->first;
                match.m_itemIndex = itemPos->second;
                AddItemRow(match, frame);
                ++itemCount;
                }

            // previous searches, newest first
            std::vector<wxString> shownSearches;
            for (size_t i = m_recentSearches.size();
                 i > 0 && shownSearches.size() < MAX_RECENT_SEARCHES; --i)
                {
                const wxString& search{ m_recentSearches[i - 1] };
                if (search.empty() ||
                    std::ranges::find(shownSearches, search) != shownSearches.cend())
                    {
                    continue;
                    }
                if (shownSearches.empty())
                    {
                    addRow(RowType::Header, _(L"Recent Searches"));
                    }
                addRow(RowType::RecentSearch, search,
                       wxArtProvider::GetBitmapBundle(wxART_FIND, wxART_MENU));
                shownSearches.push_back(search);
                }
            return;
            }

        const auto results = registry->Search(query);
        if (results.m_bestMatch.has_value())
            {
            addRow(RowType::Header, _(L"Best Action"));
            AddItemRow(results.m_bestMatch.value(), frame);
            }
        for (const auto& [catIndex, group] : results.m_groups)
            {
            addRow(RowType::Header, registry->GetCategories().at(catIndex).m_name);
            for (const auto& match : group)
                {
                AddItemRow(match, frame);
                }
            }

        if (m_findRowEnabled)
            {
            addRow(RowType::Find, wxString::Format(_(L"Find “%s” in document"), query),
                   wxArtProvider::GetBitmapBundle(wxART_FIND, wxART_MENU));
            }
        if (m_rows.empty())
            {
            addRow(RowType::Header, _(L"No matching features"));
            }
        }

    //---------------------------------------------------
    void TellMeCtrl::MoveSelection(const int step)
        {
        const int rowCount{ static_cast<int>(m_rows.size()) };
        const int current{ m_list->GetSelection() };
        int candidate{ (current != wxNOT_FOUND) ? current : (step > 0 ? -1 : rowCount) };
        for (candidate += step; candidate >= 0 && candidate < rowCount; candidate += step)
            {
            if (IsSelectable(static_cast<size_t>(candidate)))
                {
                m_list->SetSelection(candidate);
                return;
                }
            }
        }

    //---------------------------------------------------
    void TellMeCtrl::RunRow(const size_t row)
        {
        if (!IsSelectable(row))
            {
            return;
            }
        const ResultRow selectedRow{ m_rows[row] };

        switch (selectedRow.m_type)
            {
        case RowType::Item:
            {
            ChangeValue(wxString{});
            HideResults();
            const wxString itemId{ selectedRow.m_itemId };
            CallAfter(
                [this, itemId]()
                {
                    if (auto* registry = GetRegistry(); registry != nullptr)
                        {
                        registry->Run(itemId, wxGetTopLevelParent(this));
                        }
                });
            break;
            }
        case RowType::Find:
            HideResults();
            SendSearchEvent();
            break;
        case RowType::RecentSearch:
            ChangeValue(selectedRow.m_text);
            SetInsertionPointEnd();
            if (m_findRowEnabled)
                {
                HideResults();
                SendSearchEvent();
                }
            else
                {
                SetFocus();
                ShowResults();
                }
            break;
        case RowType::Header:
            break;
            }
        }

    //---------------------------------------------------
    void TellMeCtrl::AddRecentSearch(const wxString& search)
        {
        if (search.empty())
            {
            return;
            }
        const int existing{ m_recentSearches.Index(search) };
        if (existing != wxNOT_FOUND)
            {
            m_recentSearches.RemoveAt(static_cast<size_t>(existing));
            }
        m_recentSearches.push_back(search);
        if (m_recentSearches.size() > MAX_SEARCH_HISTORY)
            {
            m_recentSearches.RemoveAt(0, m_recentSearches.size() - MAX_SEARCH_HISTORY);
            }
        }

    //---------------------------------------------------
    void TellMeCtrl::SendSearchEvent()
        {
        wxCommandEvent searchEvent{ wxEVT_SEARCH, GetId() };
        searchEvent.SetEventObject(this);
        searchEvent.SetString(GetValue());
        ProcessWindowEvent(searchEvent);
        }

    //---------------------------------------------------
    void TellMeCtrl::OnCharHook(wxKeyEvent& event)
        {
        const bool popupShown{ m_popup->IsShown() };
        switch (event.GetKeyCode())
            {
        case WXK_DOWN:
        case WXK_NUMPAD_DOWN:
            if (popupShown)
                {
                MoveSelection(1);
                }
            else
                {
                ShowResults();
                }
            return;
        case WXK_UP:
        case WXK_NUMPAD_UP:
            if (popupShown)
                {
                MoveSelection(-1);
                return;
                }
            break;
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER:
            if (popupShown && m_list->GetSelection() != wxNOT_FOUND)
                {
                RunRow(static_cast<size_t>(m_list->GetSelection()));
                return;
                }
            // nothing selected, so let the control send its search event
            HideResults();
            break;
        case WXK_ESCAPE:
            if (popupShown)
                {
                HideResults();
                return;
                }
            if (!GetValue().empty())
                {
                ChangeValue(wxString{});
                return;
                }
            break;
        default:
            break;
            }
        event.Skip();
        }

    //---------------------------------------------------
    void TellMeCtrl::OnFocusLost(wxFocusEvent& event)
        {
        CallAfter(
            [this]()
            {
                // a click in the dropdown may not move the focus into it
                const bool mouseInPopup{ m_popup->IsShown() &&
                                         m_popup->GetScreenRect().Contains(wxGetMousePosition()) };
                if (!HasFocusWithin() && !mouseInPopup)
                    {
                    HideResults();
                    }
            });
        event.Skip();
        }

    //---------------------------------------------------
    void TellMeCtrl::OnFrameChanged(wxEvent& event)
        {
        HideResults();
        event.Skip();
        }
    } // namespace Wisteria::UI
