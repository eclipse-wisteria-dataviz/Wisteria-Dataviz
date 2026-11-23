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

class wxXmlDocument;
class wxRibbonButtonBar;

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
        /// @brief The item's display name.
        wxString m_name;
        /// @brief Optional text shown under the display name (not searched).
        wxString m_description;
        /// @brief Other phrases that search for the item.
        std::vector<wxString> m_phrases;
        /// @brief Optional ribbon button whose availability the item follows
        ///     (e.g., the dropdown button of a menu command).
        wxString m_buttonId;
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
        /// @brief Index of the best-matching search phrase,
        ///     or @c std::nullopt if the display name matched best.
        std::optional<size_t> m_phraseIndex;
        /// @brief The match score (higher is better).
        int m_score{ 0 };
        /// @brief Matched (start, length) ranges in the item's display name,
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
        /// @brief Returns the command ID for a feature ID, or @c std::nullopt if there is none.
        using CommandResolver = std::function<std::optional<wxWindowID>(const wxString& id)>;

        /// @brief The maximum number of recently used feature IDs that are kept.
        constexpr static size_t MAX_RECENT_IDS{ 10 };

        /// @brief Connects a feature ID to an action.
        /// @param id The feature ID (from the XML file).
        /// @param run The action to run.
        /// @param enabled Optional check for whether the action can run.
        void RegisterAction(const wxString& id, TellMeCallback run,
                            TellMeEnabledCallback enabled = {});

        /// @brief Connects a feature ID to a command.
        /// @details If the frame's ribbon has a button for @c commandId, running the feature
        ///     clicks that button, and the button's state decides whether it is enabled.
        ///     The state is first updated from the frame's @c wxEVT_UPDATE_UI handlers.
        ///     A button that is only in hidden ribbon panels is disabled.\n
        ///     Otherwise, running the feature sends a @c wxEVT_MENU event to the frame,
        ///     and its enabled state comes from the frame's @c wxEVT_UPDATE_UI handlers.
        /// @param id The feature ID (from the XML file).
        /// @param commandId The command to send.
        void RegisterCommand(const wxString& id, wxWindowID commandId);

        /// @brief Sets the function that opens a feature's help topic.
        /// @param handler The handler.
        void SetHelpHandler(HelpCallback handler) { m_helpHandler = std::move(handler); }

        /// @brief Sets the function that maps feature IDs (and @c button attributes) to command
        /// IDs.
        /// @details Loaded features with no action or help topic are registered as commands.
        /// @param resolver The resolver (e.g., one that calls @c wxXmlResource::GetXRCID()).
        void SetCommandResolver(CommandResolver resolver)
            {
            m_commandResolver = std::move(resolver);
            }

        /// @returns @c true if a ribbon button in the frame is in a shown ribbon panel
        ///     and is enabled.
        /// @param frame The frame containing the ribbon.
        /// @param buttonId The button's ID.
        [[nodiscard]]
        static bool IsRibbonButtonAvailable(wxWindow* frame, wxWindowID buttonId);

        /// @brief Loads categories and features from a Tell Me XML stream.
        /// @details Loaded items are appended to any already loaded.
        /// @param stream The XML stream.
        /// @returns @c true if the XML was loaded.
        bool LoadXml(wxInputStream& stream);

        /// @brief Loads categories and features from a Tell Me XML file.
        /// @details Loaded items are appended to any already loaded.
        /// @param filePath The path to the XML file.
        /// @returns @c true if the XML was loaded.
        bool LoadXml(const wxString& filePath);

        /// @brief Removes all loaded categories and features, but keeps registered actions.
        void ClearItems()
            {
            m_categories.clear();
            m_itemLookup.clear();
            m_itemButtonIds.clear();
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

        /// @brief Where a ribbon button was found.
        struct RibbonButtonLocation
            {
            /// @brief @c true if any ribbon in the frame has the button.
            bool m_inRibbon{ false };
            /// @brief A bar with the button in a shown panel (preferring one where it is enabled).
            wxRibbonButtonBar* m_bar{ nullptr };
            };

        bool LoadDocument(const wxXmlDocument& doc);
        [[nodiscard]]
        static RibbonButtonLocation FindRibbonButton(wxWindow* frame, wxWindowID buttonId);
        static void FindButtonInRibbon(wxWindow* window, wxWindowID buttonId,
                                       RibbonButtonLocation& location);
        static bool UpdateRibbonButton(wxRibbonButtonBar* buttonBar, wxWindowID buttonId);
        [[nodiscard]]
        static bool IsInShownRibbonPanel(const wxWindow* buttonBar);
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
        // resolved button IDs of the loaded items, by feature ID
        std::map<wxString, wxWindowID> m_itemButtonIds;
        std::vector<wxString> m_recentIds;
        HelpCallback m_helpHandler;
        CommandResolver m_commandResolver;
        };
    } // namespace Wisteria::UI

/** @}*/

#endif // WISTERIA_TELL_ME_REGISTRY_H
