/** @addtogroup UI
    @brief User interface classes.
    @date 2005-2026
    @copyright Blake Madden
    @author Blake Madden
    @details This program is free software; you can redistribute it and/or modify
     it under the terms of the 3-Clause BSD License.

     SPDX-License-Identifier: BSD-3-Clause
@{*/

#ifndef WISTERIA_TELL_ME_REGISTRY_H
#define WISTERIA_TELL_ME_REGISTRY_H

#include <functional>
#include <map>
#include <optional>
#include <utility>
#include <vector>
#include <wx/stream.h>
#include <wx/wx.h>

namespace Wisteria::UI
    {
    /// @brief A feature loaded from a Tell Me XML file.
    struct TellMeItemInfo
        {
        /// @brief The key that connects the feature to its action.
        wxString m_id;
        /// @brief Path to the feature's icon (e.g., an SVG in the resource archive).
        wxString m_iconPath;
        /// @brief Help topic to open (used when no action is registered).
        wxString m_helpTopic;
        /// @brief Phrases that describe the item. The first one is its display name.
        std::vector<wxString> m_phrases;
        };

    /// @brief A named group of features.
    struct TellMeCategory
        {
        /// @brief The category's display name.
        wxString m_name;
        /// @brief The features in the category.
        std::vector<TellMeItemInfo> m_items;
        };

    /// @brief A feature that matched a search query.
    struct TellMeMatch
        {
        /// @brief Index of the feature's category.
        size_t m_categoryIndex{ 0 };
        /// @brief Index of the feature within its category.
        size_t m_itemIndex{ 0 };
        /// @brief Index of the best-matching phrase.
        size_t m_phraseIndex{ 0 };
        /// @brief The match score (higher is better).
        int m_score{ 0 };
        /// @brief Matched (start, length) ranges in the item's display name (its first phrase),
        ///     sorted and non-overlapping.
        std::vector<std::pair<size_t, size_t>> m_highlights;
        };

    /// @brief Search results, grouped the way Tell Me displays them.
    struct TellMeSearchResults
        {
        /// @brief The top-scoring match.
        std::optional<TellMeMatch> m_bestMatch;
        /// @brief The remaining matches, grouped by category (in category order).
        std::vector<std::pair<size_t, std::vector<TellMeMatch>>> m_groups;
        };

    /// @brief Maps feature IDs to actions and searches the phrases describing them.
    class TellMeRegistry
        {
      public:
        /// @brief Runs a feature for the given frame.
        using TellMeCallback = std::function<void(wxWindow* frame)>;
        /// @brief Returns whether a feature can run for the given frame.
        using TellMeEnabledCallback = std::function<bool(wxWindow* frame)>;
        /// @brief Opens a help topic, or searches help for a query.
        using HelpCallback = std::function<void(wxWindow* frame, const wxString& value)>;

        /// @brief The maximum number of recently used feature IDs that are kept.
        constexpr static size_t MAX_RECENT_IDS{ 10 };

        /// @brief Connects a feature ID to an action.
        /// @param id The feature ID (from the XML file).
        /// @param run The action to run.
        /// @param enabled Optional check for whether the action can run.
        void RegisterAction(const wxString& id, TellMeCallback run,
                            TellMeEnabledCallback enabled = {});

        /// @brief Connects a feature ID to a menu command.
        /// @details Running the feature sends a @c wxEVT_MENU event with @c commandId
        ///     to the frame. Its enabled state comes from the frame's @c wxEVT_UPDATE_UI handlers.
        /// @param id The feature ID (from the XML file).
        /// @param commandId The command to send.
        void RegisterCommand(const wxString& id, wxWindowID commandId);

        /// @brief Sets the function that opens a feature's help topic.
        /// @param handler The handler.
        void SetHelpHandler(HelpCallback handler) { m_helpHandler = std::move(handler); }

        /// @brief Loads categories and features from a Tell Me XML stream.
        /// @details Loaded items are appended to any already loaded.
        /// @param stream The XML stream.
        /// @returns @c true if the XML was loaded.
        bool LoadXml(wxInputStream& stream);

        /// @brief Removes all loaded categories and features.
        /// @note Registered actions are kept.
        void ClearItems()
            {
            m_categories.clear();
            m_itemLookup.clear();
            }

        /// @returns The loaded categories.
        [[nodiscard]]
        const std::vector<TellMeCategory>& GetCategories() const noexcept
            {
            return m_categories;
            }

        /// @brief Finds a loaded item.
        /// @param id The feature ID.
        /// @returns The (category, feature) indices, or @c std::nullopt if not found.
        [[nodiscard]]
        std::optional<std::pair<size_t, size_t>> FindItem(const wxString& id) const;

        /// @returns The feature at the given indices.
        /// @param categoryIndex The category index.
        /// @param itemIndex The feature index within the category.
        [[nodiscard]]
        const TellMeItemInfo& GetItem(const size_t categoryIndex, const size_t itemIndex) const
            {
            return m_categories.at(categoryIndex).m_items.at(itemIndex);
            }

        /// @returns @c true if the feature has an action or a help topic that can be opened.
        /// @param id The feature ID.
        [[nodiscard]]
        bool IsRunnable(const wxString& id) const;

        /// @returns @c true if the feature can run for the given frame.
        /// @param id The feature ID.
        /// @param frame The frame that would run the item.
        [[nodiscard]]
        bool IsEnabled(const wxString& id, wxWindow* frame) const;

        /// @brief Runs a feature and adds it to the recently used list.
        /// @param id The feature ID.
        /// @param frame The frame to run the feature for.
        /// @returns @c true if the feature ran.
        bool Run(const wxString& id, wxWindow* frame);

        /// @brief Searches the runnable features' phrases.
        /// @param query The text to search for.
        /// @param maxPerCategory The maximum number of matches per category.
        /// @param maxTotal The maximum number of matches overall (including the best match).
        /// @returns The best match and the other matches, grouped by category.
        [[nodiscard]]
        TellMeSearchResults Search(const wxString& query, size_t maxPerCategory = 5,
                                   size_t maxTotal = 15) const;

        /// @returns The recently used feature IDs, newest first.
        [[nodiscard]]
        const std::vector<wxString>& GetRecentIds() const noexcept
            {
            return m_recentIds;
            }

        /// @brief Sets the recently used feature IDs (e.g., when loading settings).
        /// @param ids The IDs, newest first.
        void SetRecentIds(std::vector<wxString> ids);

      private:
        struct TellMeAction
            {
            TellMeCallback m_run;
            TellMeEnabledCallback m_enabled;
            std::optional<wxWindowID> m_commandId;
            };

        [[nodiscard]]
        static std::vector<wxString> TokenizeQuery(const wxString& query);
        [[nodiscard]]
        static std::optional<int> ScorePhrase(const wxString& phrase,
                                              const std::vector<wxString>& tokens,
                                              const wxString& joinedQuery);
        [[nodiscard]]
        static std::vector<std::pair<size_t, size_t>>
        HighlightTokens(const wxString& phrase, const std::vector<wxString>& tokens);
        [[nodiscard]]
        static std::optional<std::pair<size_t, bool>> FindToken(const wxString& lowerPhrase,
                                                                const wxString& token);
        [[nodiscard]]
        static bool IsWordStart(const wxString& text, size_t position);
        void AddRecentId(const wxString& id);

        std::vector<TellMeCategory> m_categories;
        std::map<wxString, std::pair<size_t, size_t>> m_itemLookup;
        std::map<wxString, TellMeAction> m_actions;
        std::vector<wxString> m_recentIds;
        HelpCallback m_helpHandler;
        };
    } // namespace Wisteria::UI

/** @}*/

#endif // WISTERIA_TELL_ME_REGISTRY_H
