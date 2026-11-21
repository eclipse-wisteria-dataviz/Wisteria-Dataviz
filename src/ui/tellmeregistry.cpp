///////////////////////////////////////////////////////////////////////////////
// Name:        tellmeregistry.cpp
// Author:      Blake Madden
// Copyright:   (c) 2005-2026 Blake Madden
// License:     3-Clause BSD license
// SPDX-License-Identifier: BSD-3-Clause
///////////////////////////////////////////////////////////////////////////////

#include "tellmeregistry.h"
#include <algorithm>
#include <wx/filename.h>
#include <wx/tokenzr.h>
#include <wx/xml/xml.h>

namespace Wisteria::UI
    {
    //---------------------------------------------------
    void TellMeRegistry::RegisterAction(const wxString& id, TellMeCallback run,
                                        TellMeEnabledCallback enabled)
        {
        m_actions[id] = TellMeAction{ std::move(run), std::move(enabled), std::nullopt };
        }

    //---------------------------------------------------
    void TellMeRegistry::RegisterCommand(const wxString& id, const wxWindowID commandId)
        {
        m_actions[id] = TellMeAction{ [commandId](wxWindow* frame)
                                      {
                                          if (frame == nullptr)
                                              {
                                              return;
                                              }
                                          wxCommandEvent cmd{ wxEVT_MENU, commandId };
                                          cmd.SetEventObject(frame);
                                          frame->ProcessWindowEvent(cmd);
                                      },
                                      TellMeEnabledCallback{}, commandId };
        }

    //---------------------------------------------------
    bool TellMeRegistry::LoadXml(wxInputStream& stream)
        {
        wxXmlDocument doc;
        if (!stream.IsOk() || !doc.Load(stream))
            {
            wxLogWarning(L"Unable to load Tell Me XML.");
            return false;
            }
        return LoadDocument(doc);
        }

    //---------------------------------------------------
    bool TellMeRegistry::LoadXml(const wxString& filePath)
        {
        wxXmlDocument doc;
        if (!wxFileName::FileExists(filePath) || !doc.Load(filePath))
            {
            wxLogWarning(L"Unable to load Tell Me XML file '%s'.", filePath);
            return false;
            }
        return LoadDocument(doc);
        }

    //---------------------------------------------------
    bool TellMeRegistry::LoadDocument(const wxXmlDocument& doc)
        {
        const auto* root = doc.GetRoot();
        if (root == nullptr || root->GetName() != L"tellme")
            {
            wxLogWarning(L"Invalid Tell Me XML: root must be 'tellme'.");
            return false;
            }

        for (const auto* catNode = root->GetChildren(); catNode != nullptr;
             catNode = catNode->GetNext())
            {
            if (catNode->GetType() != wxXML_ELEMENT_NODE || catNode->GetName() != L"category")
                {
                continue;
                }
            const wxString catName{
                catNode->GetAttribute(L"name", wxString{}).Strip(wxString::both)
            };

            // merge with a category of the same name (e.g., from a previously loaded file)
            auto catPos = std::ranges::find_if(m_categories, [&catName](const auto& cat)
                                               { return cat.m_name == catName; });
            if (catPos == m_categories.end())
                {
                m_categories.push_back(TellMeCategory{ catName, {} });
                catPos = std::prev(m_categories.end());
                }
            const auto catIndex = static_cast<size_t>(std::distance(m_categories.begin(), catPos));

            for (const auto* itemNode = catNode->GetChildren(); itemNode != nullptr;
                 itemNode = itemNode->GetNext())
                {
                if (itemNode->GetType() != wxXML_ELEMENT_NODE || itemNode->GetName() != L"item")
                    {
                    continue;
                    }
                TellMeItemInfo item;
                item.m_id = itemNode->GetAttribute(L"id", wxString{}).Strip(wxString::both);
                item.m_iconPath = itemNode->GetAttribute(L"icon", wxString{}).Strip(wxString::both);
                item.m_helpTopic =
                    itemNode->GetAttribute(L"help", wxString{}).Strip(wxString::both);
                item.m_name = itemNode->GetAttribute(L"name", wxString{}).Strip(wxString::both);
                item.m_description =
                    itemNode->GetAttribute(L"description", wxString{}).Strip(wxString::both);

                wxStringTokenizer tokenizer{ itemNode->GetAttribute(L"phrases", wxString{}), L";",
                                             wxTOKEN_STRTOK };
                while (tokenizer.HasMoreTokens())
                    {
                    wxString phrase{ tokenizer.GetNextToken() };
                    phrase.Trim(true).Trim(false);
                    if (!phrase.empty())
                        {
                        item.m_phrases.push_back(phrase);
                        }
                    }

                if (item.m_id.empty() || item.m_name.empty())
                    {
                    wxLogDebug(L"Tell Me: skipping feature missing an ID or name.");
                    continue;
                    }
                if (m_itemLookup.contains(item.m_id))
                    {
                    wxLogDebug(L"Tell Me: duplicate feature ID '%s' skipped.", item.m_id);
                    continue;
                    }
                if (!m_actions.contains(item.m_id) && item.m_helpTopic.empty())
                    {
                    wxLogDebug(L"Tell Me: feature ID '%s' has no registered action or help topic.",
                               item.m_id);
                    }

                auto& items = m_categories[catIndex].m_items;
                m_itemLookup[item.m_id] = std::make_pair(catIndex, items.size());
                items.push_back(std::move(item));
                }
            }

        return true;
        }

    //---------------------------------------------------
    std::optional<std::pair<size_t, size_t>> TellMeRegistry::FindItem(const wxString& id) const
        {
        const auto foundPos = m_itemLookup.find(id);
        if (foundPos == m_itemLookup.cend())
            {
            return std::nullopt;
            }
        return foundPos->second;
        }

    //---------------------------------------------------
    bool TellMeRegistry::IsRunnable(const wxString& id) const
        {
        if (m_actions.contains(id))
            {
            return true;
            }
        const auto itemPos = FindItem(id);
        return itemPos.has_value() && static_cast<bool>(m_helpHandler) &&
               !GetItem(itemPos->first, itemPos->second).m_helpTopic.empty();
        }

    //---------------------------------------------------
    bool TellMeRegistry::IsEnabled(const wxString& id, wxWindow* frame) const
        {
        const auto actionPos = m_actions.find(id);
        if (actionPos == m_actions.cend())
            {
            // help topics are always available
            return IsRunnable(id);
            }

        const auto& action = actionPos->second;
        if (action.m_enabled)
            {
            return action.m_enabled(frame);
            }
        if (action.m_commandId.has_value() && frame != nullptr)
            {
            wxUpdateUIEvent updateEvent{ action.m_commandId.value() };
            updateEvent.SetEventObject(frame);
            frame->ProcessWindowEvent(updateEvent);
            return !updateEvent.GetSetEnabled() || updateEvent.GetEnabled();
            }
        return true;
        }

    //---------------------------------------------------
    bool TellMeRegistry::Run(const wxString& id, wxWindow* frame)
        {
        if (!IsRunnable(id) || !IsEnabled(id, frame))
            {
            return false;
            }

        AddRecentId(id);

        const auto actionPos = m_actions.find(id);
        if (actionPos != m_actions.cend())
            {
            actionPos->second.m_run(frame);
            return true;
            }

        const auto itemPos = FindItem(id);
        if (itemPos.has_value() && m_helpHandler)
            {
            m_helpHandler(frame, GetItem(itemPos->first, itemPos->second).m_helpTopic);
            return true;
            }
        return false;
        }

    //---------------------------------------------------
    TellMeSearchResults TellMeRegistry::Search(const wxString& query, const size_t maxPerCategory,
                                               const size_t maxTotal) const
        {
        TellMeSearchResults results;

        const auto tokens = TokenizeQuery(query);
        if (tokens.empty() || maxTotal == 0)
            {
            return results;
            }
        wxString joinedQuery;
        for (const auto& token : tokens)
            {
            if (!joinedQuery.empty())
                {
                joinedQuery += L' ';
                }
            joinedQuery += token;
            }

        // the display name, or the search phrase at the given index
        const auto matchedText = [](const TellMeItemInfo& item,
                                    const std::optional<size_t>& phraseIndex) -> const wxString&
        { return phraseIndex.has_value() ? item.m_phrases[phraseIndex.value()] : item.m_name; };

        // best-matching name or phrase for each runnable feature
        std::vector<TellMeMatch> matches;
        for (size_t catIndex = 0; catIndex < m_categories.size(); ++catIndex)
            {
            const auto& items = m_categories[catIndex].m_items;
            for (size_t itemIndex = 0; itemIndex < items.size(); ++itemIndex)
                {
                const auto& item = items[itemIndex];
                if (!IsRunnable(item.m_id))
                    {
                    continue;
                    }
                std::optional<TellMeMatch> bestPhraseMatch;
                const auto scoreText = [&](const std::optional<size_t>& phraseIndex)
                {
                    const wxString& text{ matchedText(item, phraseIndex) };
                    const auto score = ScorePhrase(text, tokens, joinedQuery);
                    if (!score.has_value())
                        {
                        return;
                        }
                    if (!bestPhraseMatch.has_value() || score.value() > bestPhraseMatch->m_score ||
                        (score.value() == bestPhraseMatch->m_score &&
                         text.length() <
                             matchedText(item, bestPhraseMatch->m_phraseIndex).length()))
                        {
                        TellMeMatch phraseMatch;
                        phraseMatch.m_categoryIndex = catIndex;
                        phraseMatch.m_itemIndex = itemIndex;
                        phraseMatch.m_phraseIndex = phraseIndex;
                        phraseMatch.m_score = score.value();
                        bestPhraseMatch = std::move(phraseMatch);
                        }
                };
                scoreText(std::nullopt);
                for (size_t phraseIndex = 0; phraseIndex < item.m_phrases.size(); ++phraseIndex)
                    {
                    scoreText(phraseIndex);
                    }
                if (bestPhraseMatch.has_value())
                    {
                    // the display name is shown, even if a search phrase matched
                    bestPhraseMatch->m_highlights = HighlightTokens(item.m_name, tokens);
                    matches.push_back(std::move(bestPhraseMatch.value()));
                    }
                }
            }

        if (matches.empty())
            {
            return results;
            }

        // rank by score, then by shorter phrase (stable, so ties keep XML order)
        const auto phraseLength = [this, &matchedText](const TellMeMatch& match)
        {
            return matchedText(GetItem(match.m_categoryIndex, match.m_itemIndex),
                               match.m_phraseIndex)
                .length();
        };
        std::ranges::stable_sort(matches,
                                 [&phraseLength](const TellMeMatch& lhs, const TellMeMatch& rhs)
                                 {
                                     if (lhs.m_score != rhs.m_score)
                                         {
                                         return lhs.m_score > rhs.m_score;
                                         }
                                     return phraseLength(lhs) < phraseLength(rhs);
                                 });

        results.m_bestMatch = matches.front();
        size_t totalCount{ 1 };

        std::map<size_t, std::vector<TellMeMatch>> groupedMatches;
        for (auto matchPos = std::next(matches.cbegin());
             matchPos != matches.cend() && totalCount < maxTotal; ++matchPos)
            {
            auto& group = groupedMatches[matchPos->m_categoryIndex];
            if (group.size() < maxPerCategory)
                {
                group.push_back(*matchPos);
                ++totalCount;
                }
            }
        for (auto& [catIndex, group] : groupedMatches)
            {
            if (!group.empty())
                {
                results.m_groups.emplace_back(catIndex, std::move(group));
                }
            }

        return results;
        }

    //---------------------------------------------------
    void TellMeRegistry::SetRecentIds(std::vector<wxString> ids)
        {
        m_recentIds.clear();
        for (auto& id : ids)
            {
            if (!id.empty() && std::ranges::find(m_recentIds, id) == m_recentIds.cend())
                {
                m_recentIds.push_back(std::move(id));
                }
            if (m_recentIds.size() >= MAX_RECENT_IDS)
                {
                break;
                }
            }
        }

    //---------------------------------------------------
    void TellMeRegistry::AddRecentId(const wxString& id)
        {
        std::erase(m_recentIds, id);
        m_recentIds.insert(m_recentIds.begin(), id);
        if (m_recentIds.size() > MAX_RECENT_IDS)
            {
            m_recentIds.resize(MAX_RECENT_IDS);
            }
        }

    //---------------------------------------------------
    std::vector<wxString> TellMeRegistry::TokenizeQuery(const wxString& query)
        {
        std::vector<wxString> tokens;
        wxStringTokenizer tokenizer{ query.Lower(), L" \t\r\n", wxTOKEN_STRTOK };
        while (tokenizer.HasMoreTokens())
            {
            tokens.push_back(tokenizer.GetNextToken());
            }
        return tokens;
        }

    //---------------------------------------------------
    bool TellMeRegistry::IsWordStart(const wxString& text, const size_t position)
        {
        return position == 0 || !wxIsalnum(text[position - 1]);
        }

    //---------------------------------------------------
    std::optional<std::pair<size_t, bool>> TellMeRegistry::FindToken(const wxString& lowerPhrase,
                                                                     const wxString& token)
        {
        // prefer an occurrence at the start of a word
        std::optional<size_t> firstFound;
        for (size_t foundPos = lowerPhrase.find(token); foundPos != wxString::npos;
             foundPos = lowerPhrase.find(token, foundPos + 1))
            {
            if (IsWordStart(lowerPhrase, foundPos))
                {
                return std::pair<size_t, bool>{ foundPos, true };
                }
            if (!firstFound.has_value())
                {
                firstFound = foundPos;
                }
            }
        if (firstFound.has_value())
            {
            return std::pair<size_t, bool>{ firstFound.value(), false };
            }
        return std::nullopt;
        }

    //---------------------------------------------------
    std::optional<int> TellMeRegistry::ScorePhrase(const wxString& phrase,
                                                   const std::vector<wxString>& tokens,
                                                   const wxString& joinedQuery)
        {
        const wxString lowerPhrase{ phrase.Lower() };

        bool allAtWordStarts{ true };
        for (const auto& token : tokens)
            {
            const auto found = FindToken(lowerPhrase, token);
            if (!found.has_value())
                {
                return std::nullopt;
                }
            if (!found->second)
                {
                allAtWordStarts = false;
                }
            }

        if (lowerPhrase == joinedQuery)
            {
            return 100;
            }
        if (lowerPhrase.find(joinedQuery) == 0)
            {
            return 80;
            }
        return allAtWordStarts ? 60 : 40;
        }

    //---------------------------------------------------
    std::vector<std::pair<size_t, size_t>>
    TellMeRegistry::HighlightTokens(const wxString& phrase, const std::vector<wxString>& tokens)
        {
        const wxString lowerPhrase{ phrase.Lower() };

        std::vector<std::pair<size_t, size_t>> ranges;
        for (const auto& token : tokens)
            {
            if (const auto found = FindToken(lowerPhrase, token); found.has_value())
                {
                ranges.emplace_back(found->first, token.length());
                }
            }

        // sort and merge overlapping ranges
        std::ranges::sort(ranges);
        std::vector<std::pair<size_t, size_t>> merged;
        for (const auto& [start, length] : ranges)
            {
            if (!merged.empty() && start <= merged.back().first + merged.back().second)
                {
                merged.back().second =
                    std::max(merged.back().first + merged.back().second, start + length) -
                    merged.back().first;
                }
            else
                {
                merged.emplace_back(start, length);
                }
            }
        return merged;
        }
    } // namespace Wisteria::UI
